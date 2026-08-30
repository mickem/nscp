// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Tests for the syslog client's payload construction.
//
// Two halves, both untested until now. The first is connection_data, which
// turns the target's settings into typed connection info plus the RFC 3164
// priority tables - a wrong default here points an agent at the wrong port or
// files everything under the wrong facility, quietly. The second is what
// submit() actually puts on the wire: one datagram per check result, with the
// priority chosen from that result's status, the templates expanded, and any
// newline in the check output neutralised so a result cannot forge extra
// syslog records.
//
// The datagrams are read back from a real loopback UDP socket rather than
// asserted against a refactored-out builder: the wire format is the contract,
// and syslog is connectionless so there is nothing to stand up.
//
// A third half has grown since: the key plumbing. The option parser and the
// target object both feed a free-form string map that connection_data reads
// back by name, so a key spelled differently on the two sides is not an
// error anywhere - the setting just silently never reaches the wire. The
// SyslogOptions/SyslogTarget tests pin every such key end to end.

// syslog_client.hpp expects its includer to have pulled in the client
// machinery already (SyslogClient.cpp does); include it here too.
#include <client/command_line_parser.hpp>
#include <nscapi/nscapi_targets.hpp>

// syslog_handler.hpp relies on its includer for the po alias (normally
// SyslogClient.h provides it).
namespace po = boost::program_options;

#include <gtest/gtest.h>

#include <algorithm>
#include <boost/asio.hpp>
#include <chrono>
#include <fstream>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "syslog_client.hpp"
#include "syslog_handler.hpp"

// Normally provided by NSC_WRAP_DLL(); the module's logging macros need it.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

namespace {

using boost::asio::ip::udp;

client::destination_container target_with(const std::map<std::string, std::string> &options) {
  client::destination_container d;
  for (const auto &o : options) d.set_string_data(o.first, o.second);
  return d;
}

// A bound UDP socket on an ephemeral port, and the datagrams it received.
class syslog_sink {
 public:
  syslog_sink() : socket_(io_, udp::endpoint(boost::asio::ip::make_address("127.0.0.1"), 0)) {}

  unsigned short port() const { return socket_.local_endpoint().port(); }

  // Read whatever has arrived. Everything is local and already sent by the
  // time submit() returns, so a short poll is enough - but never block the
  // suite if a datagram is missing.
  std::vector<std::string> drain(const std::size_t expected) {
    std::vector<std::string> out;
    socket_.non_blocking(true);
    for (int attempt = 0; attempt < 200 && out.size() < expected; attempt++) {
      char buffer[4096];
      boost::system::error_code ec;
      const std::size_t len = socket_.receive(boost::asio::buffer(buffer), 0, ec);
      if (!ec) {
        out.emplace_back(buffer, len);
        continue;
      }
      boost::asio::steady_timer timer(io_, boost::asio::chrono::milliseconds(5));
      timer.wait();
    }
    return out;
  }

 private:
  boost::asio::io_context io_;
  udp::socket socket_;
};

// Submit one check result to a syslog sink and hand back the datagrams.
std::vector<std::string> submit_to(syslog_sink &sink, const PB::Common::ResultCode result, const std::string &message,
                                   std::map<std::string, std::string> options = {}, const std::string &sender_host = "") {
  options["address"] = "127.0.0.1";
  options["port"] = std::to_string(sink.port());
  if (options.find("facility") == options.end()) options["facility"] = "kernel";
  if (options.find("severity") == options.end()) options["severity"] = "error";
  if (options.find("tag template") == options.end()) options["tag template"] = "nscp";
  if (options.find("message template") == options.end()) options["message template"] = "%message%";

  PB::Commands::SubmitRequestMessage request;
  PB::Commands::QueryResponseMessage::Response *payload = request.add_payload();
  payload->set_command("check_test");
  payload->set_result(result);
  payload->add_lines()->set_message(message);

  PB::Commands::SubmitResponseMessage response;
  syslog_client::syslog_client_handler handler;
  client::destination_container sender;
  if (!sender_host.empty()) sender.set_host(sender_host);
  handler.submit(sender, target_with(options), request, response);
  return sink.drain(1);
}

// The <PRI> prefix of a syslog line, without the angle brackets.
int priority_of(const std::string &datagram) {
  const std::string::size_type end = datagram.find('>');
  if (datagram.empty() || datagram[0] != '<' || end == std::string::npos) return -1;
  return std::stoi(datagram.substr(1, end - 1));
}

}  // namespace

// ---------------------------------------------------------------------------
// connection_data - settings in, connection info out.
// ---------------------------------------------------------------------------

TEST(SyslogConnectionData, DefaultsToTheSyslogPortAndSaneRetries) {
  const syslog_client::connection_data con(target_with({{"address", "syslog.example.com"}}), client::destination_container());

  EXPECT_EQ(con.get_port(), "514");
  // A bare container carries destination_container's own defaults (10/2);
  // a settings-defined target overrides them with its documented 30/3 - see
  // SyslogTarget.TheTargetDefaultsReachTheConnection.
  EXPECT_EQ(con.timeout, 10u);
  EXPECT_EQ(con.retry, 2);
}

TEST(SyslogConnectionData, TakesThePortFromTheTarget) {
  const syslog_client::connection_data con(target_with({{"address", "syslog.example.com"}, {"port", "1514"}}), client::destination_container());

  EXPECT_EQ(con.get_port(), "1514");
}

TEST(SyslogConnectionData, AConfiguredTimeoutReachesTheConnection) {
  // This used to be a characterization test for the opposite behaviour:
  // connection_data read `get_int_data("timeout")`, which looks in the
  // container's free-form data map, but destination_container routes the
  // well-known keys "timeout" and "retry" into typed *fields* instead of
  // that map - so the lookup never found them and the default won no matter
  // what the operator configured. Harmless while syslog was UDP-only; with
  // stream transports the value is the connect/handshake/write deadline and
  // the retry count, so it now reads the typed fields. (The same stale
  // expression still exists in NSCAClient, IcingaClient and CollectdClient.)
  const syslog_client::connection_data con(target_with({{"address", "h"}, {"timeout", "5"}, {"retry", "1"}}), client::destination_container());

  EXPECT_EQ(con.timeout, 5u);
  EXPECT_EQ(con.retry, 1);
}

TEST(SyslogConnectionData, ASettingsStyleRetriesKeyReachesTheConnection) {
  // Settings-defined targets spell the key `retries` (the target object's
  // registered key), which stays in the free-form map; the CLI spells it
  // --retry/--retries and both land in the typed field. Both must work.
  const syslog_client::connection_data con(target_with({{"address", "h"}, {"retries", "7"}}), client::destination_container());

  EXPECT_EQ(con.retry, 7);
}

TEST(SyslogConnectionData, CarriesTheTemplatesAndSeverities) {
  const syslog_client::connection_data con(target_with({{"address", "h"},
                                                        {"severity", "error"},
                                                        {"facility", "daemon"},
                                                        {"tag template", "my-tag"},
                                                        {"message template", "prefix %message%"},
                                                        {"ok severity", "informational"},
                                                        {"warning severity", "warning"},
                                                        {"critical severity", "critical"},
                                                        {"unknown severity", "notice"}}),
                                           client::destination_container());

  EXPECT_EQ(con.severity, "error");
  EXPECT_EQ(con.facility, "daemon");
  EXPECT_EQ(con.tag_syntax, "my-tag");
  EXPECT_EQ(con.message_syntax, "prefix %message%");
  EXPECT_EQ(con.ok_severity, "informational");
  EXPECT_EQ(con.crit_severity, "critical");
  // to_string() is what the debug log prints when a submit misbehaves; it has
  // to name the endpoint and both templates or the log is useless.
  const std::string described = con.to_string();
  EXPECT_NE(described.find("my-tag"), std::string::npos) << described;
  EXPECT_NE(described.find("prefix %message%"), std::string::npos) << described;
}

// ---------------------------------------------------------------------------
// parse_priority - RFC 3164 says PRI = facility * 8 + severity.
// ---------------------------------------------------------------------------

TEST(SyslogPriority, IsFacilityTimesEightPlusSeverity) {
  syslog_client::connection_data con(target_with({{"address", "h"}}), client::destination_container());

  EXPECT_EQ(con.parse_priority("emergency", "kernel"), "<0>");  // 0 * 8 + 0
  EXPECT_EQ(con.parse_priority("debug", "kernel"), "<7>");      // 0 * 8 + 7
  EXPECT_EQ(con.parse_priority("emergency", "user"), "<8>");    // 1 * 8 + 0
  EXPECT_EQ(con.parse_priority("error", "local0"), "<131>");    // 16 * 8 + 3
  EXPECT_EQ(con.parse_priority("debug", "local7"), "<191>");    // 23 * 8 + 7
}

TEST(SyslogPriority, AnUnknownNameFallsBackToUserNotice) {
  // Both halves come from operator configuration, so a typo must not send a
  // random priority - and must not escalate either. The old fallback of <0>
  // was kernel.emergency, which receivers page or broadcast on; a typo now
  // degrades to user.notice (<13>) and logs.
  syslog_client::connection_data con(target_with({{"address", "h"}}), client::destination_container());

  EXPECT_EQ(con.parse_priority("error", "no-such-facility"), "<13>");
  EXPECT_EQ(con.parse_priority("no-such-severity", "kernel"), "<13>");
}

TEST(SyslogPriority, ClockResolvesToFifteenBecauseTheNameIsRegisteredTwice) {
  // CHARACTERIZATION TEST - the table sets facilities["clock"] twice, to 9 and
  // then to 15, so the second wins and facility 9 has no name at all. RFC 3164
  // does list both 9 ("clock daemon") and 15 ("clock daemon" again), so the
  // duplicate is in the spec too - but an operator who wants 9 cannot ask for
  // it. Pinned rather than changed: renaming either entry changes what an
  // existing `facility = clock` configuration puts on the wire.
  syslog_client::connection_data con(target_with({{"address", "h"}}), client::destination_container());

  EXPECT_EQ(con.parse_priority("emergency", "clock"), "<120>");  // 15 * 8, not 9 * 8
}

// ---------------------------------------------------------------------------
// submit - what actually goes on the wire.
// ---------------------------------------------------------------------------

TEST(SyslogSubmit, SendsOneDatagramCarryingTheCheckOutput) {
  syslog_sink sink;

  const std::vector<std::string> sent = submit_to(sink, PB::Common::ResultCode::OK, "everything is fine");

  ASSERT_EQ(sent.size(), 1u);
  EXPECT_NE(sent[0].find("everything is fine"), std::string::npos) << sent[0];
  EXPECT_NE(sent[0].find("nscp"), std::string::npos) << "the tag template is missing: " << sent[0];
}

TEST(SyslogSubmit, TheSeverityFollowsTheCheckResult) {
  // This is the mapping an operator configures per status; getting it wrong
  // means a CRITICAL check arriving as debug and never paging anyone.
  syslog_sink sink;
  const std::map<std::string, std::string> severities = {
      {"ok severity", "informational"}, {"warning severity", "warning"}, {"critical severity", "critical"}, {"unknown severity", "notice"}};

  // facility kernel (0), so the priority is the severity number itself.
  EXPECT_EQ(priority_of(submit_to(sink, PB::Common::ResultCode::OK, "m", severities).at(0)), 6);
  EXPECT_EQ(priority_of(submit_to(sink, PB::Common::ResultCode::WARNING, "m", severities).at(0)), 4);
  EXPECT_EQ(priority_of(submit_to(sink, PB::Common::ResultCode::CRITICAL, "m", severities).at(0)), 2);
  EXPECT_EQ(priority_of(submit_to(sink, PB::Common::ResultCode::UNKNOWN, "m", severities).at(0)), 5);
}

TEST(SyslogSubmit, AMissingPerStateSeverityFallsBackToTheBaseSeverity) {
  // A submission that sets only `severity` (the test helper's default,
  // "error") still gets a sane priority for every state instead of the
  // undefined-severity fallback.
  syslog_sink sink;

  EXPECT_EQ(priority_of(submit_to(sink, PB::Common::ResultCode::CRITICAL, "m").at(0)), 3);  // kernel.error
}

TEST(SyslogSubmit, ExpandsTheTagAndMessageTemplates) {
  syslog_sink sink;

  const std::vector<std::string> sent =
      submit_to(sink, PB::Common::ResultCode::OK, "the output", {{"tag template", "tag[%message%]"}, {"message template", "msg=%message%"}});

  ASSERT_EQ(sent.size(), 1u);
  // %message% expands to the check's own output - the status word is not part
  // of it, since the priority already carries the status.
  EXPECT_NE(sent[0].find("tag[the output]"), std::string::npos) << sent[0];
  EXPECT_NE(sent[0].find("msg=the output"), std::string::npos) << sent[0];
}

TEST(SyslogSubmit, TheDatagramCarriesTheSenderHostname) {
  // RFC 3164: <PRI>TIMESTAMP HOSTNAME TAG MESSAGE. Without the HOSTNAME field
  // the receiver promotes the next token - the tag - to origin host, and a
  // tag template that expands %message% would let check output choose which
  // host a record is filed under. The module's `hostname` setting rides in on
  // the sender container.
  syslog_sink sink;

  const std::vector<std::string> sent = submit_to(sink, PB::Common::ResultCode::OK, "all good", {}, "agent01.example.com");

  ASSERT_EQ(sent.size(), 1u);
  EXPECT_NE(sent[0].find(" agent01.example.com nscp "), std::string::npos) << sent[0];
}

TEST(SyslogSubmit, AnUnknownSenderBecomesTheNilHostname) {
  // No sender host may not shift the fields either; the RFC 5424 nil value
  // "-" holds the HOSTNAME position.
  syslog_sink sink;

  const std::vector<std::string> sent = submit_to(sink, PB::Common::ResultCode::OK, "all good");

  ASSERT_EQ(sent.size(), 1u);
  EXPECT_NE(sent[0].find(" - nscp "), std::string::npos) << sent[0];
}

TEST(SyslogSubmit, ControlBytesInTheCheckOutputAreNeutralised) {
  // CR/LF record splitting is only one control-byte trick: an ANSI escape
  // sequence in check output would replay in the terminal of whoever views
  // the log. Every C0 byte and DEL becomes a space.
  syslog_sink sink;

  const std::vector<std::string> sent = submit_to(sink, PB::Common::ResultCode::OK,
                                                  std::string("colour\x1b[31mred\x07"
                                                              "bell\ttab\x7f"));

  ASSERT_EQ(sent.size(), 1u);
  for (const char c : sent[0]) {
    EXPECT_GE(static_cast<unsigned char>(c), 0x20u) << "control byte survived: " << sent[0];
    EXPECT_NE(c, '\x7f') << sent[0];
  }
  // The printable text survives, spaced apart.
  EXPECT_NE(sent[0].find("colour"), std::string::npos) << sent[0];
  EXPECT_NE(sent[0].find("bell"), std::string::npos) << sent[0];
}

// ---------------------------------------------------------------------------
// Key plumbing - the option parser and the target object feed a free-form
// string map that connection_data reads back by name, so a mismatched key is
// not an error anywhere: the setting just silently never reaches the wire.
// ---------------------------------------------------------------------------

TEST(SyslogOptions, PerStateSeverityOptionsReachTheConnection) {
  // These options used to store underscore keys ("ok_severity") that
  // connection_data never read: the option parsed fine, did nothing, and the
  // empty severity then failed parse_priority - which used to mean
  // kernel.emergency on the wire.
  syslog_handler::options_reader_impl reader;
  po::options_description desc;
  client::destination_container source, data;
  reader.process(desc, source, data);

  const char *argv[] = {"test", "--ok-severity", "notice", "--warning-severity", "error", "--critical-severity", "alert", "--unknown-severity", "debug"};
  po::variables_map vm;
  po::store(po::parse_command_line(static_cast<int>(std::size(argv)), const_cast<char **>(argv), desc), vm);
  po::notify(vm);

  data.set_string_data("address", "h");
  const syslog_client::connection_data con(data, client::destination_container());

  EXPECT_EQ(con.ok_severity, "notice");
  EXPECT_EQ(con.warn_severity, "error");
  EXPECT_EQ(con.crit_severity, "alert");
  EXPECT_EQ(con.unknown_severity, "debug");
}

TEST(SyslogTarget, TheTargetDefaultsReachTheConnection) {
  // The target object's property keys must be the ones connection_data
  // reads: "tag syntax"/"message syntax" used to be stored and never read,
  // so a settings-defined target sent an empty tag and dropped the message
  // text entirely.
  const auto target = std::make_shared<syslog_handler::syslog_target_object>("default", "/settings/syslog/client/targets/default");

  client::destination_container d;
  d.apply(target);
  d.set_string_data("address", "h");

  syslog_client::connection_data con(d, client::destination_container());
  EXPECT_EQ(con.tag_syntax, "NSCA");
  EXPECT_EQ(con.message_syntax, "%message%");
  EXPECT_EQ(con.severity, "error");
  EXPECT_EQ(con.facility, "kernel");
  EXPECT_EQ(con.ok_severity, "informational");
  EXPECT_EQ(con.warn_severity, "warning");
  EXPECT_EQ(con.crit_severity, "critical");
  EXPECT_EQ(con.unknown_severity, "emergency");
  // The documented target defaults - and the historical transport.
  EXPECT_EQ(con.timeout, 30u);
  EXPECT_EQ(con.retry, 3);
  EXPECT_EQ(con.transport, "udp");
  EXPECT_FALSE(con.ssl.enabled);
  EXPECT_TRUE(con.config_error.empty()) << con.config_error;
}

TEST(SyslogSubmit, ANewlineInTheCheckOutputCannotForgeASecondRecord) {
  // Log injection: a check whose output contains CR/LF would otherwise split
  // into two syslog records, letting a monitored process write arbitrary
  // entries - including ones that look like they came from something else.
  syslog_sink sink;

  const std::vector<std::string> sent = submit_to(sink, PB::Common::ResultCode::OK, "first line\n<0>Jan  1 00:00:00 forged: second line");

  ASSERT_EQ(sent.size(), 1u) << "the output split into more than one datagram";
  EXPECT_EQ(sent[0].find('\n'), std::string::npos) << sent[0];
  EXPECT_EQ(sent[0].find('\r'), std::string::npos) << sent[0];
  // The text survives, only the line breaks are neutralised.
  EXPECT_NE(sent[0].find("first line"), std::string::npos) << sent[0];
  EXPECT_NE(sent[0].find("forged: second line"), std::string::npos) << sent[0];
}

TEST(SyslogSubmit, EveryPayloadInABatchGetsItsOwnDatagram) {
  syslog_sink sink;

  PB::Commands::SubmitRequestMessage request;
  for (const char *name : {"check_a", "check_b", "check_c"}) {
    PB::Commands::QueryResponseMessage::Response *payload = request.add_payload();
    payload->set_command(name);
    payload->set_result(PB::Common::ResultCode::OK);
    payload->add_lines()->set_message(std::string("output of ") + name);
  }

  PB::Commands::SubmitResponseMessage response;
  syslog_client::syslog_client_handler handler;
  handler.submit(client::destination_container(),
                 target_with({{"address", "127.0.0.1"},
                              {"port", std::to_string(sink.port())},
                              {"facility", "kernel"},
                              {"severity", "error"},
                              {"tag template", "nscp"},
                              {"message template", "%message%"}}),
                 request, response);

  const std::vector<std::string> sent = sink.drain(3);
  ASSERT_EQ(sent.size(), 3u);
  EXPECT_NE(sent[0].find("output of check_a"), std::string::npos) << sent[0];
  EXPECT_NE(sent[2].find("output of check_c"), std::string::npos) << sent[2];
}

TEST(SyslogSubmit, OnlySubmitIsSupported) {
  // The syslog protocol is one-way: there is nothing to query or execute, and
  // the handler has to say so rather than pretend it worked.
  syslog_client::syslog_client_handler handler;
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

// ---------------------------------------------------------------------------
// Transport selection - udp stays the default, tcp/tls are opt-in, and a
// configuration that makes no sense fails the submission instead of guessing
// (the wrong guess would be sending in the clear on a target the operator
// configured for TLS).
// ---------------------------------------------------------------------------

TEST(SyslogTransport, DefaultsToUdpOnTheSyslogPort) {
  const syslog_client::connection_data con(target_with({{"address", "h"}}), client::destination_container());

  EXPECT_EQ(con.transport, "udp");
  EXPECT_EQ(con.get_port(), "514");
  EXPECT_FALSE(con.ssl.enabled);
  EXPECT_TRUE(con.config_error.empty()) << con.config_error;
}

TEST(SyslogTransport, TcpIsPlainOnTheSyslogPort) {
  const syslog_client::connection_data con(target_with({{"address", "h"}, {"transport", "tcp"}}), client::destination_container());

  EXPECT_EQ(con.transport, "tcp");
  EXPECT_EQ(con.get_port(), "514");
  EXPECT_FALSE(con.ssl.enabled);
  EXPECT_TRUE(con.config_error.empty()) << con.config_error;
}

TEST(SyslogTransport, TlsUsesTheRfc5425PortAndVerifiesPeersByDefault) {
  const syslog_client::connection_data con(target_with({{"address", "h"}, {"transport", "tls"}}), client::destination_container());

  EXPECT_EQ(con.transport, "tls");
  // RFC 5425 assigns syslog-over-TLS its own port; an explicit port still
  // wins (next test).
  EXPECT_EQ(con.get_port(), "6514");
  EXPECT_TRUE(con.ssl.enabled);
  // Verification must be the default - a TLS transport that accepts any
  // certificate is just obfuscation.
  EXPECT_EQ(con.ssl.verify_mode, "peer");
  EXPECT_EQ(con.ssl.tls_version, "1.2+");
  EXPECT_TRUE(con.config_error.empty()) << con.config_error;
}

TEST(SyslogTransport, AnExplicitPortBeatsTheTransportDefault) {
  const syslog_client::connection_data con(target_with({{"address", "h"}, {"port", "10514"}, {"transport", "tls"}}), client::destination_container());

  EXPECT_EQ(con.get_port(), "10514");
}

TEST(SyslogTransport, TheTreeWideUseSslBooleanMeansTls) {
  // `use ssl` is the convention every other client target understands; here
  // it selects the TLS transport.
  const syslog_client::connection_data con(target_with({{"address", "h"}, {"ssl", "true"}}), client::destination_container());

  EXPECT_EQ(con.transport, "tls");
  EXPECT_TRUE(con.ssl.enabled);
  EXPECT_TRUE(con.config_error.empty()) << con.config_error;
}

TEST(SyslogTransport, UseSslUpgradesTcpAndNeverDowngrades) {
  const syslog_client::connection_data upgraded(target_with({{"address", "h"}, {"transport", "tcp"}, {"ssl", "true"}}), client::destination_container());
  EXPECT_EQ(upgraded.transport, "tls");
  EXPECT_TRUE(upgraded.config_error.empty()) << upgraded.config_error;

  // ...but combined with an explicit udp it is a contradiction, and the
  // submission must fail rather than pick a side silently.
  const syslog_client::connection_data contradiction(target_with({{"address", "h"}, {"transport", "udp"}, {"ssl", "true"}}), client::destination_container());
  EXPECT_FALSE(contradiction.config_error.empty());
}

TEST(SyslogTransport, AnAddressSchemeSelectsTheTransport) {
  const syslog_client::connection_data con(target_with({{"address", "tls://h"}}), client::destination_container());

  EXPECT_EQ(con.transport, "tls");
  EXPECT_EQ(con.get_port(), "6514");
  EXPECT_TRUE(con.ssl.enabled);
}

TEST(SyslogTransport, AnUnknownTransportIsAConfigError) {
  const syslog_client::connection_data con(target_with({{"address", "h"}, {"transport", "sctp"}}), client::destination_container());

  EXPECT_FALSE(con.config_error.empty());
  EXPECT_NE(con.config_error.find("sctp"), std::string::npos) << con.config_error;
}

TEST(SyslogTransport, InsecureDisablesVerificationExplicitly) {
  // The only ways out of peer verification are explicit: `insecure = true`
  // or `verify mode = none`. There is no silent verify-none path.
  const syslog_client::connection_data con(target_with({{"address", "h"}, {"transport", "tls"}, {"insecure", "true"}}), client::destination_container());

  EXPECT_EQ(con.ssl.verify_mode, "none");
}

TEST(SyslogTransport, AConfigErrorFailsTheSubmission) {
  // No sink: an invalid transport must be refused before anything is sent.
  PB::Commands::SubmitRequestMessage request;
  PB::Commands::QueryResponseMessage::Response *payload = request.add_payload();
  payload->set_command("check_test");
  payload->set_result(PB::Common::ResultCode::OK);
  payload->add_lines()->set_message("m");

  PB::Commands::SubmitResponseMessage response;
  syslog_client::syslog_client_handler handler;
  handler.submit(client::destination_container(), target_with({{"address", "127.0.0.1"}, {"transport", "sctp"}}), request, response);

  ASSERT_EQ(response.payload_size(), 1);
  EXPECT_EQ(response.payload(0).result().code(), PB::Common::Result_StatusCodeType_STATUS_ERROR);
  EXPECT_NE(response.payload(0).result().message().find("sctp"), std::string::npos) << response.payload(0).result().message();
}

// ---------------------------------------------------------------------------
// Framing - RFC 6587 octet counting is the default; the LF-terminated
// non-transparent framing exists for older TCP receivers only and is not
// allowed over TLS (RFC 5425 mandates octet counting).
// ---------------------------------------------------------------------------

TEST(SyslogFraming, OctetCountingPrefixesTheByteCount) {
  EXPECT_EQ(syslog_client::frame_message("octet-counted", "hello"), "5 hello");
  EXPECT_EQ(syslog_client::frame_message("octet-counted", "<13>Jan  1 00:00:00 host tag msg"), "32 <13>Jan  1 00:00:00 host tag msg");
  EXPECT_EQ(syslog_client::frame_message("octet-counted", ""), "0 ");
}

TEST(SyslogFraming, NonTransparentFramingTerminatesWithLf) {
  // Safe only because submit() has already replaced every control byte: the
  // message body cannot contain the LF trailer.
  EXPECT_EQ(syslog_client::frame_message("non-transparent", "hello"), "hello\n");
}

TEST(SyslogFraming, DefaultsToOctetCountingOnStreamTransports) {
  const syslog_client::connection_data con(target_with({{"address", "h"}, {"transport", "tcp"}}), client::destination_container());

  EXPECT_EQ(con.framing, "octet-counted");
  EXPECT_TRUE(con.config_error.empty()) << con.config_error;
}

TEST(SyslogFraming, NonTransparentOverTlsIsAConfigError) {
  const syslog_client::connection_data con(target_with({{"address", "h"}, {"transport", "tls"}, {"framing", "non-transparent"}}),
                                           client::destination_container());

  EXPECT_FALSE(con.config_error.empty());
  EXPECT_NE(con.config_error.find("5425"), std::string::npos) << con.config_error;
}

TEST(SyslogFraming, AnUnknownFramingIsAConfigError) {
  const syslog_client::connection_data con(target_with({{"address", "h"}, {"transport", "tcp"}, {"framing", "chunked"}}), client::destination_container());

  EXPECT_FALSE(con.config_error.empty());
}

// ---------------------------------------------------------------------------
// TCP - the frames are read back from a real loopback acceptor, same
// philosophy as the UDP sink: the wire format is the contract.
// ---------------------------------------------------------------------------

namespace {

using boost::asio::ip::tcp;

// A loopback TCP acceptor; read_connection() collects one connection's bytes
// until EOF. The client connects, writes and closes before we read - the
// kernel backlog and buffers hold everything - so no thread is needed.
class syslog_stream_sink {
 public:
  syslog_stream_sink() : acceptor_(io_, tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), 0)) { acceptor_.non_blocking(true); }

  unsigned short port() const { return acceptor_.local_endpoint().port(); }

  std::string read_connection() {
    tcp::socket socket(io_);
    boost::system::error_code ec = boost::asio::error::would_block;
    for (int attempt = 0; attempt < 200 && ec; ++attempt) {
      acceptor_.accept(socket, ec);
      if (ec) pause();
    }
    if (ec) return "";
    socket.non_blocking(true);
    std::string out;
    for (int attempt = 0; attempt < 200; ++attempt) {
      char buffer[4096];
      const std::size_t len = socket.read_some(boost::asio::buffer(buffer), ec);
      if (!ec) {
        out.append(buffer, len);
        continue;
      }
      if (ec == boost::asio::error::would_block) {
        pause();
        continue;
      }
      break;  // EOF (or a real error, in which case `out` shows what arrived)
    }
    return out;
  }

 private:
  void pause() {
    boost::asio::steady_timer timer(io_, boost::asio::chrono::milliseconds(5));
    timer.wait();
  }
  boost::asio::io_context io_;
  tcp::acceptor acceptor_;
};

// Split an RFC 6587 octet-counted stream back into messages. Returns what it
// can parse; a framing bug shows up as a wrong element count or a mangled
// tail element in the failing assertion.
std::vector<std::string> parse_octet_counted(const std::string &stream) {
  std::vector<std::string> out;
  std::string::size_type pos = 0;
  while (pos < stream.size()) {
    const std::string::size_type sp = stream.find(' ', pos);
    if (sp == std::string::npos) {
      out.push_back(stream.substr(pos));
      break;
    }
    int len = -1;
    try {
      len = std::stoi(stream.substr(pos, sp - pos));
    } catch (...) {
    }
    if (len < 0 || sp + 1 + len > stream.size()) {
      out.push_back(stream.substr(pos));
      break;
    }
    out.push_back(stream.substr(sp + 1, len));
    pos = sp + 1 + len;
  }
  return out;
}

// Submit a batch of check results to a stream target and hand back the
// response payload.
PB::Commands::SubmitResponseMessage submit_stream(std::map<std::string, std::string> options, const std::vector<std::string> &messages,
                                                  syslog_client::syslog_client_handler *custom_handler = nullptr) {
  if (options.find("facility") == options.end()) options["facility"] = "kernel";
  if (options.find("severity") == options.end()) options["severity"] = "error";
  if (options.find("tag template") == options.end()) options["tag template"] = "nscp";
  if (options.find("message template") == options.end()) options["message template"] = "%message%";

  PB::Commands::SubmitRequestMessage request;
  int i = 0;
  for (const std::string &message : messages) {
    PB::Commands::QueryResponseMessage::Response *payload = request.add_payload();
    payload->set_command("check_" + std::to_string(i++));
    payload->set_result(PB::Common::ResultCode::OK);
    payload->add_lines()->set_message(message);
  }

  PB::Commands::SubmitResponseMessage response;
  syslog_client::syslog_client_handler local_handler;
  syslog_client::syslog_client_handler *handler = custom_handler ? custom_handler : &local_handler;
  handler->submit(client::destination_container(), target_with(options), request, response);
  return response;
}

bool submission_ok(const PB::Commands::SubmitResponseMessage &response) {
  return response.payload_size() == 1 && response.payload(0).result().code() == PB::Common::Result_StatusCodeType_STATUS_OK;
}

std::string submission_message(const PB::Commands::SubmitResponseMessage &response) {
  return response.payload_size() == 1 ? response.payload(0).result().message() : "<no payload>";
}

}  // namespace

TEST(SyslogTcp, DeliversOctetCountedFramesOverOneConnection) {
  syslog_stream_sink sink;

  const PB::Commands::SubmitResponseMessage response =
      submit_stream({{"address", "127.0.0.1"}, {"port", std::to_string(sink.port())}, {"transport", "tcp"}}, {"first result", "second result", "third result"});

  EXPECT_TRUE(submission_ok(response)) << submission_message(response);
  const std::vector<std::string> messages = parse_octet_counted(sink.read_connection());
  ASSERT_EQ(messages.size(), 3u);
  EXPECT_NE(messages[0].find("first result"), std::string::npos) << messages[0];
  EXPECT_NE(messages[2].find("third result"), std::string::npos) << messages[2];
  // Each frame is one full syslog line: priority through message.
  EXPECT_EQ(messages[0].find('<'), 0u) << messages[0];
}

TEST(SyslogTcp, NonTransparentFramingTerminatesEachMessageWithLf) {
  syslog_stream_sink sink;

  const PB::Commands::SubmitResponseMessage response = submit_stream(
      {{"address", "127.0.0.1"}, {"port", std::to_string(sink.port())}, {"transport", "tcp"}, {"framing", "non-transparent"}}, {"first", "second"});

  EXPECT_TRUE(submission_ok(response)) << submission_message(response);
  const std::string stream = sink.read_connection();
  ASSERT_FALSE(stream.empty());
  EXPECT_EQ(stream[stream.size() - 1], '\n');
  EXPECT_EQ(std::count(stream.begin(), stream.end(), '\n'), 2);
}

TEST(SyslogTcp, AConnectionFailureFailsTheSubmission) {
  // Reserve an ephemeral port, then close it so nobody is listening. UDP
  // would report success here ("presumably"); a stream transport must not.
  unsigned short dead_port;
  {
    boost::asio::io_context io;
    tcp::acceptor acceptor(io, tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), 0));
    dead_port = acceptor.local_endpoint().port();
  }

  const PB::Commands::SubmitResponseMessage response = submit_stream(
      {{"address", "127.0.0.1"}, {"port", std::to_string(dead_port)}, {"transport", "tcp"}, {"retry", "0"}, {"timeout", "5"}}, {"nobody hears this"});

  ASSERT_EQ(response.payload_size(), 1);
  EXPECT_EQ(response.payload(0).result().code(), PB::Common::Result_StatusCodeType_STATUS_ERROR) << submission_message(response);
}

#ifdef USE_SSL

// ---------------------------------------------------------------------------
// TLS - a real loopback TLS server (RFC 5425). The positive path proves the
// handshake, hostname pinning and octet-counted framing end to end; the
// negative path proves verification is actually enforced - the client fails
// closed instead of delivering to a server it cannot authenticate.
// ---------------------------------------------------------------------------

namespace {

// A self-signed localhost certificate for the loopback TLS tests ONLY (CN
// localhost, SANs DNS:localhost + IP:127.0.0.1 + IP:::1, valid ~100 years).
// The private key is public by design - never deploy it anywhere.
const char kTestServerCertPem[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDOjCCAiKgAwIBAgIUYtqXJskiVfy59YX0DNt49BpobUIwDQYJKoZIhvcNAQEL\n"
    "BQAwFDESMBAGA1UEAwwJbG9jYWxob3N0MCAXDTI2MDgzMDA4MTUwMVoYDzIxMjYw\n"
    "ODA2MDgxNTAxWjAUMRIwEAYDVQQDDAlsb2NhbGhvc3QwggEiMA0GCSqGSIb3DQEB\n"
    "AQUAA4IBDwAwggEKAoIBAQDfk2CGuPjzWpcBcJT+KMC0i/tnMYVTj3UGZ0mmLoFy\n"
    "CZNlir2y4YMCCLmOCQCV0yi6vmjxb7mx3QcziUy9fmXuh1/GshiSzWRnBJsW3Zdu\n"
    "93P4Ujknha2lKv0RvvGraZsnmgrqSunr/T5nGKM0urY8lChLvnkvxTo8yMoh8ptD\n"
    "++lsth1XZx9CXTso+s2D2adOaqdmWZqR5JUQdXYpXI7ee5caq8E4q5L9rinJGTGB\n"
    "SYGqLJW22/WKXmjhls64y5OYEkOJyZGRKaT1KpBjmoBSvp7qoUM2HDg/nl543U0J\n"
    "aJtM1UnTzsLT3vuFTCnp7xDuzwI1WCX6Abq4Asih55UTAgMBAAGjgYEwfzAdBgNV\n"
    "HQ4EFgQUzlaV0GvjmpPw4IAMD4pCg0dGZIwwHwYDVR0jBBgwFoAUzlaV0GvjmpPw\n"
    "4IAMD4pCg0dGZIwwDwYDVR0TAQH/BAUwAwEB/zAsBgNVHREEJTAjgglsb2NhbGhv\n"
    "c3SHBH8AAAGHEAAAAAAAAAAAAAAAAAAAAAEwDQYJKoZIhvcNAQELBQADggEBAMqj\n"
    "gCwbOsN0jJAamHVH/H+ZnPuGejzlbWvMpCWlJT4g9noHnCZoj5S7SUgoEQjR87hv\n"
    "E4cOHC7KabtYWzAkw9Jg8mI9PUg3ByiDJf74aLuRZFOPKOnmxWC1V7h76CFIfqx1\n"
    "iT6GZ2i0ssZ/ASE4lrhfAFSTVqDinJG04f51Ik7g5J/5sdGo5xlb3WzN+CyKI25o\n"
    "xqghklwR5tc/y8JTsJ3/unYTcYofNJfEJZUDxwr+bU4FQYzijUuh9wkN33eb31Wn\n"
    "u1vOXuMtz0ecEvfksfh63AWWYvG+gTghLrtdL5sP79huarMdo/bFdNg8digShE/e\n"
    "oVCoe+72XJxzn76s4Qc=\n"
    "-----END CERTIFICATE-----\n";

const char kTestServerKeyPem[] =
    "-----BEGIN PRIVATE KEY-----\n"
    "MIIEvwIBADANBgkqhkiG9w0BAQEFAASCBKkwggSlAgEAAoIBAQDfk2CGuPjzWpcB\n"
    "cJT+KMC0i/tnMYVTj3UGZ0mmLoFyCZNlir2y4YMCCLmOCQCV0yi6vmjxb7mx3Qcz\n"
    "iUy9fmXuh1/GshiSzWRnBJsW3Zdu93P4Ujknha2lKv0RvvGraZsnmgrqSunr/T5n\n"
    "GKM0urY8lChLvnkvxTo8yMoh8ptD++lsth1XZx9CXTso+s2D2adOaqdmWZqR5JUQ\n"
    "dXYpXI7ee5caq8E4q5L9rinJGTGBSYGqLJW22/WKXmjhls64y5OYEkOJyZGRKaT1\n"
    "KpBjmoBSvp7qoUM2HDg/nl543U0JaJtM1UnTzsLT3vuFTCnp7xDuzwI1WCX6Abq4\n"
    "Asih55UTAgMBAAECggEABZdx3z3FEG3VlP6z9NbKfBdaBmqMYRZ9z7TrvKAgEKmr\n"
    "2NniyTampiQbllOKgwGOsgRq+AZR14s2rbGODJUSKWykygouPQUlOY7kVMARerND\n"
    "PlBkZUcuvplLLFelh6sF89Zm1iaUbP7rfY2pH9DhfizE0X8S9m+PIj7YIXbSpjbY\n"
    "b1RUKhiZc//WLgjCWJrT0JyPahsxSUklh+vOA2nkCtogkbdpYX+BQkpns2NZy8X2\n"
    "KKOryWYr51/8Or/eHpW5PafUZLGmW04RonIrMeQPaFgcaN0Q9AwGtkwDMhX3aUnS\n"
    "w/MzzsxpmGVcpUpzQvVA8yay9p9JQLVMtaZtsF/rUQKBgQD4tlP/wPMZxxUYChlB\n"
    "T9jlnzbWAJfSuapdfrm/fEhF7Z1wVDVp97aYhxH9dbOsreQ8hXMnuYcWoiht/K3x\n"
    "vDMTU1h+CXDe8TxH7y4TpEtAFEtV+gPiaG+gb72zJknb8e+mCHZ6Bw2kWBqYuwTJ\n"
    "pZbHlKySsC66WpsRl0ullisxxwKBgQDmIH3ZnIoqiiL/0mP0vTjbDar5RvILHhb1\n"
    "Pe8D9EteS5iVK+HgKPlhDsRtz4Pc9+sp6jEAfe56yh2NQvCEGRZ6HjL265Z3f+UC\n"
    "+sWGK/G7YXLlfqjhmRA5vMBYUfUFtcxqa+amARLbjRGhFFkoUdeacX4MEAD6K3H5\n"
    "56Tkt2+CVQKBgQCV1mq2y0zqO8lWiUb71RoMBKapuQO+vYKI/z7ywPJdxrCyKtlf\n"
    "NRC5xa4t8ApGPyEg4RkmVpyvIxSOZst4tnRi8TSAAjoQ5m71u4Ab70Ayo8dbTdun\n"
    "PFn74zX7R1b5/kDt221dSQCMAVRMrWdOAMfdB5IMcb5FIS8JIwg9A0KUowKBgQDO\n"
    "PSRMmDxDL5V6S6WFOufveWpXCTv2trrAgwboNlItafaPCcreyBnm4AMunqGAsTcy\n"
    "U04jMLk/lk+xv+IpoQpB2zq1jCFHa3lkmDXZrxxvYEBGaKMVu8WM2RpQjPLYcTuH\n"
    "zLfi4mdyGBVKlR/qQJ7HLRtX2CfoDHOTEgT3bWYkGQKBgQCi//oyr3yeIQ6i2dSG\n"
    "QEvi67e+R2vnjBCu0PSM+DXa1w3Z+JaGWdFgrI0USdqL6xviRfJowPH11/cebKpN\n"
    "p8b9CnZNCCXeS17koVKGjMr2hQllka3Hfc5vqsYBdmZpzlRiBKDInSMfhrxxL/Ut\n"
    "J6QHZvmyJqLV7/LvOuLYhAdGtg==\n"
    "-----END PRIVATE KEY-----\n";

// An unrelated self-signed certificate (CN wrong-ca) that did NOT issue the
// server certificate above - the trust anchor for the fail-closed test.
const char kUnrelatedCaPem[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDCTCCAfGgAwIBAgIUK5TQ/uoxgT61xOk8OJR8zVNYHx8wDQYJKoZIhvcNAQEL\n"
    "BQAwEzERMA8GA1UEAwwId3JvbmctY2EwIBcNMjYwODMwMDgxNjQxWhgPMjEyNjA4\n"
    "MDYwODE2NDFaMBMxETAPBgNVBAMMCHdyb25nLWNhMIIBIjANBgkqhkiG9w0BAQEF\n"
    "AAOCAQ8AMIIBCgKCAQEAwMA+0nhax/J4D++PD4YUDFcSYjRTLV/coOa4A6xQe451\n"
    "s0pgR1wZAmKT5t3K1tFgnNiPbbAsA5bTihZnAkYOe0RNI/GVN/lpT+nJDsV0F/zS\n"
    "HUhpD/2vfhW6AmtgEmMUaNr10fSwW4EGMWxuv3u7V2RDBtTD4QG/9ZY4yeaAcTEU\n"
    "IpuK54+ReBfl/NJb3JOhnMKv9QmoqkYOjDzKVUpuwCrUzb8rvr3nMV82pFbLyMWi\n"
    "LwWH/TAUzZ34l+yVUG8KsBFOw7PJ0atkqBsrdCm37cu7CJnP7rE+x6e84h7MHh5Z\n"
    "7ynwuz3XwRfylqJ5p3/OJ7zQxnkZrYb5rnhbXD2tTwIDAQABo1MwUTAdBgNVHQ4E\n"
    "FgQUW09SfJTxticbXft3C0G+iIrn9wYwHwYDVR0jBBgwFoAUW09SfJTxticbXft3\n"
    "C0G+iIrn9wYwDwYDVR0TAQH/BAUwAwEB/zANBgkqhkiG9w0BAQsFAAOCAQEAHOuv\n"
    "xmukzjE4gsxKXRgPogVHdMVcSwPqC1qqPNX5gxG9XBL/Mj+KVBi/etFIoqYunWap\n"
    "+J2pmvxuFTOdL8vjf1nDfCU8/F6+WSA0x1JR6sSd+q8HnikfT+tw8TBI7euEIHgy\n"
    "sS71FBPRTqbkv09pGIgdu48PaEakn3V2h8uOWmeWgO6NzU1wCubqpO4F9W3hU1kt\n"
    "kv8yupNiT2zNItjjwtZQvVdu3wFsa3Nbf8ZQXTcCEWlvpOF6HTZwkymcvcQWxtNW\n"
    "q/vLOFuGuS9CbEWKSm5Wd3y1deEgWCiqWSIWygUXd2vSgYDim/uLRJ3RHGTskLTi\n"
    "hON3OqKzvERavl/piA==\n"
    "-----END CERTIFICATE-----\n";

// Write `pem` into the test's temp dir so the client's `ca` option (a file
// path fed to SSL_CTX_load_verify_locations) can point at it.
std::string write_pem(const std::string &name, const char *pem) {
  const std::string path = ::testing::TempDir() + name;
  std::ofstream out(path.c_str(), std::ios::binary | std::ios::trunc);
  out << pem;
  out.close();
  return path;
}

// A loopback RFC 5425 receiver: accepts TLS connections on a background
// thread (the handshake needs both ends live, unlike the plain-TCP sink),
// records each connection's plaintext bytes, and counts failed handshakes.
class syslog_tls_sink {
 public:
  syslog_tls_sink() : context_(boost::asio::ssl::context::sslv23_server), acceptor_(io_, tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), 0)) {
    context_.use_certificate_chain(boost::asio::buffer(kTestServerCertPem, sizeof(kTestServerCertPem) - 1));
    context_.use_private_key(boost::asio::buffer(kTestServerKeyPem, sizeof(kTestServerKeyPem) - 1), boost::asio::ssl::context::pem);
    thread_ = std::thread([this] { run(); });
  }

  ~syslog_tls_sink() {
    // Closing the acceptor does not wake a thread blocked in a synchronous
    // accept(2); a throwaway connection does, and the stop flag tells the
    // thread not to treat it as a session.
    {
      std::lock_guard<std::mutex> lock(mutex_);
      stopping_ = true;
    }
    try {
      boost::asio::io_context io;
      tcp::socket wakeup(io);
      wakeup.connect(tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), port()));
    } catch (...) {
    }
    if (thread_.joinable()) thread_.join();
  }

  unsigned short port() const { return acceptor_.local_endpoint().port(); }

  std::string received() {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_;
  }

  int handshake_failures() {
    std::lock_guard<std::mutex> lock(mutex_);
    return handshake_failures_;
  }

  // Poll until the recorded plaintext satisfies `predicate` (or ~1s passes);
  // the client has already returned from submit() when this is called, so
  // this only bridges the handoff to the server thread.
  std::string wait_for(const std::function<bool(const std::string &)> &predicate) {
    for (int attempt = 0; attempt < 200; ++attempt) {
      const std::string snapshot = received();
      if (predicate(snapshot)) return snapshot;
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return received();
  }

 private:
  bool stopping() {
    std::lock_guard<std::mutex> lock(mutex_);
    return stopping_;
  }

  void run() {
    for (;;) {
      boost::asio::ssl::stream<tcp::socket> stream(io_, context_);
      boost::system::error_code ec;
      acceptor_.accept(stream.lowest_layer(), ec);
      if (ec || stopping()) return;  // test over
      stream.handshake(boost::asio::ssl::stream_base::server, ec);
      if (ec) {
        std::lock_guard<std::mutex> lock(mutex_);
        ++handshake_failures_;
        continue;
      }
      for (;;) {
        char buffer[4096];
        const std::size_t len = stream.read_some(boost::asio::buffer(buffer), ec);
        if (len > 0) {
          std::lock_guard<std::mutex> lock(mutex_);
          data_.append(buffer, len);
        }
        if (ec) break;  // EOF / shutdown
      }
    }
  }

  boost::asio::io_context io_;
  boost::asio::ssl::context context_;
  tcp::acceptor acceptor_;
  std::thread thread_;
  std::mutex mutex_;
  std::string data_;
  int handshake_failures_ = 0;
  bool stopping_ = false;
};

}  // namespace

TEST(SyslogTls, DeliversOctetCountedFramesWhenTheServerVerifies) {
  syslog_tls_sink sink;

  const PB::Commands::SubmitResponseMessage response = submit_stream({{"address", "127.0.0.1"},
                                                                      {"port", std::to_string(sink.port())},
                                                                      {"transport", "tls"},
                                                                      {"ca", write_pem("syslog_tls_ca.pem", kTestServerCertPem)},
                                                                      {"retry", "0"},
                                                                      {"timeout", "10"}},
                                                                     {"secure result"});

  EXPECT_TRUE(submission_ok(response)) << submission_message(response);
  const std::string plaintext = sink.wait_for([](const std::string &s) { return s.find("secure result") != std::string::npos; });
  const std::vector<std::string> messages = parse_octet_counted(plaintext);
  ASSERT_EQ(messages.size(), 1u) << plaintext;
  EXPECT_NE(messages[0].find("secure result"), std::string::npos) << messages[0];
}

TEST(SyslogTls, FailsClosedWhenTheServerCertIsNotTrusted) {
  // The trust anchor is an unrelated certificate: chain verification must
  // fail, the submission must report failure, and no plaintext may reach the
  // server - failing open here would silently deliver to an impersonator.
  syslog_tls_sink sink;

  const PB::Commands::SubmitResponseMessage response = submit_stream({{"address", "127.0.0.1"},
                                                                      {"port", std::to_string(sink.port())},
                                                                      {"transport", "tls"},
                                                                      {"ca", write_pem("syslog_tls_wrong_ca.pem", kUnrelatedCaPem)},
                                                                      {"retry", "0"},
                                                                      {"timeout", "10"}},
                                                                     {"must not arrive"});

  ASSERT_EQ(response.payload_size(), 1);
  EXPECT_EQ(response.payload(0).result().code(), PB::Common::Result_StatusCodeType_STATUS_ERROR) << submission_message(response);
  // The server saw the aborted handshake, and never any plaintext.
  sink.wait_for([&sink](const std::string &) { return sink.handshake_failures() > 0; });
  EXPECT_EQ(sink.received(), "");
}

TEST(SyslogTls, VerificationFailsWithoutAnyConfiguredTrustAnchor) {
  // With `ca` unset and no module-level default the SSL context holds no
  // trust anchors at all; peer verification must fail closed rather than
  // fall through to accepting anything.
  syslog_tls_sink sink;

  const PB::Commands::SubmitResponseMessage response = submit_stream(
      {{"address", "127.0.0.1"}, {"port", std::to_string(sink.port())}, {"transport", "tls"}, {"retry", "0"}, {"timeout", "10"}}, {"must not arrive"});

  ASSERT_EQ(response.payload_size(), 1);
  EXPECT_EQ(response.payload(0).result().code(), PB::Common::Result_StatusCodeType_STATUS_ERROR) << submission_message(response);
}

TEST(SyslogTls, InsecureIsAnExplicitOptOut) {
  // `insecure = true` (or `verify mode = none`) delivers without verifying -
  // an explicit operator decision, never a fallback.
  syslog_tls_sink sink;

  const PB::Commands::SubmitResponseMessage response = submit_stream(
      {{"address", "127.0.0.1"}, {"port", std::to_string(sink.port())}, {"transport", "tls"}, {"insecure", "true"}, {"retry", "0"}, {"timeout", "10"}},
      {"explicitly unverified"});

  EXPECT_TRUE(submission_ok(response)) << submission_message(response);
  const std::string plaintext = sink.wait_for([](const std::string &s) { return s.find("explicitly unverified") != std::string::npos; });
  EXPECT_NE(plaintext.find("explicitly unverified"), std::string::npos) << plaintext;
}

TEST(SyslogTls, TheModuleLevelCaFallbackFillsAnUnsetCa) {
  // A command-line submission arrives with `ca` unset; the handler's
  // default_ca (resolved from ${ca-path} at module load) must be used then -
  // and only then, an explicit `ca` wins.
  syslog_tls_sink sink;

  syslog_client::syslog_client_handler handler;
  handler.default_ca = write_pem("syslog_tls_default_ca.pem", kTestServerCertPem);

  const PB::Commands::SubmitResponseMessage response = submit_stream(
      {{"address", "127.0.0.1"}, {"port", std::to_string(sink.port())}, {"transport", "tls"}, {"retry", "0"}, {"timeout", "10"}}, {"via default ca"}, &handler);

  EXPECT_TRUE(submission_ok(response)) << submission_message(response);
  const std::string plaintext = sink.wait_for([](const std::string &s) { return s.find("via default ca") != std::string::npos; });
  EXPECT_NE(plaintext.find("via default ca"), std::string::npos) << plaintext;
}

#endif  // USE_SSL

// ---------------------------------------------------------------------------
// The new command-line options land on the keys connection_data reads.
// ---------------------------------------------------------------------------

TEST(SyslogOptions, TransportAndTlsOptionsReachTheConnection) {
  syslog_handler::options_reader_impl reader;
  po::options_description desc;
  client::destination_container source, data;
  reader.process(desc, source, data);

  // `--insecure true` (a *valued* boolean) is the REST calling convention -
  // po::bool_switch would reject it, see the project conventions.
  const char *argv[] = {"test", "--transport", "tls", "--framing", "octet-counted", "--ca", "/tmp/ca.pem", "--insecure", "true"};
  po::variables_map vm;
  po::store(po::parse_command_line(static_cast<int>(std::size(argv)), const_cast<char **>(argv), desc), vm);
  po::notify(vm);

  data.set_string_data("address", "h");
  const syslog_client::connection_data con(data, client::destination_container());

  EXPECT_EQ(con.transport, "tls");
  EXPECT_EQ(con.framing, "octet-counted");
  EXPECT_EQ(con.ssl.ca_path, "/tmp/ca.pem");
  EXPECT_EQ(con.ssl.verify_mode, "none") << "insecure did not take effect";
}

TEST(SyslogOptions, TheValuedSslBooleanSelectsTls) {
  // `ssl=true` over REST arrives as a single valued token; the shared
  // --ssl option is a po::value<bool>, so this must parse and select TLS.
  syslog_handler::options_reader_impl reader;
  po::options_description desc;
  client::destination_container source, data;
  reader.process(desc, source, data);

  const char *argv[] = {"test", "--ssl", "true"};
  po::variables_map vm;
  po::store(po::parse_command_line(static_cast<int>(std::size(argv)), const_cast<char **>(argv), desc), vm);
  po::notify(vm);

  data.set_string_data("address", "h");
  const syslog_client::connection_data con(data, client::destination_container());

  EXPECT_EQ(con.transport, "tls");
}
