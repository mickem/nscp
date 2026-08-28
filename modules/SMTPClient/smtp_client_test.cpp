// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Tests for the SMTP client's connection settings.
//
// smtp.cpp (the protocol itself) has its own suite; this covers the settings
// that decide who the agent talks to and how: the submission port, the
// security mode, the credentials, and the name it announces in EHLO. The
// message templates live here too, since a target that renders an empty
// subject is a target whose mail gets filed as spam.

#include "smtp_client.hpp"

#include <gtest/gtest.h>

#include <client/command_line_parser.hpp>
#include <map>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <string>
#include <vector>

#include "smtp_handler.hpp"

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

TEST(SmtpConnectionData, AnExplicitPortWins) { EXPECT_EQ(connection_for({{"address", "mail.example.com:2525"}}).get_port(), "2525"); }

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

TEST(SmtpConnectionData, CarriesTheConfiguredCaBundle) {
  // The target's `ca` (defaulting to ${ca-path}, already expanded by the
  // settings layer) is what verification runs against. Without it the client
  // fell back to OpenSSL's default verify paths, which on Windows do not
  // include the system certificate store - so verification failed against
  // every public provider and insecure-skip-verify was the only way out.
  EXPECT_EQ(connection_for({{"address", "h"}, {"ca", "/etc/ssl/certs/ca-certificates.crt"}}).ca_path, "/etc/ssl/certs/ca-certificates.crt");
}

TEST(SmtpConnectionData, AnUnsetCaMeansTheOpenSslDefaults) { EXPECT_EQ(connection_for({{"address", "h"}}).ca_path, ""); }

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

TEST(SmtpConnectionData, DoesNotClaimToHonourRetry) {
  // smtp::send() makes one attempt per submission. The setting used to be read
  // into the inherited retry field, where nothing consumed it, so the target
  // looked configured for retries it never performed. The inherited default
  // stands, untouched by the target's own `retry`.
  const smtp_client::connection_data con = connection_for({{"address", "h"}, {"retry", "7"}});

  EXPECT_NE(con.retry, 7) << "retry is not honoured, so it must not be read in as though it were";
}

// =============================================================================
// Command line / REST argument handling
// =============================================================================

namespace {

// Parse `args` through the module's own option descriptor, over a destination
// container that already carries the target's settings, and return it. This is
// the shape the real path has: the target is loaded first, then the command
// line is parsed on top of it.
client::destination_container parse_over_target(const std::map<std::string, std::string> &target_options, const std::vector<std::string> &args) {
  client::destination_container data = target_with(target_options);
  client::destination_container source;

  smtp_handler::options_reader_impl reader;
  boost::program_options::options_description desc("test");
  reader.process(desc, source, data);

  boost::program_options::variables_map vm;
  boost::program_options::store(boost::program_options::command_line_parser(args).options(desc).run(), vm);
  boost::program_options::notify(vm);
  return data;
}

}  // namespace

TEST(SmtpOptions, AcceptsAValuedBooleanAsRestPassesIt) {
  // REST hands the whole "key=value" over as one token. bool_switch rejects
  // that with "does not take any arguments" - and only over REST, so it would
  // have looked fine from the command line.
  client::destination_container data = parse_over_target({{"address", "h"}}, {"--insecure-skip-verify=true"});

  EXPECT_TRUE(data.get_bool_data("insecure-skip-verify"));
}

TEST(SmtpOptions, AcceptsABareBooleanFlagFromTheCommandLine) {
  client::destination_container data = parse_over_target({{"address", "h"}}, {"--insecure-skip-verify"});

  EXPECT_TRUE(data.get_bool_data("insecure-skip-verify"));
}

TEST(SmtpOptions, AValuedFalseTurnsTheFlagOff) {
  client::destination_container data = parse_over_target({{"address", "h"}, {"insecure-skip-verify", "true"}}, {"--insecure-skip-verify=false"});

  EXPECT_FALSE(data.get_bool_data("insecure-skip-verify"));
}

TEST(SmtpOptions, AnAbsentFlagLeavesTheTargetSettingAlone) {
  // The option carries no default: `data` arrives already populated from the
  // target, so a default would fire the notifier with false on every
  // submission and quietly undo the target's own insecure-skip-verify.
  client::destination_container data = parse_over_target({{"address", "h"}, {"insecure-skip-verify", "true"}}, {});

  EXPECT_TRUE(data.get_bool_data("insecure-skip-verify")) << "the command line did not mention the option, so the target's value must stand";
}
