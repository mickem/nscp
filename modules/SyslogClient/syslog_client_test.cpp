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

#include <boost/asio.hpp>
#include <map>
#include <string>
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
  EXPECT_EQ(con.timeout, 30);
  EXPECT_EQ(con.retry, 3);
}

TEST(SyslogConnectionData, TakesThePortFromTheTarget) {
  const syslog_client::connection_data con(target_with({{"address", "syslog.example.com"}, {"port", "1514"}}), client::destination_container());

  EXPECT_EQ(con.get_port(), "1514");
}

TEST(SyslogConnectionData, AConfiguredTimeoutDoesNotReachTheConnection) {
  // CHARACTERIZATION TEST - this pins current behaviour, which is wrong.
  //
  // connection_data reads `arguments.get_int_data("timeout", 30)`, which looks
  // in the container's free-form data map. But destination_container routes the
  // well-known keys "timeout" and "retry" into typed *fields* instead of that
  // map (set_string_data), so the lookup never finds them and the default wins
  // no matter what the operator configured.
  //
  // Harmless here - syslog is UDP and never uses the value for anything but a
  // debug line - but the same expression appears in NSCAClient, IcingaClient
  // and CollectdClient, where it is the socket timeout. Fixing it means
  // choosing what an unset timeout should be: the container defaults to 10,
  // these clients document 30.
  const syslog_client::connection_data con(target_with({{"address", "h"}, {"timeout", "5"}, {"retry", "1"}}), client::destination_container());

  EXPECT_EQ(con.timeout, 30) << "documenting the defect: the configured 5 was ignored";
  EXPECT_EQ(con.retry, 3) << "documenting the defect: the configured 1 was ignored";
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
