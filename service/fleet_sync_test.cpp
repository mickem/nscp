// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// What the fleet sync makes of an answer: which pacer call, which log line and
// which follow-up call each status leads to. The sync is built without its
// thread and stepped by hand, with the fleet server and both clocks replaced
// (fleet_seams), so a rejection that is retried a minute later costs a
// millisecond here rather than a minute in the jest suites.
//
// The pacing rules themselves are tested with the pacers
// (libs/onboarding/sync_test.cpp, failure_context_pacer_test.cpp), and the
// real transport, the reload and the on-disk apply against a fake server in
// tests/fleet-sync*.test.ts. This file is the join between the two.

#include "fleet_sync.hpp"

#include <gtest/gtest.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#include <algorithm>
#include <boost/filesystem.hpp>
#include <boost/json.hpp>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = boost::filesystem;
namespace json = boost::json;

struct fleet_sync_test_access {
  static bool begin(fleet_sync &sync) { return sync.begin(); }
  static unsigned long turn(fleet_sync &sync, bool *at_least = nullptr) {
    const fleet_sync::poll_sleep sleep = sync.turn();
    if (at_least != nullptr) *at_least = sleep.at_least;
    return sleep.seconds;
  }
};

namespace {

class recording_logger : public nsclient::logging::logger {
 public:
  // "<level>: <message>", in the order logged.
  std::vector<std::string> lines;

  void trace(const std::string &, const char *, int, const std::string &message) override { lines.push_back("trace: " + message); }
  void debug(const std::string &, const char *, int, const std::string &message) override { lines.push_back("debug: " + message); }
  void info(const std::string &, const char *, int, const std::string &message) override { lines.push_back("info: " + message); }
  void warning(const std::string &, const char *, int, const std::string &message) override { lines.push_back("warning: " + message); }
  void error(const std::string &, const char *, int, const std::string &message) override { lines.push_back("error: " + message); }
  void critical(const std::string &, const char *, int, const std::string &message) override { lines.push_back("critical: " + message); }
  bool should_trace() const override { return true; }
  bool should_debug() const override { return true; }
  bool should_info() const override { return true; }
  bool should_warning() const override { return true; }
  bool should_error() const override { return true; }
  bool should_critical() const override { return true; }
  void raw(const std::string &) override {}
  void add_subscriber(nsclient::logging::logging_subscriber_instance) override {}
  nsclient::logging::unsubscribe_result close_subscriber(nsclient::logging::logging_subscriber_instance) override { return {}; }
  void reopen_subscriber(nsclient::logging::logging_subscriber_instance) override {}
  bool drop_subscriber(nsclient::logging::logging_subscriber_instance) override { return false; }
  nsclient::logging::unsubscribe_result remove_subscriber(nsclient::logging::logging_subscriber_instance) override { return {}; }
  std::vector<nsclient::logging::logging_subscriber_instance> clear_subscribers() override { return {}; }
  bool startup() override { return false; }
  bool shutdown() override { return false; }
  void destroy() override {}
  void configure() override {}
  void set_log_level(std::string) override {}
  std::string get_log_level() const override { return ""; }
  void set_backend(std::string) override {}

  // How many lines at `level` contain `needle`.
  int count(const std::string &level, const std::string &needle) const {
    return static_cast<int>(std::count_if(lines.begin(), lines.end(), [&](const std::string &line) {
      return line.compare(0, level.size() + 1, level + ":") == 0 && line.find(needle) != std::string::npos;
    }));
  }
  std::string all() const {
    std::string out;
    for (const std::string &line : lines) out += line + "\n";
    return out;
  }
};

// A minimal self-signed certificate expiring `days` from now: the sync checks
// the expiry of whatever the manifest holds on every turn.
std::string make_cert_expiring_in(const long days) {
  EVP_PKEY *raw_key = nullptr;
  EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, nullptr);
  EVP_PKEY_keygen_init(ctx);
  EVP_PKEY_keygen(ctx, &raw_key);
  EVP_PKEY_CTX_free(ctx);
  X509 *cert = X509_new();
  X509_set_version(cert, 2);
  ASN1_INTEGER_set(X509_get_serialNumber(cert), 1);
  const long seconds = days * 24 * 3600;
  X509_gmtime_adj(X509_getm_notBefore(cert), seconds < 0 ? seconds - 3600 : -3600);
  X509_gmtime_adj(X509_getm_notAfter(cert), seconds);
  X509_set_pubkey(cert, raw_key);
  X509_NAME *name = X509_get_subject_name(cert);
  X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, reinterpret_cast<const unsigned char *>("test"), -1, -1, 0);
  X509_set_issuer_name(cert, name);
  X509_sign(cert, raw_key, nullptr);
  BIO *bio = BIO_new(BIO_s_mem());
  PEM_write_bio_X509(bio, cert);
  char *data = nullptr;
  const long len = BIO_get_mem_data(bio, &data);
  const std::string pem(data, static_cast<std::size_t>(len));
  BIO_free(bio);
  X509_free(cert);
  EVP_PKEY_free(raw_key);
  return pem;
}

http::response answer(const unsigned int status, const std::string &body = "{}", const std::map<std::string, std::string> &headers = {}) {
  http::response response;
  response.status_code_ = status;
  response.payload_ = body;
  // Lower-case, as the http client stores them.
  response.headers_ = headers;
  return response;
}

struct seen_call {
  std::string verb;
  std::string path;
  std::string payload;
};

const char *const poll_path = "/agent/v1/desired-state";
const char *const report_path = "/agent/v1/state-report";
const char *const heartbeat_path = "/agent/v1/heartbeat";
const char *const facts_path = "/agent/v1/facts";
const char *const failure_path = "/agent/v1/failure-context";
const char *const renew_path = "/agent/v1/renew";

class FleetSyncTest : public ::testing::Test {
 protected:
  typedef std::function<http::response(const seen_call &)> route;

  fs::path dir;
  std::shared_ptr<recording_logger> log = std::make_shared<recording_logger>();
  nsclient::core::tag_repository_instance tags = std::make_shared<nsclient::core::tag_repository>();
  nsclient::core::fact_repository_instance facts = std::make_shared<nsclient::core::fact_repository>();
  nsclient::core::failed_check_store_instance failures = capturing_store();
  nsclient::core::metrics_snapshot_store snapshots;
  std::unique_ptr<fleet_sync> sync;
  fleet_config config;
  onboarding::enrolled_identity identity;

  std::vector<seen_call> calls;
  // Keyed by path without its query; a path with no route is answered 200 {}.
  std::map<std::string, route> routes;
  // What every poll answer carries, on top of the 304.
  std::map<std::string, std::string> poll_headers;
  int reloads = 0;

  std::int64_t wall = 1700000000;
  std::chrono::steady_clock::time_point steady = std::chrono::steady_clock::time_point() + std::chrono::hours(1000);

  void SetUp() override {
    dir = fs::temp_directory_path() / fs::unique_path("nscp-fleet-sync-test-%%%%-%%%%");
    fs::create_directories(dir);
    config.state_file = (dir / "agent-state.json").string();
    config.managed_path = (dir / "fleet").string();
    config.hostname = "host-1";
    config.nscp_version = "0.0.0";
    config.failure_context_interval = 600;

    identity.private_key_pem = "KEY";
    identity.cert_pem = make_cert_expiring_in(90);
    identity.ca_pem = "CA";
    identity.bundle_signing_pub_pem = "SIGNING";
    identity.server_url = "https://fleet.test";
    identity.mtls_url = "https://fleet.test";
    identity.mtls_server_cert_pem = "PIN";

    // Steady state: nothing new, poll again in a second.
    routes[poll_path] = [this](const seen_call &) { return answer(304, R"({"next_poll_in_seconds":1})", poll_headers); };
  }

  void TearDown() override {
    sync.reset();
    boost::system::error_code ignored;
    fs::remove_all(dir, ignored);
  }

  http::response serve(const char *verb, const std::string &path, const std::string &payload) {
    seen_call call{verb, path, payload};
    calls.push_back(call);
    const auto found = routes.find(path.substr(0, path.find('?')));
    return found == routes.end() ? answer(200) : found->second(call);
  }

  // A new sync over the same directory and repositories: a restart.
  void create() {
    fleet_seams seams;
    seams.start_thread = false;
    seams.transport = [this](const char *verb, const std::string &path, const std::string &payload, std::size_t) { return serve(verb, path, payload); };
    seams.wall_clock = [this] { return wall; };
    seams.steady_clock = [this] { return steady; };
    sync.reset(new fleet_sync(log, config, tags, facts, failures, [this] { ++reloads; }, seams));
  }

  // Enrolled and through its startup calls, which no case here is about.
  void start() {
    onboarding::save_state(identity, config.state_file);
    create();
    ASSERT_TRUE(fleet_sync_test_access::begin(*sync)) << log->all();
    calls.clear();
  }

  unsigned long turn(bool *at_least = nullptr) { return fleet_sync_test_access::turn(*sync, at_least); }
  void turns(const int count) {
    for (int i = 0; i < count; ++i) turn();
  }
  void advance(const long seconds) {
    wall += seconds;
    steady += std::chrono::seconds(seconds);
  }

  std::vector<seen_call> calls_to(const std::string &path) const {
    std::vector<seen_call> out;
    for (const seen_call &call : calls) {
      if (call.path.substr(0, call.path.find('?')) == path) out.push_back(call);
    }
    return out;
  }
  std::size_t count_to(const std::string &path) const { return calls_to(path).size(); }

  // A failure log that keeps the metrics snapshot with each failure: a report
  // only goes for a failure that has one.
  static nsclient::core::failed_check_store_instance capturing_store() {
    nsclient::core::failed_check_store_instance store = std::make_shared<nsclient::core::failed_check_store>();
    nsclient::core::failed_check_store::options options;
    options.capture_metrics = true;
    store->configure(options);
    return store;
  }

  // A check fails, and is recorded with a snapshot of the host - or, with
  // `with_snapshot` off, as it is before the first collection round.
  void fail_check(const std::string &command, const std::string &message, const bool with_snapshot = true) {
    PB::Commands::QueryResponseMessage::Response response;
    response.set_command(command);
    response.set_result(PB::Common::ResultCode::CRITICAL);
    response.add_lines()->set_message(message);
    nsclient::core::metrics_snapshot_ptr snapshot;
    if (with_snapshot) {
      PB::Metrics::MetricsMessage metrics;
      PB::Metrics::MetricsBundle *system = metrics.add_payload()->add_bundles();
      system->set_key("system");
      PB::Metrics::Metric *idle = system->add_value();
      idle->set_key("cpu.idle");
      idle->mutable_gauge_value()->set_value(3);
      snapshot = snapshots.store(metrics, wall);
    }
    ASSERT_TRUE(failures->record(response, nullptr, "test", 0, wall, snapshot));
  }

  // The server takes failure reports, and says so on every poll.
  void server_takes_failure_reports() { poll_headers[onboarding::failure_context_header] = "accept"; }

  // One fact set, so the host has a document other than the empty one.
  void have_facts(const std::string &value = "linux") {
    PB::Facts::Object set;
    PB::Facts::Field *field = set.add_fields();
    field->set_key("family");
    field->mutable_value()->set_string_value(value);
    std::string error;
    ASSERT_NE(facts->set("os", 1, set, error), nsclient::core::fact_repository::set_result::rejected) << error;
  }
  // The server says on every poll what facts hash it holds.
  void server_holds_facts(const std::string &hash) { poll_headers[onboarding::facts_hash_header] = hash; }
};

// --- starting ---------------------------------------------------------------

TEST_F(FleetSyncTest, does_not_start_without_an_enrollment) {
  create();
  EXPECT_FALSE(fleet_sync_test_access::begin(*sync));
  EXPECT_EQ(log->count("error", "No fleet enrollment found"), 1) << log->all();
  EXPECT_TRUE(calls.empty()) << "nothing may be sent by a host that is not enrolled";
}

TEST_F(FleetSyncTest, refuses_a_plaintext_management_url_nobody_allowed) {
  identity.mtls_url = "http://fleet.test";
  onboarding::save_state(identity, config.state_file);
  create();
  EXPECT_FALSE(fleet_sync_test_access::begin(*sync));
  EXPECT_EQ(log->count("error", "Fleet management url is not https"), 1) << log->all();
  EXPECT_TRUE(calls.empty());
}

TEST_F(FleetSyncTest, runs_over_plaintext_when_boot_ini_allows_it_and_says_so) {
  identity.mtls_url = "http://fleet.test/";
  config.allow_plaintext = true;
  onboarding::save_state(identity, config.state_file);
  create();
  EXPECT_TRUE(fleet_sync_test_access::begin(*sync));
  EXPECT_EQ(log->count("error", "INSECURE: the fleet management url is not https"), 1) << log->all();
}

TEST_F(FleetSyncTest, stops_when_the_heartbeat_says_the_certificate_is_revoked) {
  routes[heartbeat_path] = [](const seen_call &) { return answer(403); };
  onboarding::save_state(identity, config.state_file);
  create();
  EXPECT_FALSE(fleet_sync_test_access::begin(*sync));
  EXPECT_EQ(log->count("error", "rejected our certificate (revoked)"), 1) << log->all();
  EXPECT_EQ(count_to(report_path), 0u) << "a revoked host reports nothing";
}

TEST_F(FleetSyncTest, starts_with_a_heartbeat_and_a_state_report_carrying_the_tags) {
  tags->set("drives", "c:");
  onboarding::save_state(identity, config.state_file);
  create();
  ASSERT_TRUE(fleet_sync_test_access::begin(*sync)) << log->all();
  ASSERT_EQ(calls.size(), 2u);
  EXPECT_EQ(calls[0].path, heartbeat_path);
  EXPECT_EQ(calls[1].path, report_path);
  const json::object report = json::parse(calls[1].payload).as_object();
  const json::object &reported = report.at("reported_tags").as_object();
  EXPECT_EQ(reported.at("hostname").as_string(), "host-1");
  EXPECT_EQ(reported.at("drives").as_string(), "c:");
}

TEST_F(FleetSyncTest, a_heartbeat_that_cannot_reach_the_server_does_not_stop_the_sync) {
  routes[heartbeat_path] = [](const seen_call &) -> http::response { throw std::runtime_error("Failed to connect to fleet.test:443: connection refused"); };
  onboarding::save_state(identity, config.state_file);
  create();
  EXPECT_TRUE(fleet_sync_test_access::begin(*sync));
  EXPECT_EQ(log->count("error", "Fleet heartbeat failed"), 1) << log->all();
  // The report that follows gets through, and says the server is back.
  EXPECT_EQ(log->count("info", "connection restored after 1 failed attempt"), 1) << log->all();
}

TEST_F(FleetSyncTest, a_corrupt_applied_state_file_is_ignored_and_the_poll_asks_for_everything) {
  fs::create_directories(config.managed_path);
  std::ofstream((fs::path(config.managed_path) / "applied-state.json").string().c_str()) << "{ not json";
  start();
  EXPECT_EQ(log->count("error", "Ignoring corrupt applied-state file"), 1) << log->all();
  turn();
  ASSERT_EQ(count_to(poll_path), 1u);
  EXPECT_EQ(calls_to(poll_path)[0].path.find("current_hash=h"), std::string::npos) << calls_to(poll_path)[0].path;
}

// --- the poll ---------------------------------------------------------------

TEST_F(FleetSyncTest, a_304_sets_the_poll_interval_the_server_names) {
  routes[poll_path] = [](const seen_call &) { return answer(304, R"({"next_poll_in_seconds":45})"); };
  start();
  EXPECT_EQ(turn(), 45u);
}

TEST_F(FleetSyncTest, polls_at_the_configured_interval_until_the_server_names_one) {
  config.poll_interval = 300;
  int asked = 0;
  routes[poll_path] = [&](const seen_call &) {
    return asked > 0 ? answer(304, "{\"next_poll_in_seconds\":" + std::to_string(asked) + "}") : answer(304, "{}");
  };
  start();
  EXPECT_EQ(turn(), 300u) << "a 304 without a hint keeps the interval in force";
  asked = 20;
  EXPECT_EQ(turn(), 20u);
  asked = 0;
  EXPECT_EQ(turn(), 20u) << "and what the server named stays in force after it";
}

TEST_F(FleetSyncTest, holds_the_interval_the_server_asks_for_to_the_configured_bounds) {
  config.min_poll_interval = 30;
  config.max_poll_interval = 600;
  int asked = 1;
  routes[poll_path] = [&](const seen_call &) { return answer(304, "{\"next_poll_in_seconds\":" + std::to_string(asked) + "}"); };
  start();
  EXPECT_EQ(turn(), 30u) << "a shorter one is raised to the floor";
  asked = 120;
  EXPECT_EQ(turn(), 120u) << "one inside the bounds is the server's to choose";
  asked = 3600;
  EXPECT_EQ(turn(), 600u) << "a longer one is lowered to the ceiling";

  // The same bounds on the interval a new desired state names.
  routes[poll_path] = [](const seen_call &) { return answer(200, R"({"state_hash":"h-1","next_poll_in_seconds":5,"merged_config_json":{},"bundles":[]})"); };
  EXPECT_EQ(turn(), 30u) << log->all();
}

TEST_F(FleetSyncTest, the_bounds_apply_to_the_starting_interval_too) {
  config.poll_interval = 10;
  config.min_poll_interval = 30;
  routes[poll_path] = [](const seen_call &) { return answer(304, "{}"); };
  start();
  EXPECT_EQ(turn(), 30u);
}

TEST_F(FleetSyncTest, a_ceiling_below_the_floor_is_the_floor) {
  config.min_poll_interval = 120;
  config.max_poll_interval = 30;
  routes[poll_path] = [](const seen_call &) { return answer(304, R"({"next_poll_in_seconds":5})"); };
  start();
  EXPECT_EQ(turn(), 120u);
}

TEST_F(FleetSyncTest, the_bounds_do_not_shorten_a_wait_the_server_named_for_one_call) {
  config.max_poll_interval = 60;
  routes[poll_path] = [](const seen_call &) { return answer(429, "slow down", {{"retry-after", "900"}}); };
  start();
  EXPECT_EQ(turn(), 900u) << "a Retry-After is not a poll interval";
}

TEST_F(FleetSyncTest, a_failed_poll_backs_off_from_the_interval_in_force) {
  config.min_poll_interval = 100;
  routes[poll_path] = [](const seen_call &) { return answer(500, "boom"); };
  start();
  EXPECT_EQ(turn(), 200u);
  EXPECT_EQ(turn(), 400u);
  EXPECT_EQ(turn(), 500u) << "capped at five intervals";
}

TEST_F(FleetSyncTest, a_rate_limited_poll_sleeps_the_retry_after_and_is_not_a_failure) {
  routes[poll_path] = [](const seen_call &) { return answer(429, "slow down", {{"retry-after", "120"}}); };
  start();
  bool at_least = false;
  EXPECT_EQ(turn(&at_least), 120u);
  EXPECT_TRUE(at_least) << "a wait the server named is a minimum: jittered upwards only";
  EXPECT_EQ(log->count("info", "Desired-state poll rate limited"), 1);
  EXPECT_EQ(log->count("error", "Desired-state poll failed"), 0) << log->all();
}

TEST_F(FleetSyncTest, a_rate_limited_poll_without_a_retry_after_waits_one_interval) {
  routes[poll_path] = [](const seen_call &) { return answer(429); };
  start();
  bool at_least = true;
  EXPECT_EQ(turn(&at_least), 60u) << "the default interval, until a body names another";
  EXPECT_FALSE(at_least);
}

TEST_F(FleetSyncTest, a_failing_poll_backs_off_and_takes_the_retry_after_as_a_floor) {
  int status = 500;
  std::map<std::string, std::string> headers;
  routes[poll_path] = [&](const seen_call &) { return answer(status, "boom", headers); };
  start();
  // The default interval is 60s: doubled per failure, capped at five times it.
  EXPECT_EQ(turn(), 120u);
  EXPECT_EQ(turn(), 240u);
  EXPECT_EQ(turn(), 300u);
  EXPECT_EQ(turn(), 300u);
  EXPECT_EQ(log->count("error", "Desired-state poll failed: 500 boom"), 4);

  // A maintenance page's Retry-After: 1 does not shorten the backoff ...
  status = 503;
  headers["retry-after"] = "1";
  bool at_least = false;
  EXPECT_EQ(turn(&at_least), 300u);
  EXPECT_TRUE(at_least);
  // ... and a longer one is the wait.
  headers["retry-after"] = "900";
  EXPECT_EQ(turn(), 900u);
  // An absurd one is clamped to a day.
  headers["retry-after"] = "999999999";
  EXPECT_EQ(turn(), 86400u);
}

TEST_F(FleetSyncTest, a_poll_that_cannot_reach_the_server_sends_nothing_else_that_turn) {
  have_facts();
  server_holds_facts("none");
  bool down = true;
  const route up = routes[poll_path];
  routes[poll_path] = [&, up](const seen_call &call) -> http::response {
    if (down) throw std::runtime_error("Failed to connect to fleet.test:443: connection refused");
    return up(call);
  };
  start();
  turns(3);
  EXPECT_EQ(calls.size(), 3u) << "three polls and nothing piled on top of them";
  // The first failure is an error with advice; the repeats are not.
  EXPECT_EQ(log->count("error", "Desired-state poll failed"), 1) << log->all();

  down = false;
  turn();
  EXPECT_EQ(log->count("info", "connection restored after 3 failed attempt"), 1) << log->all();
  EXPECT_EQ(count_to(facts_path), 1u) << "and the upload the poll asked for goes with it";
}

TEST_F(FleetSyncTest, a_repeating_transport_failure_is_one_error_and_a_reminder_every_tenth) {
  routes[poll_path] = [](const seen_call &) -> http::response { throw std::runtime_error("Failed to connect to fleet.test:443: connection refused"); };
  start();
  turns(20);
  EXPECT_EQ(log->count("error", "Desired-state poll failed"), 1);
  EXPECT_EQ(log->count("info", "Fleet server still unreachable"), 2) << log->all();
}

TEST_F(FleetSyncTest, a_rejected_identity_is_blamed_on_the_expired_certificate_when_it_has_expired) {
  identity.cert_pem = make_cert_expiring_in(-3);
  routes[poll_path] = [](const seen_call &) -> http::response {
    throw std::runtime_error("Failed to connect to fleet.test:443: tlsv13 alert certificate required");
  };
  start();
  turn();
  EXPECT_EQ(log->count("error", "client certificate has expired"), 1) << log->all();
}

TEST_F(FleetSyncTest, a_desired_state_that_cannot_be_parsed_is_reported_without_a_hash) {
  routes[poll_path] = [](const seen_call &) { return answer(200, "<html>not json</html>"); };
  start();
  turn();
  EXPECT_EQ(log->count("error", "Failed to process desired state"), 1) << log->all();
  ASSERT_EQ(count_to(report_path), 1u);
  const json::object report = json::parse(calls_to(report_path)[0].payload).as_object();
  EXPECT_FALSE(report.at("errors").as_array().empty());
  EXPECT_EQ(reloads, 0);
}

TEST_F(FleetSyncTest, a_state_report_the_server_rejects_is_sent_again_when_the_tags_move) {
  int status = 500;
  routes[report_path] = [&](const seen_call &) { return answer(status, "no"); };
  start();
  // The startup report was rejected, so the tags still count as unreported.
  EXPECT_EQ(log->count("error", "Failed to submit state report: 500 no"), 1) << log->all();
  turn();
  EXPECT_EQ(count_to(report_path), 1u);
  status = 200;
  turn();
  EXPECT_EQ(count_to(report_path), 2u);
  // Reported: quiet until a module changes a tag.
  turns(3);
  EXPECT_EQ(count_to(report_path), 2u);
  tags->set("sqlserver", "detected");
  turn();
  EXPECT_EQ(count_to(report_path), 3u);
}

// --- certificate renewal ----------------------------------------------------

TEST_F(FleetSyncTest, leaves_a_certificate_with_time_left_alone) {
  start();
  turns(3);
  EXPECT_EQ(count_to(renew_path), 0u);
}

TEST_F(FleetSyncTest, keeps_the_identity_when_the_server_refuses_the_renewal) {
  identity.cert_pem = make_cert_expiring_in(5);
  routes[renew_path] = [](const seen_call &) { return answer(500, "ca offline"); };
  start();
  turn();
  ASSERT_EQ(count_to(renew_path), 1u);
  EXPECT_NE(calls_to(renew_path)[0].payload.find("csr_pem"), std::string::npos);
  EXPECT_EQ(log->count("error", "Certificate renewal failed: 500 ca offline"), 1) << log->all();
  // Still the certificate we had, and tried again on the next turn.
  const boost::optional<onboarding::enrolled_identity> stored = onboarding::load_state(config.state_file);
  ASSERT_TRUE(stored.is_initialized());
  EXPECT_EQ(stored.value().cert_pem, identity.cert_pem);
  turn();
  EXPECT_EQ(count_to(renew_path), 2u);
}

TEST_F(FleetSyncTest, keeps_the_identity_when_the_renewal_answer_is_not_a_certificate) {
  identity.cert_pem = make_cert_expiring_in(5);
  routes[renew_path] = [](const seen_call &) { return answer(200, "{}"); };
  start();
  turn();
  EXPECT_EQ(log->count("error", "Certificate renewal failed"), 1) << log->all();
  EXPECT_EQ(log->count("info", "Client certificate renewed"), 0);
  EXPECT_EQ(onboarding::load_state(config.state_file).value().cert_pem, identity.cert_pem);
}

TEST_F(FleetSyncTest, a_renewal_that_cannot_reach_the_server_holds_the_rest_of_the_turn) {
  identity.cert_pem = make_cert_expiring_in(5);
  have_facts();
  server_holds_facts("none");
  routes[renew_path] = [](const seen_call &) -> http::response { throw std::runtime_error("connection reset"); };
  start();
  calls.clear();
  turn();
  EXPECT_EQ(log->count("error", "Certificate renewal failed"), 1) << log->all();
  EXPECT_EQ(count_to(facts_path), 0u) << "no megabyte at a server the renewal just failed to reach";
}

TEST_F(FleetSyncTest, says_so_when_the_certificate_cannot_be_read) {
  identity.cert_pem = "not a certificate";
  start();
  turn();
  EXPECT_EQ(log->count("error", "Cannot check certificate expiry"), 1) << log->all();
  EXPECT_EQ(count_to(renew_path), 0u);
}

// --- facts ------------------------------------------------------------------

TEST_F(FleetSyncTest, uploads_the_facts_only_when_the_server_reports_a_miss) {
  have_facts();
  start();
  // A server that says nothing about facts is sent nothing.
  turns(3);
  EXPECT_EQ(count_to(facts_path), 0u);

  const std::string ours = facts->get_hash();
  ASSERT_FALSE(ours.empty());
  routes[facts_path] = [&](const seen_call &) { return answer(200, "{}", {{onboarding::facts_hash_header, ours}}); };
  server_holds_facts("none");
  turn();
  ASSERT_EQ(count_to(facts_path), 1u);
  const json::object upload = json::parse(calls_to(facts_path)[0].payload).as_object();
  EXPECT_EQ(upload.at("facts_hash").as_string(), ours.c_str());
  EXPECT_EQ(upload.at("facts").as_object().at("os").as_object().at("family").as_string(), "linux");
  EXPECT_NE(calls_to(poll_path).back().path.find("facts_hash=" + ours), std::string::npos) << "the poll says what we hold";

  // The server holds it now, and nothing more is sent.
  server_holds_facts(ours);
  turns(5);
  EXPECT_EQ(count_to(facts_path), 1u);
}

TEST_F(FleetSyncTest, gives_up_on_a_facts_document_refused_as_too_large_and_names_the_sets) {
  have_facts();
  server_holds_facts("none");
  routes[facts_path] = [](const seen_call &) { return answer(413, "too big"); };
  start();
  turns(5);
  EXPECT_EQ(count_to(facts_path), 1u) << "it would be too large every time";
  EXPECT_EQ(log->count("error", "refused the facts document as too large"), 1) << log->all();
  EXPECT_EQ(log->count("error", "Largest sets, as JSON: os ("), 1) << log->all();

  // Not an hour later either ...
  advance(3600);
  turns(3);
  EXPECT_EQ(count_to(facts_path), 1u);
  // ... but a different document is a different question.
  have_facts("windows");
  turn();
  EXPECT_EQ(count_to(facts_path), 2u);
}

TEST_F(FleetSyncTest, paces_a_rejected_facts_upload_and_tries_it_again_a_step_later) {
  have_facts();
  start();
  routes[facts_path] = [](const seen_call &) { return answer(404, "no route"); };
  server_holds_facts("none");
  turns(5);
  EXPECT_EQ(count_to(facts_path), 1u) << "not on every poll";
  EXPECT_EQ(log->count("error", "does not accept facts uploads (404)"), 1) << log->all();

  advance(61);
  turns(3);
  EXPECT_EQ(count_to(facts_path), 2u) << "one more at the next step";
  // The same document rejected the same way is not news twice.
  EXPECT_EQ(log->count("error", "does not accept facts uploads (404)"), 1) << log->all();
  EXPECT_EQ(log->count("debug", "does not accept facts uploads (404)"), 1) << log->all();
}

TEST_F(FleetSyncTest, a_rate_limited_facts_upload_holds_every_upload_for_its_retry_after) {
  have_facts();
  start();
  routes[facts_path] = [](const seen_call &) { return answer(503, "busy", {{"retry-after", "600"}}); };
  server_holds_facts("none");
  turn();
  ASSERT_EQ(count_to(facts_path), 1u);
  EXPECT_EQ(log->count("error", "Facts upload failed: 503 busy"), 1) << log->all();

  // Past the document's own first step (a minute), still inside the hold.
  advance(300);
  turns(3);
  EXPECT_EQ(count_to(facts_path), 1u);
  advance(301);
  turns(2);
  EXPECT_EQ(count_to(facts_path), 2u);
}

TEST_F(FleetSyncTest, a_facts_upload_the_network_cuts_is_paced_without_touching_the_connection_state) {
  have_facts();
  start();
  routes[facts_path] = [](const seen_call &) -> http::response { throw std::runtime_error("connection reset by peer"); };
  server_holds_facts("none");
  turns(5);
  EXPECT_EQ(count_to(facts_path), 1u);
  EXPECT_EQ(log->count("error", "Facts upload failed: connection reset by peer"), 1) << log->all();
  EXPECT_EQ(log->count("info", "connection restored"), 0) << "the polls never failed";
}

TEST_F(FleetSyncTest, an_unreadable_facts_header_is_no_answer_and_is_said_once) {
  have_facts();
  start();
  server_holds_facts("not-a-digest");
  turns(4);
  EXPECT_EQ(count_to(facts_path), 0u);
  EXPECT_EQ(log->count("error", "unreadable X-Facts-Hash header (not-a-digest)"), 1) << log->all();
}

TEST_F(FleetSyncTest, an_error_page_on_the_poll_says_nothing_about_facts_whatever_it_carries) {
  have_facts();
  start();
  routes[poll_path] = [](const seen_call &) { return answer(502, "bad gateway", {{onboarding::facts_hash_header, "none"}}); };
  turns(3);
  EXPECT_EQ(count_to(facts_path), 0u);
}

TEST_F(FleetSyncTest, says_once_when_the_server_hashes_the_document_its_own_way) {
  have_facts();
  start();
  routes[facts_path] = [](const seen_call &) { return answer(200, "{}", {{onboarding::facts_hash_header, std::string(64, 'e')}}); };
  server_holds_facts("none");
  turns(5);
  EXPECT_EQ(count_to(facts_path), 1u);
  EXPECT_EQ(log->count("error", "answered with a different hash"), 1) << log->all();
}

// --- failure reports --------------------------------------------------------

TEST_F(FleetSyncTest, reports_a_failed_check_only_to_a_server_that_asks) {
  start();
  fail_check("check_cpu", "cpu high");
  turns(3);
  EXPECT_EQ(count_to(failure_path), 0u);

  server_takes_failure_reports();
  turn();
  ASSERT_EQ(count_to(failure_path), 1u);
  const json::object report = json::parse(calls_to(failure_path)[0].payload).as_object();
  EXPECT_EQ(report.at("failure").as_object().at("command").as_string(), "check_cpu");
  EXPECT_EQ(report.at("version").as_int64(), 1);
  EXPECT_EQ(report.at("report_id").as_string().size(), 26u);
  EXPECT_EQ(report.at("sent_at_unix").as_int64(), wall);
  EXPECT_EQ(report.at("context").as_object().at("state").as_string(), "captured");
  EXPECT_EQ(report.at("context").as_object().at("captured_at_unix").as_int64(), wall);

  // One per interval (ten minutes here), whatever fails in between.
  fail_check("check_memory", "mem high");
  advance(599);
  turns(3);
  EXPECT_EQ(count_to(failure_path), 1u);
  advance(2);
  turn();
  ASSERT_EQ(count_to(failure_path), 2u);
  // A new report, so a new id.
  EXPECT_NE(json::parse(calls_to(failure_path)[1].payload).as_object().at("report_id").as_string(), report.at("report_id").as_string());
}

TEST_F(FleetSyncTest, sends_no_failure_report_when_capture_is_off_and_says_so_once) {
  failures = std::make_shared<nsclient::core::failed_check_store>();
  server_takes_failure_reports();
  start();
  fail_check("check_cpu", "cpu high", false);
  turns(3);
  // Without a snapshot a report says nothing the monitoring system that ran
  // the check does not already know.
  EXPECT_EQ(count_to(failure_path), 0u);
  EXPECT_EQ(log->count("info", "Not reporting failed checks to the fleet server"), 1) << log->all();

  fail_check("check_memory", "mem high", false);
  advance(700);
  turns(3);
  EXPECT_EQ(count_to(failure_path), 0u);
  EXPECT_EQ(log->count("info", "Not reporting failed checks to the fleet server"), 1) << "still once";
}

TEST_F(FleetSyncTest, a_failure_without_a_snapshot_does_not_use_up_the_interval) {
  server_takes_failure_reports();
  start();
  // Recorded before the first collection round: capture is on, and there was
  // nothing to keep yet.
  fail_check("check_cpu", "cpu high", false);
  turns(2);
  EXPECT_EQ(count_to(failure_path), 0u);
  EXPECT_EQ(log->count("info", "Not reporting failed checks"), 0) << "capture is on: nothing for the operator to change";

  // The next failure has one, and goes at once. The earlier one was set
  // aside, not dropped: it is counted among the others, exactly as it would
  // have been had both failed before the same turn.
  fail_check("check_memory", "mem high");
  turn();
  ASSERT_EQ(count_to(failure_path), 1u);
  const json::object report = json::parse(calls_to(failure_path)[0].payload).as_object();
  EXPECT_EQ(report.at("failure").as_object().at("command").as_string(), "check_memory");
  const json::object &suppressed = report.at("suppressed").as_object();
  EXPECT_EQ(suppressed.at("count").as_int64(), 1);
  EXPECT_EQ(suppressed.at("by_check").as_array().at(0).as_object().at("command").as_string(), "check_cpu");

  // Reported now, so not counted a second time.
  fail_check("check_disk", "disk full");
  advance(700);
  turn();
  ASSERT_EQ(count_to(failure_path), 2u);
  EXPECT_EQ(json::parse(calls_to(failure_path)[1].payload).as_object().at("suppressed").as_object().at("count").as_int64(), 0);
}

TEST_F(FleetSyncTest, failures_without_a_snapshot_are_listed_once_not_every_turn) {
  server_takes_failure_reports();
  start();
  fail_check("check_cpu", "cpu high", false);
  turns(5);
  EXPECT_EQ(count_to(failure_path), 0u);
  // Set aside on the first turn, and not looked at again until something
  // newer fails.
  EXPECT_EQ(log->count("debug", "no snapshot is held for it"), 1) << log->all();
}

TEST_F(FleetSyncTest, a_failure_with_a_snapshot_is_reported_ahead_of_newer_ones_without) {
  server_takes_failure_reports();
  start();
  fail_check("check_cpu", "cpu high");
  fail_check("check_memory", "mem high", false);
  turn();
  ASSERT_EQ(count_to(failure_path), 1u);
  const json::object report = json::parse(calls_to(failure_path)[0].payload).as_object();
  EXPECT_EQ(report.at("failure").as_object().at("command").as_string(), "check_cpu");
  EXPECT_EQ(report.at("suppressed").as_object().at("count").as_int64(), 1);
}

TEST_F(FleetSyncTest, sends_no_failure_reports_when_they_are_switched_off) {
  config.failure_context = false;
  server_takes_failure_reports();
  start();
  fail_check("check_cpu", "cpu high");
  turns(3);
  EXPECT_EQ(count_to(failure_path), 0u);
}

TEST_F(FleetSyncTest, an_unreadable_failure_context_header_is_said_once_and_sends_nothing) {
  poll_headers[onboarding::failure_context_header] = "maybe";
  start();
  fail_check("check_cpu", "cpu high");
  turns(4);
  EXPECT_EQ(count_to(failure_path), 0u);
  EXPECT_EQ(log->count("info", "X-Failure-Context header this agent does not understand (maybe)"), 1) << log->all();
}

TEST_F(FleetSyncTest, keeps_the_last_report_time_across_a_restart) {
  server_takes_failure_reports();
  start();
  fail_check("check_cpu", "cpu high");
  turn();
  ASSERT_EQ(count_to(failure_path), 1u);

  // A restart: a new sync and a failure log that starts over.
  failures = capturing_store();
  start();
  fail_check("check_cpu", "cpu high again");
  advance(300);
  turns(3);
  EXPECT_EQ(count_to(failure_path), 0u) << "inside the interval the previous process opened";
  advance(301);
  turn();
  EXPECT_EQ(count_to(failure_path), 1u);
}

TEST_F(FleetSyncTest, gives_up_on_a_failure_report_refused_as_too_large) {
  server_takes_failure_reports();
  int status = 413;
  routes[failure_path] = [&](const seen_call &) { return answer(status, "too big"); };
  start();
  fail_check("check_cpu", "cpu high");
  turn();
  EXPECT_EQ(count_to(failure_path), 1u);
  EXPECT_EQ(log->count("error", "refused a failure report as too large"), 1) << log->all();
  advance(7200);
  turns(3);
  EXPECT_EQ(count_to(failure_path), 1u) << "it would be too large every time";

  // The next failure is reported at once, and on its own.
  status = 202;
  fail_check("check_memory", "mem high");
  turn();
  ASSERT_EQ(count_to(failure_path), 2u);
  const json::object report = json::parse(calls_to(failure_path)[1].payload).as_object();
  EXPECT_EQ(report.at("failure").as_object().at("command").as_string(), "check_memory");
  EXPECT_EQ(report.at("suppressed").as_object().at("count").as_int64(), 0);
}

TEST_F(FleetSyncTest, retries_a_rejected_failure_report_a_minute_later_and_logs_a_status_once) {
  server_takes_failure_reports();
  int status = 500;
  routes[failure_path] = [&](const seen_call &) { return answer(status, "boom"); };
  start();
  fail_check("check_cpu", "cpu high");
  turns(5);
  EXPECT_EQ(count_to(failure_path), 1u);
  EXPECT_EQ(log->count("error", "Failure report to the fleet server failed: 500 boom"), 1) << log->all();

  advance(59);
  turns(2);
  EXPECT_EQ(count_to(failure_path), 1u);
  advance(2);
  turn();
  ASSERT_EQ(count_to(failure_path), 2u);
  // The same report again, under the same id: had the first one landed and
  // only its answer been lost, the server can tell.
  const auto id_of = [this](const std::size_t i) { return std::string(json::parse(calls_to(failure_path)[i].payload).as_object().at("report_id").as_string()); };
  EXPECT_EQ(id_of(0), id_of(1));
  EXPECT_EQ(log->count("error", "Failure report to the fleet server failed: 500 boom"), 1) << "the same status again is not news";
  EXPECT_EQ(log->count("debug", "Failure report to the fleet server failed: 500 boom"), 1);

  // A different status is. The second step is two minutes.
  status = 502;
  advance(121);
  turn();
  EXPECT_EQ(count_to(failure_path), 3u);
  EXPECT_EQ(log->count("error", "Failure report to the fleet server failed: 502 boom"), 1) << log->all();

  // And once it lands, the failure is not sent again.
  status = 202;
  advance(241);
  turn();
  ASSERT_EQ(count_to(failure_path), 4u);
  EXPECT_EQ(id_of(0), id_of(3));
  advance(7200);
  turns(3);
  EXPECT_EQ(count_to(failure_path), 4u);

  // What fails after it landed is another report.
  fail_check("check_memory", "mem high");
  turn();
  ASSERT_EQ(count_to(failure_path), 5u);
  EXPECT_NE(id_of(0), id_of(4));
}

TEST_F(FleetSyncTest, treats_a_missing_failure_route_as_a_server_that_does_not_ask) {
  server_takes_failure_reports();
  routes[failure_path] = [](const seen_call &) { return answer(404, "not found"); };
  start();
  fail_check("check_cpu", "cpu high");
  turns(5);
  EXPECT_EQ(count_to(failure_path), 1u) << "every poll since advertised reports again, and none brought it back";
  EXPECT_EQ(log->count("info", "advertises failure reports but has no /agent/v1/failure-context route (404)"), 1) << log->all();
  EXPECT_EQ(log->count("error", "Failure report to the fleet server failed"), 0) << log->all();
}

TEST_F(FleetSyncTest, a_rate_limited_failure_report_waits_out_its_retry_after) {
  server_takes_failure_reports();
  int status = 503;
  routes[failure_path] = [&](const seen_call &) { return answer(status, "busy", {{"retry-after", "900"}}); };
  start();
  fail_check("check_cpu", "cpu high");
  turn();
  ASSERT_EQ(count_to(failure_path), 1u);

  // Past the first retry step (a minute), still inside the server's wait.
  status = 202;
  advance(600);
  turns(3);
  EXPECT_EQ(count_to(failure_path), 1u);
  advance(301);
  turn();
  EXPECT_EQ(count_to(failure_path), 2u);
}

TEST_F(FleetSyncTest, a_failure_report_the_network_cuts_is_paced_like_a_rejection) {
  server_takes_failure_reports();
  bool cut = true;
  routes[failure_path] = [&](const seen_call &) -> http::response {
    if (cut) throw std::runtime_error("connection reset by peer");
    return answer(202);
  };
  start();
  fail_check("check_cpu", "cpu high");
  turns(5);
  EXPECT_EQ(count_to(failure_path), 1u);
  EXPECT_EQ(log->count("error", "Failure report to the fleet server failed: connection reset by peer"), 1) << log->all();
  EXPECT_EQ(log->count("info", "connection restored"), 0) << "the polls never failed";

  advance(61);
  turn();
  EXPECT_EQ(count_to(failure_path), 2u);
  EXPECT_EQ(log->count("debug", "Failure report to the fleet server failed: connection reset by peer"), 1);

  cut = false;
  advance(121);
  turn();
  EXPECT_EQ(count_to(failure_path), 3u);
}

TEST_F(FleetSyncTest, sends_no_failure_report_in_a_turn_whose_poll_was_turned_away) {
  server_takes_failure_reports();
  start();
  turn();  // the server asks
  fail_check("check_cpu", "cpu high");
  const route ok = routes[poll_path];
  routes[poll_path] = [](const seen_call &) { return answer(502, "bad gateway"); };
  turns(2);
  EXPECT_EQ(count_to(failure_path), 0u);
  routes[poll_path] = ok;
  turn();
  EXPECT_EQ(count_to(failure_path), 1u);
}

TEST_F(FleetSyncTest, drops_failures_that_were_cleared_before_they_could_be_reported) {
  start();
  fail_check("check_cpu", "cpu high");
  failures->clear();
  server_takes_failure_reports();
  turns(3);
  EXPECT_EQ(count_to(failure_path), 0u);
}

}  // namespace
