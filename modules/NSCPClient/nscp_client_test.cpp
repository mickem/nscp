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

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <string>

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
