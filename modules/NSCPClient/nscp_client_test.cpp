// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Tests for the NSCP (agent-to-agent) client's connection settings.
//
// This is the client that talks to another NSClient++ over its own protocol,
// so the settings decide both where a check is forwarded and whether that link
// is encrypted and verified. The protocol half needs a second agent and is
// covered by the integration suite.

// nscp_client.hpp expects its includer to have pulled in the client machinery
// and the logging macros already (NSCPClient.cpp does).
#include <client/command_line_parser.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>

#include "nscp_client.hpp"

namespace po = boost::program_options;  // nscp_handler.hpp expects the includer's alias
#include "nscp_handler.hpp"

#include <gtest/gtest.h>

#include <boost/asio.hpp>
#include <map>
#include <memory>
#include <string>
#include <vector>

nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

namespace {

// connection_data expands ${...} in certificate paths through the handler.
struct test_handler : socket_helpers::client::client_handler {
  void log_debug(std::string, int, std::string) const override {}
  void log_error(std::string, int, std::string) const override {}
  std::string expand_path(std::string path) override { return "/expanded" + path; }
};

client::destination_container target_with(const std::map<std::string, std::string> &options) {
  client::destination_container d;
  for (const auto &o : options) d.set_string_data(o.first, o.second);
  return d;
}

nscp_client::connection_data connection_for(const std::map<std::string, std::string> &options) {
  return nscp_client::connection_data(client::destination_container(), target_with(options), std::make_shared<test_handler>());
}

}  // namespace

TEST(NscpConnectionData, DefaultsToTheAgentsOwnPortAndQueryPath) {
  const nscp_client::connection_data con = connection_for({{"address", "agent.example.com"}});

  EXPECT_EQ(con.get_port(), "8443");
  EXPECT_EQ(con.path, "/query.pb");
}

TEST(NscpConnectionData, AnExplicitPortAndPathWin) {
  const nscp_client::connection_data con = connection_for({{"address", "agent.example.com:9443"}, {"path", "/custom.pb"}});

  EXPECT_EQ(con.get_port(), "9443");
  EXPECT_EQ(con.path, "/custom.pb");
}

TEST(NscpConnectionData, TakesTheTimeoutAndRetryFromTheTarget) {
  // Read from the container's typed fields, which is where
  // destination_container actually puts these two - the other clients look
  // them up in the data map and therefore never see them.
  client::destination_container target = target_with({{"address", "h"}});
  target.set_string_data("timeout", "5");
  target.set_string_data("retry", "1");

  const nscp_client::connection_data con(client::destination_container(), target, std::make_shared<test_handler>());

  EXPECT_EQ(con.timeout, 5);
  EXPECT_EQ(con.retry, 1);
}

TEST(NscpConnectionData, CipherAndVerifyDefaultsAreTheDocumentedOnes) {
  const nscp_client::connection_data con = connection_for({{"address", "h"}});

  EXPECT_EQ(con.ssl.allowed_ciphers, "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH") << "the cipher list that excludes anonymous and export suites";
  EXPECT_EQ(con.ssl.verify_mode, "none");
  EXPECT_EQ(con.ssl.certificate_key_format, "PEM");
}

TEST(NscpConnectionData, TlsMaterialIsCarriedAndPathsExpanded) {
  const nscp_client::connection_data con =
      connection_for({{"address", "h"}, {"certificate key", "${certificate-path}/key.pem"}, {"ca", "/etc/ca.pem"}, {"verify mode", "peer-cert"}});

  EXPECT_EQ(con.ssl.certificate_key, "/expanded${certificate-path}/key.pem") << "the handler resolves ${...} tokens";
  EXPECT_EQ(con.ssl.ca_path, "/etc/ca.pem");
  EXPECT_EQ(con.ssl.verify_mode, "peer-cert");
}

TEST(NscpConnectionData, SslHasTheFinalWordOverNoSsl) {
  // Two spellings, one inverted, applied in order: "no ssl" then "ssl".
  EXPECT_TRUE(connection_for({{"address", "h"}, {"no ssl", "false"}}).ssl.enabled);
  EXPECT_FALSE(connection_for({{"address", "h"}, {"no ssl", "true"}}).ssl.enabled);
  EXPECT_TRUE(connection_for({{"address", "h"}, {"no ssl", "true"}, {"ssl", "true"}}).ssl.enabled);
  EXPECT_FALSE(connection_for({{"address", "h"}, {"no ssl", "false"}, {"ssl", "false"}}).ssl.enabled);
}

TEST(NscpConnectionData, DescribesItselfWithoutLeakingThePassword) {
  // This is emitted at trace level on every operation, and historically leaked
  // the shared secret into operator debug logs.
  const std::string described = connection_for({{"address", "agent.example.com"}, {"password", "s3cret"}}).to_string();

  EXPECT_NE(described.find("agent.example.com"), std::string::npos) << described;
  EXPECT_EQ(described.find("s3cret"), std::string::npos) << "the password reached the trace line: " << described;
  EXPECT_NE(described.find("<set>"), std::string::npos) << "but whether one is configured is still visible: " << described;
}

TEST(NscpConnectionData, AnAbsentPasswordIsShownAsUnset) {
  const std::string described = connection_for({{"address", "h"}}).to_string();

  EXPECT_NE(described.find("<unset>"), std::string::npos) << described;
}

// ============================================================================
// nrpe_target_object / options_reader_impl (nscp_handler.hpp)
// ============================================================================

namespace {

// Minimal in-memory settings store (same shape as the mock used by the
// nscapi settings object tests).
class mock_settings : public nscapi::settings_helper::settings_impl_interface {
 public:
  std::map<std::string, std::map<std::string, std::string>> settings_data;

  void register_path(std::string, std::string, std::string, bool, bool) override {}
  void register_key(std::string, std::string, std::string, std::string, std::string, std::string, bool, bool, bool) override {}
  void register_subkey(std::string, std::string, std::string, bool, bool) override {}
  void register_tpl(std::string, std::string, std::string, std::string, std::string) override {}

  std::string get_string(std::string path, std::string key, std::string def) override {
    const auto pit = settings_data.find(path);
    if (pit == settings_data.end()) return def;
    const auto kit = pit->second.find(key);
    if (kit == pit->second.end()) return def;
    return kit->second;
  }
  void set_string(std::string path, std::string key, std::string value) override { settings_data[path][key] = value; }

  string_list get_sections(std::string) override { return {}; }
  string_list get_keys(std::string) override { return {}; }
  std::string expand_path(std::string key) override { return key; }
  void remove_key(std::string, std::string) override {}
  void remove_path(std::string) override {}

  void err(const char *, int, std::string) override {}
  void warn(const char *, int, std::string) override {}
  void info(const char *, int, std::string) override {}
  void debug(const char *, int, std::string) override {}
};

constexpr char kBasePath[] = "/settings/NSCP/client/targets";

}  // namespace

TEST(NscpTargetObject, ConstructorSetsTlsDefaults) {
  nscp_handler::nrpe_target_object obj("default", kBasePath);

  EXPECT_EQ(obj.get_property_int("timeout", 0), 30);
  EXPECT_EQ(obj.get_property_string("certificate format"), "PEM");
  EXPECT_EQ(obj.get_property_string("allowed ciphers"), "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");
  EXPECT_EQ(obj.get_property_string("verify mode"), "none");
  EXPECT_EQ(obj.get_property_string("password"), "");
}

TEST(NscpTargetObject, ReadAppliesConfiguredSettings) {
  auto proxy = std::make_shared<mock_settings>();
  const std::string path = std::string(kBasePath) + "/default";
  proxy->settings_data[path]["password"] = "s3cret";
  proxy->settings_data[path]["verify mode"] = "peer-cert";
  proxy->settings_data[path]["use ssl"] = "true";

  nscp_handler::nrpe_target_object obj("default", kBasePath);
  obj.read(proxy, false, false);

  EXPECT_EQ(obj.get_property_string("password"), "s3cret");
  EXPECT_EQ(obj.get_property_string("verify mode"), "peer-cert");
  EXPECT_TRUE(obj.get_property_bool("ssl", false)) << "'use ssl' maps to the 'ssl' property";
}

TEST(NscpTargetObject, TranslateDelegatesToParent) {
  nscp_handler::nrpe_target_object obj("default", kBasePath);
  obj.translate("password", "translated-pw");
  EXPECT_EQ(obj.get_property_string("password"), "translated-pw");
}

TEST(NscpOptionsReader, CreateAndCloneProduceTargets) {
  nscp_handler::options_reader_impl reader;
  const nscapi::settings_objects::object_instance obj = reader.create("t1", kBasePath);
  ASSERT_TRUE(obj);
  EXPECT_EQ(obj->get_alias(), "t1");
  EXPECT_EQ(obj->get_property_int("timeout", 0), 30);

  obj->set_property_string("password", "inherited");
  const nscapi::settings_objects::object_instance clone = reader.clone(obj, "t2", kBasePath);
  ASSERT_TRUE(clone);
  EXPECT_EQ(clone->get_alias(), "t2");
  EXPECT_EQ(clone->get_property_string("password"), "inherited");
}

TEST(NscpOptionsReader, ProcessMapsPasswordAndSslOptions) {
  po::options_description desc;
  client::destination_container source;
  client::destination_container data;
  nscp_handler::options_reader_impl reader;
  reader.process(desc, source, data);

  const std::vector<std::string> args = {"--password", "pw1", "--certificate", "/tmp/c.pem", "--verify", "peer-cert"};
  po::variables_map vm;
  po::store(po::command_line_parser(args).options(desc).run(), vm);
  po::notify(vm);

  EXPECT_EQ(data.get_string_data("password"), "pw1");
  EXPECT_EQ(data.get_string_data("certificate"), "/tmp/c.pem");
  EXPECT_EQ(data.get_string_data("verify mode"), "peer-cert");
}

// ============================================================================
// client_handler — logging against an uninitialised core
// ============================================================================

TEST(NscpClientLogging, LoggingIsANoOpWithoutACore) {
  // The plugin singleton's core wrapper has null function pointers in the
  // test binary: logging must degrade to a no-op, not crash.
  nscp_client::client_handler h;
  EXPECT_NO_THROW(h.log_debug(__FILE__, __LINE__, "debug message"));
  EXPECT_NO_THROW(h.log_error(__FILE__, __LINE__, "error message"));
}

TEST(NscpClientLogging, ExpandPathWithoutACoreThrows) {
  nscp_client::client_handler h;
  EXPECT_THROW(h.expand_path("${certificate-path}/x.pem"), std::exception);
}

// ============================================================================
// nscp_client_handler — command dispatch and transport error paths
// ============================================================================

namespace {

using test_client = nscp_client::nscp_client_handler<test_handler>;

// Grab a port that was just proven free and close it again, so connecting to
// it is (near-)deterministically refused.
unsigned short closed_port() {
  boost::asio::io_context io;
  boost::asio::ip::tcp::acceptor acceptor(io, boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), 0));
  return acceptor.local_endpoint().port();
}

client::destination_container unreachable_target() {
  client::destination_container t;
  t.set_string_data("address", "127.0.0.1:" + std::to_string(closed_port()));
  t.set_string_data("no ssl", "true");
  t.set_string_data("timeout", "2");
  t.set_string_data("retry", "0");
  return t;
}

}  // namespace

TEST(NscpClientHandler, GetCommandPrefersAliasThenCommand) {
  test_client h;
  EXPECT_EQ(h.get_command("alias", "cmd"), "alias");
  EXPECT_EQ(h.get_command("", "cmd"), "cmd");
  EXPECT_EQ(h.get_command("", ""), "");
}

TEST(NscpClientHandler, QueryReportsUnknownWhenTheAgentIsUnreachable) {
  test_client h;
  PB::Commands::QueryRequestMessage req;
  req.add_payload()->set_command("check_dummy");
  PB::Commands::QueryResponseMessage resp;

  EXPECT_TRUE(h.query(client::destination_container(), unreachable_target(), req, resp));

  ASSERT_EQ(resp.payload_size(), 1);
  EXPECT_EQ(resp.payload(0).result(), PB::Common::ResultCode::UNKNOWN);
  ASSERT_GE(resp.payload(0).lines_size(), 1);
  EXPECT_NE(resp.payload(0).lines(0).message().find("rror"), std::string::npos) << "the transport failure must reach the caller: "
                                                                                << resp.payload(0).lines(0).message();
}

TEST(NscpClientHandler, QueryLogsValidationErrorsForMissingTlsMaterial) {
  // An enabled-but-broken TLS setup (missing key file) must still produce a
  // response; the validation errors go to the error log.
  test_client h;
  client::destination_container target = unreachable_target();
  target.set_string_data("ssl", "true");
  target.set_string_data("certificate key", "/nonexistent/key.pem");

  PB::Commands::QueryRequestMessage req;
  req.add_payload()->set_command("check_dummy");
  PB::Commands::QueryResponseMessage resp;

  EXPECT_TRUE(h.query(client::destination_container(), target, req, resp));
  ASSERT_EQ(resp.payload_size(), 1);
  EXPECT_EQ(resp.payload(0).result(), PB::Common::ResultCode::UNKNOWN);
}

TEST(NscpClientHandler, SubmitProducesOnePayloadPerRequestEntry) {
  test_client h;
  PB::Commands::SubmitRequestMessage req;
  PB::Commands::QueryResponseMessage::Response *p = req.add_payload();
  p->set_command("c1");
  p->add_arguments("a1");
  PB::Commands::SubmitResponseMessage resp;

  EXPECT_TRUE(h.submit(client::destination_container(), unreachable_target(), req, resp));

  ASSERT_EQ(resp.payload_size(), 1);
  EXPECT_EQ(resp.payload(0).command(), "c1");
  EXPECT_FALSE(resp.payload(0).result().message().empty());
}

TEST(NscpClientHandler, SubmitUsesTheAliasWhenSet) {
  test_client h;
  PB::Commands::SubmitRequestMessage req;
  PB::Commands::QueryResponseMessage::Response *p = req.add_payload();
  p->set_command("real_command");
  p->set_alias("friendly_name");
  PB::Commands::SubmitResponseMessage resp;

  EXPECT_TRUE(h.submit(client::destination_container(), unreachable_target(), req, resp));

  ASSERT_EQ(resp.payload_size(), 1);
  EXPECT_EQ(resp.payload(0).command(), "friendly_name");
}

TEST(NscpClientHandler, ExecForwardsCommandAndArguments) {
  test_client h;
  PB::Commands::ExecuteRequestMessage req;
  PB::Commands::ExecuteRequestMessage::Request *p = req.add_payload();
  p->set_command("do_thing");
  p->add_arguments("arg1");
  PB::Commands::ExecuteResponseMessage resp;

  EXPECT_TRUE(h.exec(client::destination_container(), unreachable_target(), req, resp));

  ASSERT_EQ(resp.payload_size(), 1);
  EXPECT_EQ(resp.payload(0).command(), "do_thing");
  EXPECT_FALSE(resp.payload(0).message().empty());
}

TEST(NscpClientHandler, MetricsAreNotSupported) {
  test_client h;
  PB::Metrics::MetricsMessage msg;
  EXPECT_FALSE(h.metrics(client::destination_container(), unreachable_target(), msg));
}
