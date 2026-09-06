// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Tests for the Icinga 2 client's connection settings and its submissions.
//
// icinga.cpp (the JSON body and the response parsing) already has a suite;
// this covers icinga_client.hpp, which had none: connection_data, which turns
// a target's settings into an API endpoint, and submit(), which posts a check
// result to /v1/actions/process-check-result and turns the answer back into a
// submit response.
//
// The request is captured from a loopback HTTP server, so what is asserted is
// the API call Icinga would receive - the path, the credentials and the body.

#include <client/command_line_parser.hpp>

#include "icinga_client.hpp"

// icinga_target_object.hpp relies on its includer's po alias (IcingaClient.cpp
// declares it before pulling the header in); do the same here.
namespace po = boost::program_options;
#include "icinga_target_object.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include <cstdlib>
#include <future>
#include <map>
#include <string>
#include <thread>
#include <vector>

nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

namespace {

using boost::asio::ip::tcp;

client::destination_container target_with(const std::map<std::string, std::string> &options) {
  client::destination_container d;
  for (const auto &o : options) d.set_string_data(o.first, o.second);
  return d;
}

// Accepts one connection, keeps the request, and answers with a caller-chosen
// status and body.
class loopback_icinga_api {
 public:
  explicit loopback_icinga_api(std::string response_body = R"({"results":[{"code":200,"status":"Successfully processed check result"}]})",
                               const int status = 200)
      : body_to_send_(std::move(response_body)), status_(status) {
    std::promise<unsigned short> p;
    std::future<unsigned short> f = p.get_future();
    thread_ = std::thread([this, prom = std::move(p)]() mutable {
      try {
        boost::asio::io_context io;
        tcp::acceptor acceptor(io, {tcp::v4(), 0});
        prom.set_value(acceptor.local_endpoint().port());
        tcp::socket socket(io);
        acceptor.accept(socket);

        boost::asio::streambuf buffer;
        boost::system::error_code ec;
        boost::asio::read_until(socket, buffer, "\r\n\r\n", ec);
        const std::string received((std::istreambuf_iterator<char>(&buffer)), std::istreambuf_iterator<char>());
        const std::string::size_type split = received.find("\r\n\r\n");
        headers_ = split == std::string::npos ? received : received.substr(0, split);
        std::string body = split == std::string::npos ? std::string() : received.substr(split + 4);

        std::size_t expected = 0;
        const std::string::size_type cl = headers_.find("Content-Length:");
        if (cl != std::string::npos) expected = std::strtoul(headers_.c_str() + cl + 15, nullptr, 10);
        while (body.size() < expected && !ec) {
          char chunk[4096];
          const std::size_t n = socket.read_some(boost::asio::buffer(chunk), ec);
          if (!ec) body.append(chunk, n);
        }
        request_body_ = body;

        const std::string response = "HTTP/1.1 " + std::to_string(status_) + " x\r\nContent-Type: application/json\r\nContent-Length: " +
                                     std::to_string(body_to_send_.size()) + "\r\n\r\n" + body_to_send_;
        boost::asio::write(socket, boost::asio::buffer(response), ec);
      } catch (...) {
      }
    });
    port_ = f.get();
  }

  ~loopback_icinga_api() {
    if (thread_.joinable()) thread_.join();
  }

  unsigned short port() const { return port_; }
  std::string request_body() {
    if (thread_.joinable()) thread_.join();
    return request_body_;
  }
  std::string headers() {
    if (thread_.joinable()) thread_.join();
    return headers_;
  }

 private:
  std::string body_to_send_;
  int status_;
  unsigned short port_ = 0;
  std::string headers_;
  std::string request_body_;
  std::thread thread_;
};

// Submit one result, and hand back the response payload so the caller can
// check how the API's answer was interpreted.
PB::Commands::SubmitResponseMessage submit_one(loopback_icinga_api &api, const std::string &alias, const PB::Common::ResultCode result,
                                               const std::string &message, std::map<std::string, std::string> extra = {}) {
  PB::Commands::SubmitRequestMessage request;
  PB::Commands::QueryResponseMessage::Response *payload = request.add_payload();
  payload->set_command("check_something");
  if (!alias.empty()) payload->set_alias(alias);
  payload->set_result(result);
  payload->add_lines()->set_message(message);

  extra["address"] = "http://127.0.0.1:" + std::to_string(api.port());
  if (extra.find("username") == extra.end()) extra["username"] = "root";
  if (extra.find("password") == extra.end()) extra["password"] = "icinga";

  client::destination_container sender;
  sender.set_host("monitored-host");

  PB::Commands::SubmitResponseMessage response;
  icinga_client::icinga_client_handler handler;
  handler.submit(sender, target_with(extra), request, response);
  return response;
}

}  // namespace

// ---------------------------------------------------------------------------
// connection_data
// ---------------------------------------------------------------------------

TEST(IcingaConnectionData, DefaultsToHttpsOnTheApiPort) {
  const icinga_client::connection_data con(target_with({{"address", "icinga.example.com"}}), client::destination_container());

  EXPECT_EQ(con.protocol, "https") << "Icinga 2's API is https-only in practice";
  EXPECT_EQ(con.get_port(), "5665");
}

TEST(IcingaConnectionData, PlainHttpIsHonouredForATestSetup) {
  const icinga_client::connection_data con(target_with({{"address", "http://icinga.example.com"}}), client::destination_container());

  EXPECT_EQ(con.protocol, "http");
  EXPECT_EQ(con.get_port(), "5665");
}

TEST(IcingaConnectionData, AnExplicitPortWins) {
  const icinga_client::connection_data con(target_with({{"address", "https://icinga.example.com:8443"}}), client::destination_container());

  EXPECT_EQ(con.get_port(), "8443");
}

TEST(IcingaConnectionData, NoBasePathLeavesTheApiPathsAlone) {
  const icinga_client::connection_data con(target_with({{"address", "https://icinga.example.com:5665/"}}), client::destination_container());

  EXPECT_EQ(con.base_path, "");
  EXPECT_EQ(con.api_path("/v1/actions/process-check-result"), "/v1/actions/process-check-result");
}

TEST(IcingaConnectionData, ABasePathIsPrependedToTheApiPaths) {
  // An Icinga 2 master behind a reverse proxy subpath: the base path was
  // parsed off the address but dropped when building requests, so every call
  // went to the proxy root instead of the master.
  const icinga_client::connection_data con(target_with({{"address", "https://proxy.example.com/icinga/"}}), client::destination_container());

  EXPECT_EQ(con.base_path, "/icinga");
  EXPECT_EQ(con.api_path("/v1/actions/process-check-result"), "/icinga/v1/actions/process-check-result");
  EXPECT_EQ(con.api_path("/v1/objects/hosts/srv1"), "/icinga/v1/objects/hosts/srv1");
}

TEST(IcingaConnectionData, CarriesTheApiCredentials) {
  const icinga_client::connection_data con(target_with({{"address", "https://h"}, {"username", "root"}, {"password", "s3cret"}}),
                                           client::destination_container());

  EXPECT_EQ(con.username, "root");
  EXPECT_EQ(con.password, "s3cret");
}

TEST(IcingaConnectionData, TlsVersionDefaultsToOneThree) {
  const icinga_client::connection_data con(target_with({{"address", "https://h"}}), client::destination_container());

  EXPECT_EQ(con.tls_version, "1.3");
}

TEST(IcingaConnectionData, TheTimeoutReachesTheHttpClient) {
  // The http client's timeout_seconds_ defaults to 0 = wait forever, so if the
  // target's timeout is not copied across, a stalled Icinga endpoint wedges
  // the submitting thread indefinitely. Note that destination_container routes
  // "timeout" into its typed field, not the data map - reading it with
  // get_int_data() silently yields the fallback instead of the setting.
  const icinga_client::connection_data con(target_with({{"address", "https://h"}, {"timeout", "7"}}), client::destination_container());

  EXPECT_EQ(con.timeout, 7u);
  EXPECT_EQ(icinga_client::make_client_options(con).timeout_seconds_, 7u);
}

TEST(IcingaConnectionData, ADefaultConstructedTargetStillGetsAFiniteTimeout) {
  // Whatever the default ends up being (the configured target object seeds
  // 30), it must be non-zero: 0 means wait forever on the http client.
  const icinga_client::connection_data con(target_with({{"address", "https://h"}}), client::destination_container());

  EXPECT_GT(con.timeout, 0u);
  EXPECT_EQ(icinga_client::make_client_options(con).timeout_seconds_, con.timeout);
}

TEST(IcingaConnectionData, ObjectTemplatesAndEnsureFlagAreRead) {
  const icinga_client::connection_data con(target_with({{"address", "https://h"},
                                                        {"ensure_objects", "true"},
                                                        {"host_template", "generic-host"},
                                                        {"service_template", "generic-service"},
                                                        {"check_command", "dummy"}}),
                                           client::destination_container());

  EXPECT_TRUE(con.ensure_objects);
  EXPECT_EQ(con.host_template, "generic-host");
  EXPECT_EQ(con.service_template, "generic-service");
  EXPECT_EQ(con.check_command, "dummy");
}

TEST(IcingaConnectionData, TheSenderNamesTheHostAndTheCheckSource) {
  client::destination_container sender;
  sender.set_host("monitored-host");

  const icinga_client::connection_data con(target_with({{"address", "https://h"}}), sender);

  EXPECT_EQ(con.sender_hostname, "monitored-host");
  EXPECT_EQ(con.check_source, "monitored-host") << "check_source defaults to whoever is reporting";
}

TEST(IcingaConnectionData, AnExplicitCheckSourceIsKept) {
  client::destination_container sender;
  sender.set_host("monitored-host");

  const icinga_client::connection_data con(target_with({{"address", "https://h"}, {"check_source", "nscp-agent"}}), sender);

  EXPECT_EQ(con.check_source, "nscp-agent");
}

TEST(IcingaConnectionData, WithoutASenderTheLocalHostNameIsUsed) {
  // submit_icinga from the CLI has no sender, and Icinga rejects a check
  // result with an empty host, so something has to fill it in.
  const icinga_client::connection_data con(target_with({{"address", "https://h"}}), client::destination_container());

  EXPECT_FALSE(con.sender_hostname.empty());
  EXPECT_EQ(con.sender_hostname, boost::asio::ip::host_name());
}

// ---------------------------------------------------------------------------
// submit - the API call Icinga receives.
// ---------------------------------------------------------------------------

TEST(IcingaSubmit, PostsAProcessCheckResultAction) {
  loopback_icinga_api api;

  submit_one(api, "disk", PB::Common::ResultCode::WARNING, "running low");

  EXPECT_NE(api.headers().find("POST /v1/actions/process-check-result"), std::string::npos) << api.headers();
  // Icinga only answers json, and the API requires the client to say so.
  EXPECT_NE(api.headers().find("Accept: application/json"), std::string::npos) << api.headers();
}

TEST(IcingaSubmit, AuthenticatesWithTheConfiguredCredentials) {
  loopback_icinga_api api;

  submit_one(api, "disk", PB::Common::ResultCode::OK, "fine");

  // root:icinga
  EXPECT_NE(api.headers().find("Authorization: Basic cm9vdDppY2luZ2E="), std::string::npos) << api.headers();
}

TEST(IcingaSubmit, AServiceResultFiltersOnHostAndService) {
  loopback_icinga_api api;

  submit_one(api, "disk", PB::Common::ResultCode::WARNING, "running low");

  const std::string body = api.request_body();
  EXPECT_NE(body.find("\"type\":\"Service\""), std::string::npos) << body;
  EXPECT_NE(body.find("host.name==\\\"monitored-host\\\""), std::string::npos) << body;
  EXPECT_NE(body.find("service.name==\\\"disk\\\""), std::string::npos) << body;
  EXPECT_NE(body.find("\"exit_status\":1"), std::string::npos) << body;
}

TEST(IcingaSubmit, TheHostCheckAliasFiltersOnTheHostAlone) {
  loopback_icinga_api api;

  submit_one(api, "host_check", PB::Common::ResultCode::CRITICAL, "host is down");

  const std::string body = api.request_body();
  EXPECT_NE(body.find("\"type\":\"Host\""), std::string::npos) << body;
  EXPECT_EQ(body.find("service.name"), std::string::npos) << body;
  // A host is UP or DOWN: CRITICAL maps onto DOWN (1), not the service's 2.
  EXPECT_NE(body.find("\"exit_status\":1"), std::string::npos) << body;
}

TEST(IcingaSubmit, PerfdataIsSplitOutOfThePluginOutput) {
  loopback_icinga_api api;

  submit_one(api, "disk", PB::Common::ResultCode::OK, "all good|'used'=42%;80;90");

  const std::string body = api.request_body();
  EXPECT_NE(body.find("\"performance_data\""), std::string::npos) << body;
  EXPECT_NE(body.find("'used'=42%;80;90"), std::string::npos) << body;
  // The perfdata must not be left in the human-readable output as well.
  EXPECT_NE(body.find("\"plugin_output\":\"all good\""), std::string::npos) << body;
}

TEST(IcingaSubmit, ASuccessfulSubmissionIsReportedAsGood) {
  loopback_icinga_api api;

  const PB::Commands::SubmitResponseMessage response = submit_one(api, "disk", PB::Common::ResultCode::OK, "fine");

  ASSERT_EQ(response.payload_size(), 1);
  EXPECT_EQ(response.payload(0).result().code(), PB::Common::Result_StatusCodeType_STATUS_OK);
  EXPECT_NE(response.payload(0).result().message().find("Successfully processed"), std::string::npos) << response.payload(0).result().message();
}

TEST(IcingaSubmit, AnApiErrorIsReportedRatherThanSwallowed) {
  // A submission that Icinga rejected must not look like a delivery.
  loopback_icinga_api api(R"({"results":[{"code":404,"status":"No objects found."}]})", 404);

  const PB::Commands::SubmitResponseMessage response = submit_one(api, "disk", PB::Common::ResultCode::OK, "fine");

  ASSERT_EQ(response.payload_size(), 1);
  EXPECT_EQ(response.payload(0).result().code(), PB::Common::Result_StatusCodeType_STATUS_ERROR);
  EXPECT_NE(response.payload(0).result().message().find("No objects found"), std::string::npos) << response.payload(0).result().message();
}

TEST(IcingaSubmit, AnUnreachableApiIsReportedAsAnError) {
  // Nothing is listening on a port the kernel has just handed back.
  boost::asio::io_context io;
  tcp::acceptor probe(io, {tcp::v4(), 0});
  const unsigned short closed = probe.local_endpoint().port();
  probe.close();

  PB::Commands::SubmitRequestMessage request;
  PB::Commands::QueryResponseMessage::Response *payload = request.add_payload();
  payload->set_command("check_something");
  payload->set_result(PB::Common::ResultCode::OK);
  payload->add_lines()->set_message("fine");

  PB::Commands::SubmitResponseMessage response;
  icinga_client::icinga_client_handler handler;
  handler.submit(client::destination_container(), target_with({{"address", "http://127.0.0.1:" + std::to_string(closed)}}), request, response);

  ASSERT_EQ(response.payload_size(), 1);
  EXPECT_EQ(response.payload(0).result().code(), PB::Common::Result_StatusCodeType_STATUS_ERROR);
}

TEST(IcingaSubmit, AStalledApiTimesOutInsteadOfHangingForever) {
  // A server that accepts the connection and then never answers. Before the
  // timeout was wired through, this hung the submitting thread forever.
  std::promise<unsigned short> p;
  std::future<unsigned short> f = p.get_future();
  std::thread server([prom = std::move(p)]() mutable {
    try {
      boost::asio::io_context io;
      tcp::acceptor acceptor(io, {tcp::v4(), 0});
      prom.set_value(acceptor.local_endpoint().port());
      tcp::socket socket(io);
      acceptor.accept(socket);
      // Never answer: drain until the client gives up and closes, at which
      // point the read reports EOF and the thread exits.
      boost::system::error_code ec;
      char buf[4096];
      while (!ec) socket.read_some(boost::asio::buffer(buf), ec);
    } catch (...) {
    }
  });
  const unsigned short port = f.get();

  PB::Commands::SubmitRequestMessage request;
  PB::Commands::QueryResponseMessage::Response *payload = request.add_payload();
  payload->set_command("check_something");
  payload->set_result(PB::Common::ResultCode::OK);
  payload->add_lines()->set_message("fine");

  PB::Commands::SubmitResponseMessage response;
  icinga_client::icinga_client_handler handler;
  handler.submit(client::destination_container(), target_with({{"address", "http://127.0.0.1:" + std::to_string(port)}, {"timeout", "1"}}), request, response);
  server.join();

  ASSERT_EQ(response.payload_size(), 1);
  EXPECT_EQ(response.payload(0).result().code(), PB::Common::Result_StatusCodeType_STATUS_ERROR);
  EXPECT_NE(response.payload(0).result().message().find("timed out"), std::string::npos) << response.payload(0).result().message();
}

// ---------------------------------------------------------------------------
// icinga_target_object - the settings a `[/settings/icinga/client/targets/x]`
// section is read into, and the command line options offered by submit_icinga.
// ---------------------------------------------------------------------------

namespace {

// A settings backend backed by a plain map, so read() can be driven without a
// settings core (mirrors MockSettingsInterface in nscapi's helper_test).
class fake_settings : public nscapi::settings_helper::settings_impl_interface {
 public:
  std::map<std::string, std::map<std::string, std::string>> values;
  std::vector<std::string> registered_keys;
  std::string expand_prefix;

  void register_path(std::string, std::string, std::string, bool, bool) override {}
  void register_key(std::string, std::string key, std::string, std::string, std::string, std::string, bool, bool, bool) override {
    registered_keys.push_back(key);
  }
  void register_subkey(std::string, std::string, std::string, bool, bool) override {}
  void register_tpl(std::string, std::string, std::string, std::string, std::string) override {}
  std::string get_string(std::string path, std::string key, std::string def) override {
    const auto pit = values.find(path);
    if (pit == values.end()) return def;
    const auto kit = pit->second.find(key);
    return kit == pit->second.end() ? def : kit->second;
  }
  void set_string(std::string path, std::string key, std::string value) override { values[path][key] = value; }
  string_list get_sections(std::string) override { return {}; }
  string_list get_keys(std::string) override { return {}; }
  std::string expand_path(std::string key) override { return expand_prefix + key; }
  void remove_key(std::string, std::string) override {}
  void remove_path(std::string) override {}
  void err(const char *, int, std::string) override {}
  void warn(const char *, int, std::string) override {}
  void info(const char *, int, std::string) override {}
  void debug(const char *, int, std::string) override {}

  bool registered(const std::string &key) const {
    return std::find(registered_keys.begin(), registered_keys.end(), key) != registered_keys.end();
  }
};

// The section every `[/settings/icinga/client/targets]`-defined target lives
// under; the object appends its alias, so "default" reads from kTargetPath.
const std::string kTargetsPath = "/settings/icinga/client/targets";
const std::string kTargetPath = kTargetsPath + "/default";

}  // namespace

TEST(IcingaTargetObject, ANewTargetGetsTheThirtySecondTimeout) {
  icinga_handler::icinga_target_object target("default", kTargetsPath);

  EXPECT_EQ(target.get_property_int("timeout", 0), 30);
}

TEST(IcingaTargetObject, ReadPicksUpTheConfiguredApiSettings) {
  auto settings = std::make_shared<fake_settings>();
  settings->values[kTargetPath] = {{"username", "api-user"},        {"password", "s3cret"},
                                   {"ensure objects", "true"},      {"host template", "my-host-tpl"},
                                   {"service template", "my-svc-tpl"}, {"check command", "passive"},
                                   {"check source", "nscp-agent"},  {"tls version", "1.2"},
                                   {"verify mode", "none"},         {"ca", "/etc/ssl/ca.pem"}};

  icinga_handler::icinga_target_object target("default", kTargetsPath);
  target.read(settings, false, false);

  EXPECT_EQ(target.get_property_string("username"), "api-user");
  EXPECT_EQ(target.get_property_string("password"), "s3cret");
  EXPECT_TRUE(target.get_property_bool("ensure_objects", false));
  EXPECT_EQ(target.get_property_string("host_template"), "my-host-tpl");
  EXPECT_EQ(target.get_property_string("service_template"), "my-svc-tpl");
  EXPECT_EQ(target.get_property_string("check_command"), "passive");
  EXPECT_EQ(target.get_property_string("check_source"), "nscp-agent");
  EXPECT_EQ(target.get_property_string("tls version"), "1.2");
  EXPECT_EQ(target.get_property_string("verify mode"), "none");
  EXPECT_EQ(target.get_property_string("ca"), "/etc/ssl/ca.pem");
}

TEST(IcingaTargetObject, ReadFallsBackToTheTlsDefaults) {
  // Nothing configured: the TLS keys have defaults (1.3, peer, ${ca-path})
  // while the credential keys have none and must stay unset.
  auto settings = std::make_shared<fake_settings>();
  settings->expand_prefix = "expanded:";

  icinga_handler::icinga_target_object target("default", kTargetsPath);
  target.read(settings, false, false);

  EXPECT_EQ(target.get_property_string("tls version"), "1.3");
  EXPECT_EQ(target.get_property_string("verify mode"), "peer");
  // `ca` is a path key: the ${ca-path} placeholder must go through the
  // settings layer's expand_path, not reach the SSL stack verbatim.
  EXPECT_EQ(target.get_property_string("ca"), "expanded:${ca-path}");
  EXPECT_EQ(target.get_property_string("username"), "");
  EXPECT_EQ(target.get_property_string("password"), "");
}

TEST(IcingaTargetObject, ReadRegistersTheApiKeysForDocumentation) {
  auto settings = std::make_shared<fake_settings>();

  icinga_handler::icinga_target_object target("default", kTargetsPath);
  target.read(settings, false, false);

  for (const std::string key : {"username", "password", "ensure objects", "host template", "service template", "check command", "check source",
                                "tls version", "verify mode", "ca"}) {
    EXPECT_TRUE(settings->registered(key)) << "key not registered: " << key;
  }
}

TEST(IcingaTargetObject, AOnelinerReadStopsAfterTheSharedTargetKeys) {
  auto settings = std::make_shared<fake_settings>();
  settings->values[kTargetPath] = {{"username", "api-user"}};

  icinga_handler::icinga_target_object target("default", kTargetsPath);
  target.read(settings, true, false);

  // The parent's keys are read (timeout has a default of 30)...
  EXPECT_EQ(target.get_property_int("timeout", 0), 30);
  // ...but none of the Icinga specific keys are registered or read.
  EXPECT_FALSE(settings->registered("username"));
  EXPECT_EQ(target.get_property_string("username"), "");
}

TEST(IcingaTargetObject, ASampleReadStillRegistersTheKeys) {
  auto settings = std::make_shared<fake_settings>();

  icinga_handler::icinga_target_object target("default", kTargetsPath);
  target.read(settings, false, true);

  EXPECT_TRUE(settings->registered("username"));
}

TEST(IcingaOptionsReader, CreateAndCloneProduceTargetObjects) {
  icinga_handler::options_reader_impl reader;

  const nscapi::settings_objects::object_instance created = reader.create("default", kTargetsPath);
  ASSERT_TRUE(created);
  EXPECT_EQ(created->get_alias(), "default");
  EXPECT_EQ(created->get_property_int("timeout", 0), 30) << "create() must go through the defaulting constructor";

  created->set_property_string("username", "api-user");
  const nscapi::settings_objects::object_instance cloned = reader.clone(created, "copy", kTargetsPath);
  ASSERT_TRUE(cloned);
  EXPECT_EQ(cloned->get_alias(), "copy");
  EXPECT_EQ(cloned->get_property_string("username"), "api-user") << "clone() must inherit the parent's settings";
}

TEST(IcingaOptionsReader, ProcessParsesTheSubmitIcingaOptions) {
  icinga_handler::options_reader_impl reader;
  boost::program_options::options_description desc;
  client::destination_container source, data;
  reader.process(desc, source, data);

  const std::vector<std::string> argv = {"--username", "api-user",   "--password",         "s3cret",     "--hostname",     "sender-host",
                                         "--ensure-objects",         "--host-template",    "ht",         "--service-template", "st",
                                         "--check-command", "cc",    "--check-source",     "cs",         "--tls-version",  "1.2",
                                         "--verify-mode",   "none",  "--ca",               "/etc/ca.pem"};
  boost::program_options::variables_map vm;
  boost::program_options::store(boost::program_options::command_line_parser(argv).options(desc).run(), vm);
  boost::program_options::notify(vm);

  EXPECT_EQ(data.get_string_data("username"), "api-user");
  EXPECT_EQ(data.get_string_data("password"), "s3cret");
  EXPECT_EQ(data.get_string_data("ensure_objects"), "true") << "--ensure-objects without a value must mean true";
  EXPECT_EQ(data.get_string_data("host_template"), "ht");
  EXPECT_EQ(data.get_string_data("service_template"), "st");
  EXPECT_EQ(data.get_string_data("check_command"), "cc");
  EXPECT_EQ(data.get_string_data("check_source"), "cs");
  EXPECT_EQ(data.get_string_data("tls version"), "1.2");
  EXPECT_EQ(data.get_string_data("verify mode"), "none");
  EXPECT_EQ(data.get_string_data("ca"), "/etc/ca.pem");
  EXPECT_EQ(source.address.host, "sender-host") << "--hostname names the sender, not the API";
}

TEST(IcingaOptionsReader, TheTlsVersionDefaultsToOneThree) {
  icinga_handler::options_reader_impl reader;
  boost::program_options::options_description desc;
  client::destination_container source, data;
  reader.process(desc, source, data);

  boost::program_options::variables_map vm;
  boost::program_options::store(boost::program_options::command_line_parser(std::vector<std::string>{}).options(desc).run(), vm);
  boost::program_options::notify(vm);

  EXPECT_EQ(data.get_string_data("tls version"), "1.3");
  EXPECT_FALSE(data.has_data("ensure_objects")) << "object creation must stay opt-in";
}

TEST(IcingaSubmit, QueryAndExecAreNotSupported) {
  icinga_client::icinga_client_handler handler;
  const client::destination_container empty;

  PB::Commands::QueryRequestMessage query_request;
  PB::Commands::QueryResponseMessage query_response;
  EXPECT_FALSE(handler.query(empty, empty, query_request, query_response));

  PB::Commands::ExecuteRequestMessage exec_request;
  PB::Commands::ExecuteResponseMessage exec_response;
  EXPECT_FALSE(handler.exec(empty, empty, exec_request, exec_response));

  PB::Metrics::MetricsMessage metrics;
  EXPECT_FALSE(handler.metrics(empty, empty, metrics));
}
