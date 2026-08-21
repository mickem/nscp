// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Tests for the SMTP client's connection settings.
//
// smtp.cpp (the protocol itself) has its own suite; this covers the settings
// that decide who the agent talks to and how: the submission port, the
// security mode, the credentials, and the name it announces in EHLO. The
// message templates live here too, since a target that renders an empty
// subject is a target whose mail gets filed as spam.

// smtp_client.hpp expects its includer to have pulled in the client
// machinery and the logging macros already (SMTPClient.cpp does).
#include <client/command_line_parser.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>

#include "smtp_client.hpp"

#include <gtest/gtest.h>

#include <map>
#include <string>

nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

namespace {

client::destination_container target_with(const std::map<std::string, std::string> &options) {
  client::destination_container d;
  for (const auto &o : options) d.set_string_data(o.first, o.second);
  return d;
}

smtp_client::connection_data connection_for(const std::map<std::string, std::string> &options,
                                            const client::destination_container &sender = client::destination_container()) {
  return smtp_client::connection_data(target_with(options), sender);
}

}  // namespace

TEST(SmtpConnectionData, DefaultsToTheSubmissionPort) {
  // 587 (submission), not 25 (relay): the agent authenticates and submits.
  EXPECT_EQ(connection_for({{"address", "mail.example.com"}}).get_port(), "587");
}

TEST(SmtpConnectionData, AnExplicitPortWins) {
  EXPECT_EQ(connection_for({{"address", "mail.example.com:2525"}}).get_port(), "2525");
}

TEST(SmtpConnectionData, DefaultsToStarttls) {
  // Silently sending mail in clear because the target did not name a mode
  // would be the wrong default.
  EXPECT_EQ(connection_for({{"address", "h"}}).security, "starttls");
}

TEST(SmtpConnectionData, AnExplicitSecurityModeIsKept) {
  EXPECT_EQ(connection_for({{"address", "h"}, {"security", "none"}}).security, "none");
  EXPECT_EQ(connection_for({{"address", "h"}, {"security", "ssl"}}).security, "ssl");
}

TEST(SmtpConnectionData, CertificateVerificationIsOnUnlessWaived) {
  EXPECT_FALSE(connection_for({{"address", "h"}}).insecure_skip_verify);
  EXPECT_TRUE(connection_for({{"address", "h"}, {"insecure-skip-verify", "true"}}).insecure_skip_verify);
}

TEST(SmtpConnectionData, CarriesTheCredentialsAndEnvelope) {
  const smtp_client::connection_data con = connection_for({{"address", "h"},
                                                           {"username", "agent"},
                                                           {"password", "s3cret"},
                                                           {"sender", "nscp@example.com"},
                                                           {"recipient", "ops@example.com"},
                                                           {"subject", "%status%: %alias%"},
                                                           {"template", "%message%"}});

  EXPECT_EQ(con.username, "agent");
  EXPECT_EQ(con.password, "s3cret");
  EXPECT_EQ(con.sender, "nscp@example.com");
  EXPECT_EQ(con.recipient_str, "ops@example.com");
  EXPECT_EQ(con.subject_template, "%status%: %alias%");
  EXPECT_EQ(con.template_string, "%message%");
}

TEST(SmtpConnectionData, TheEhloNameComesFromTheSenderWhenNotConfigured) {
  // Regression: the sender's host name was read from the free-form data map,
  // where destination_container never puts it, so this stayed empty and the
  // EHLO fell back to "localhost" - which some servers reject outright.
  client::destination_container sender;
  sender.set_host("agent-host");

  const smtp_client::connection_data con = connection_for({{"address", "h"}}, sender);

  EXPECT_EQ(con.sender_hostname, "agent-host");
}

TEST(SmtpConnectionData, AnExplicitEhloHostnameWins) {
  client::destination_container sender;
  sender.set_host("agent-host");

  const smtp_client::connection_data con = connection_for({{"address", "h"}, {"ehlo-hostname", "mail-gw.example.com"}}, sender);

  EXPECT_EQ(con.canonical_name, "mail-gw.example.com");
  EXPECT_EQ(con.sender_hostname, "agent-host") << "the sender is still recorded";
}

TEST(SmtpConnectionData, DescribesItselfWithoutLeakingThePassword) {
  // to_string() is emitted on every operation at trace level.
  const std::string described = connection_for({{"address", "mail.example.com"}, {"username", "agent"}, {"password", "s3cret"}}).to_string();

  EXPECT_NE(described.find("mail.example.com"), std::string::npos) << described;
  EXPECT_EQ(described.find("s3cret"), std::string::npos) << "the password must never reach a log: " << described;
}
