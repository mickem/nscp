// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_nt_commands.hpp"

#include <gtest/gtest.h>

#include <nscapi/nscapi_helper_singleton.hpp>

#include <algorithm>
#include <string>
#include <vector>

// This test binary has no generated module glue, so it must define the plugin
// singleton itself (normally provided by NSC_WRAP_DLL()).
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

using namespace check_nt_commands;

namespace {

std::set<int> parse(const std::string &spec) {
  std::set<std::string> unknown;
  std::set<int> out = parse_allowed_commands(spec, unknown);
  EXPECT_TRUE(unknown.empty()) << "unexpected unknown tokens parsing: " << spec;
  return out;
}

std::vector<std::string> args_of(int code, const std::string &raw) {
  mapped_command out;
  EXPECT_TRUE(map_request(code, raw, out));
  return std::vector<std::string>(out.arguments.begin(), out.arguments.end());
}

bool has_arg(const std::vector<std::string> &args, const std::string &arg) { return std::find(args.begin(), args.end(), arg) != args.end(); }

// A single-line response carrying one byte-unit perf entry, the shape the
// perf-config in the request mappings (`used(unit:B)free(unit:B)`) produces.
PB::Commands::QueryResponseMessage::Response make_perf_response(double value, double maximum) {
  PB::Commands::QueryResponseMessage::Response payload;
  payload.set_result(PB::Common::ResultCode::OK);
  auto *line = payload.add_lines();
  line->set_message("message text");
  auto *perf = line->add_perf();
  perf->set_alias("value");
  auto *fv = perf->mutable_float_value();
  fv->set_value(value);
  fv->set_unit("B");
  fv->mutable_maximum()->set_value(maximum);
  return payload;
}

PB::Commands::QueryResponseMessage::Response make_message_only_response(PB::Common::ResultCode result, const std::string &message) {
  PB::Commands::QueryResponseMessage::Response payload;
  payload.set_result(result);
  payload.add_lines()->set_message(message);
  return payload;
}

}  // namespace

// =============================================================================
// parse_allowed_commands — the `allow` setting
// =============================================================================

TEST(CheckNtAllow, AnyAndAllEnableEveryCommand) {
  const std::set<int> everything = {REQ_CLIENTVERSION, REQ_CPULOAD, REQ_UPTIME,  REQ_USEDDISKSPACE, REQ_SERVICESTATE,
                                    REQ_PROCSTATE,     REQ_MEMUSE,  REQ_COUNTER, REQ_FILEAGE,       REQ_INSTANCES};
  EXPECT_EQ(parse("any"), everything);
  EXPECT_EQ(parse("all"), everything);
}

TEST(CheckNtAllow, GroupsExpandToTheirDocumentedCodes) {
  EXPECT_EQ(parse("metrics"), (std::set<int>{REQ_CPULOAD, REQ_UPTIME, REQ_USEDDISKSPACE, REQ_MEMUSE}));
  EXPECT_EQ(parse("info"), (std::set<int>{REQ_CLIENTVERSION}));
  EXPECT_EQ(parse("service"), (std::set<int>{REQ_SERVICESTATE}));
  EXPECT_EQ(parse("process"), (std::set<int>{REQ_PROCSTATE}));
  EXPECT_EQ(parse("counters"), (std::set<int>{REQ_COUNTER, REQ_INSTANCES}));
  EXPECT_EQ(parse("files"), (std::set<int>{REQ_FILEAGE}));
}

TEST(CheckNtAllow, MetricsDoesNotEnableTheArbitraryReadCommands) {
  const std::set<int> metrics = parse("metrics, info");
  EXPECT_EQ(metrics.count(REQ_COUNTER), 0u);
  EXPECT_EQ(metrics.count(REQ_INSTANCES), 0u);
  EXPECT_EQ(metrics.count(REQ_FILEAGE), 0u);
  EXPECT_EQ(metrics.count(REQ_SERVICESTATE), 0u);
  EXPECT_EQ(metrics.count(REQ_PROCSTATE), 0u);
}

TEST(CheckNtAllow, IndividualCommandNamesMapToTheirCode) {
  EXPECT_EQ(parse("clientversion"), (std::set<int>{REQ_CLIENTVERSION}));
  EXPECT_EQ(parse("cpuload"), (std::set<int>{REQ_CPULOAD}));
  EXPECT_EQ(parse("uptime"), (std::set<int>{REQ_UPTIME}));
  EXPECT_EQ(parse("useddiskspace"), (std::set<int>{REQ_USEDDISKSPACE}));
  EXPECT_EQ(parse("servicestate"), (std::set<int>{REQ_SERVICESTATE}));
  EXPECT_EQ(parse("procstate"), (std::set<int>{REQ_PROCSTATE}));
  EXPECT_EQ(parse("memuse"), (std::set<int>{REQ_MEMUSE}));
  EXPECT_EQ(parse("counter"), (std::set<int>{REQ_COUNTER}));
  EXPECT_EQ(parse("fileage"), (std::set<int>{REQ_FILEAGE}));
  EXPECT_EQ(parse("instances"), (std::set<int>{REQ_INSTANCES}));
}

TEST(CheckNtAllow, TokensAreCaseInsensitiveAndTrimmed) {
  EXPECT_EQ(parse("  Metrics ,  INFO  "), parse("metrics,info"));
  EXPECT_EQ(parse("UpTime"), (std::set<int>{REQ_UPTIME}));
}

TEST(CheckNtAllow, UnknownTokensAreCollectedAndEnableNothing) {
  std::set<std::string> unknown;
  const std::set<int> out = parse_allowed_commands("bogus, uptime, ALSO-bad", unknown);
  EXPECT_EQ(out, (std::set<int>{REQ_UPTIME}));
  EXPECT_EQ(unknown, (std::set<std::string>{"bogus", "also-bad"}));
}

TEST(CheckNtAllow, OnlyUnknownTokensFailClosed) {
  std::set<std::string> unknown;
  EXPECT_TRUE(parse_allowed_commands("no-such-command", unknown).empty());
  EXPECT_EQ(unknown.size(), 1u);
}

TEST(CheckNtAllow, EmptySpecAndEmptyTokensEnableNothing) {
  std::set<std::string> unknown;
  EXPECT_TRUE(parse_allowed_commands("", unknown).empty());
  EXPECT_TRUE(parse_allowed_commands(" , ,", unknown).empty());
  EXPECT_TRUE(unknown.empty());  // empty tokens are skipped, not "unknown"
}

// =============================================================================
// map_request — request code -> modern query
// =============================================================================

TEST(CheckNtMapRequest, CpuLoadMapsEachValueToATimeWindow) {
  mapped_command out;
  ASSERT_TRUE(map_request(REQ_CPULOAD, "1", out));
  EXPECT_EQ(out.command, "check_cpu");
  EXPECT_EQ(out.arguments, (std::list<std::string>{"time=1m"}));

  // check_nt sends one window per request, but the wire format allows more.
  EXPECT_EQ(args_of(REQ_CPULOAD, "5&10"), (std::vector<std::string>{"time=5m", "time=10m"}));
}

TEST(CheckNtMapRequest, UptimeNeverAlerts) {
  mapped_command out;
  ASSERT_TRUE(map_request(REQ_UPTIME, "", out));
  EXPECT_EQ(out.command, "check_uptime");
  EXPECT_EQ(out.arguments, (std::list<std::string>{"warn=uptime<0"}));
}

TEST(CheckNtMapRequest, UsedDiskSpaceChecksTheRequestedFixedDrive) {
  mapped_command out;
  ASSERT_TRUE(map_request(REQ_USEDDISKSPACE, "c", out));
  EXPECT_EQ(out.command, "check_drivesize");
  const std::vector<std::string> args(out.arguments.begin(), out.arguments.end());
  EXPECT_EQ(args[0], "drive=c");
  EXPECT_TRUE(has_arg(args, "filter=type='fixed' and mounted = 1"));
  // The byte unit is what keeps extract_perf_* from scaling the values.
  EXPECT_TRUE(has_arg(args, "perf-config=used(unit:B)free(unit:B)"));
}

TEST(CheckNtMapRequest, ServiceStateStripsShowFailAndKeepsTheServiceList) {
  const std::vector<std::string> args = args_of(REQ_SERVICESTATE, "ShowFail&svc1&svc2");
  EXPECT_EQ(args[0], "service=svc1");
  EXPECT_EQ(args[1], "service=svc2");
  EXPECT_FALSE(has_arg(args, "top-syntax=${list}"));
  EXPECT_TRUE(has_arg(args, "detail-syntax=${name}: ${legacy_state}"));
  EXPECT_TRUE(has_arg(args, "crit=not state = 'running'"));
}

TEST(CheckNtMapRequest, ServiceStateShowAllListsEveryRequestedService) {
  const std::vector<std::string> args = args_of(REQ_SERVICESTATE, "ShowAll&svc1");
  EXPECT_EQ(args[0], "service=svc1");
  EXPECT_TRUE(has_arg(args, "top-syntax=${list}"));
}

TEST(CheckNtMapRequest, ProcStateMirrorsServiceStateForProcesses) {
  mapped_command out;
  ASSERT_TRUE(map_request(REQ_PROCSTATE, "ShowAll&nscp.exe", out));
  EXPECT_EQ(out.command, "check_process");
  const std::vector<std::string> args(out.arguments.begin(), out.arguments.end());
  EXPECT_EQ(args[0], "process=nscp.exe");
  EXPECT_TRUE(has_arg(args, "top-syntax=${list}"));
  EXPECT_TRUE(has_arg(args, "detail-syntax=${exe}: ${legacy_state}"));
}

TEST(CheckNtMapRequest, MemUseChecksCommittedMemory) {
  mapped_command out;
  ASSERT_TRUE(map_request(REQ_MEMUSE, "", out));
  EXPECT_EQ(out.command, "check_memory");
  const std::vector<std::string> args(out.arguments.begin(), out.arguments.end());
  EXPECT_TRUE(has_arg(args, "type=committed"));
  EXPECT_TRUE(has_arg(args, "perf-config=used(unit:B)free(unit:B)"));
}

TEST(CheckNtMapRequest, CounterPassesTheRawCounterPathThrough) {
  mapped_command out;
  ASSERT_TRUE(map_request(REQ_COUNTER, "\\Processor(_Total)\\% Processor Time", out));
  EXPECT_EQ(out.command, "check_pdh");
  EXPECT_EQ(out.arguments, (std::list<std::string>{"counter=\\Processor(_Total)\\% Processor Time"}));
}

TEST(CheckNtMapRequest, FileAgeChecksTheRequestedPath) {
  mapped_command out;
  ASSERT_TRUE(map_request(REQ_FILEAGE, "C:\\some\\file.txt", out));
  EXPECT_EQ(out.command, "check_files");
  const std::vector<std::string> args(out.arguments.begin(), out.arguments.end());
  EXPECT_EQ(args[0], "path=C:\\some\\file.txt");
  EXPECT_TRUE(has_arg(args, "crit=age<0"));
}

TEST(CheckNtMapRequest, InlineAndUnknownCodesAreNotMapped) {
  mapped_command out;
  EXPECT_FALSE(map_request(REQ_CLIENTVERSION, "", out));  // answered inline
  EXPECT_FALSE(map_request(REQ_INSTANCES, "Process", out));  // answered inline
  EXPECT_FALSE(map_request(0, "", out));
  EXPECT_FALSE(map_request(42, "", out));
}

// =============================================================================
// format_response — modern response -> legacy payload
// =============================================================================
//
// The value orderings below are check_nt wire contract: MEMUSE is parsed by
// the client as `<total>&<used>` while USEDDISKSPACE is `<free>&<total>` -
// swapping a pair would still "work" but report wrong numbers.

TEST(CheckNtFormatResponse, FirstPerfValueCommandsReturnTheValue) {
  const auto payload = make_perf_response(42, 100);
  EXPECT_EQ(format_response(REQ_CPULOAD, "check_cpu", payload), "42");
  EXPECT_EQ(format_response(REQ_UPTIME, "check_uptime", payload), "42");
  EXPECT_EQ(format_response(REQ_COUNTER, "check_pdh", payload), "42");
}

TEST(CheckNtFormatResponse, MemUseReturnsTotalThenUsed) {
  EXPECT_EQ(format_response(REQ_MEMUSE, "check_memory", make_perf_response(1024, 4096)), "4096&1024");
}

TEST(CheckNtFormatResponse, UsedDiskSpaceReturnsFreeThenTotal) {
  EXPECT_EQ(format_response(REQ_USEDDISKSPACE, "check_drivesize", make_perf_response(1024, 4096)), "1024&4096");
}

TEST(CheckNtFormatResponse, FileAgeReturnsWholeMinutesAndTheMessage) {
  // 3599 seconds is 59 minutes on the wire - the division truncates.
  EXPECT_EQ(format_response(REQ_FILEAGE, "check_files", make_perf_response(3599, 0)), "59&message text");
}

TEST(CheckNtFormatResponse, StateCommandsPrefixTheResultCode) {
  EXPECT_EQ(format_response(REQ_SERVICESTATE, "check_service", make_message_only_response(PB::Common::ResultCode::OK, "all good")), "0& all good");
  EXPECT_EQ(format_response(REQ_PROCSTATE, "check_process", make_message_only_response(PB::Common::ResultCode::CRITICAL, "notepad.exe: not running")),
            "2& notepad.exe: not running");
}

TEST(CheckNtFormatResponse, MissingPerformanceDataIsAnError) {
  const auto payload = make_message_only_response(PB::Common::ResultCode::OK, "no perf here");
  for (int code : {REQ_CPULOAD, REQ_UPTIME, REQ_COUNTER, REQ_MEMUSE, REQ_USEDDISKSPACE, REQ_FILEAGE}) {
    EXPECT_EQ(format_response(code, "some_command", payload), "ERROR: No performance data from command: some_command") << "code=" << code;
  }
}

TEST(CheckNtFormatResponse, WrongLineCountIsAnError) {
  PB::Commands::QueryResponseMessage::Response payload;
  payload.set_result(PB::Common::ResultCode::OK);
  EXPECT_EQ(format_response(REQ_UPTIME, "check_uptime", payload), "ERROR: Invalid number of lines returned from command: check_uptime, 0");
  payload.add_lines()->set_message("one");
  payload.add_lines()->set_message("two");
  EXPECT_EQ(format_response(REQ_UPTIME, "check_uptime", payload), "ERROR: Invalid number of lines returned from command: check_uptime, 2");
}

TEST(CheckNtFormatResponse, UnknownCodeIsAnError) {
  EXPECT_EQ(format_response(42, "whatever", make_perf_response(1, 2)), "ERROR: Unknown command whatever");
}
