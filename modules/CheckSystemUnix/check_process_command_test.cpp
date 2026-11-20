// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// check_process as a command: its keyword registry over fabricated records,
// and the command body over the processes of the machine running the test.
// check_process_test.cpp covers the parsers underneath; nothing there builds
// the filter, so a keyword that stopped resolving or an option that stopped
// parsing would not have failed a unit test.
//
// The live cases look for the test binary itself - the one process that is
// certain to be running - and for a name that cannot exist, so the outcome
// does not depend on what else the host runs.

#include <gtest/gtest.h>
#include <unistd.h>

#include <memory>
#include <parsers/filter/cli_helper.hpp>
#include <string>
#include <vector>

#include "check_process.h"
#include "check_test_support.hpp"

using check_proc::check_proc_filter::enumerate_processes;
using check_proc::check_proc_filter::filter_obj;
using check_test::contains;
using check_test::has_perf_like;
using check_test::make_request;
using check_test::message_of;
using check_test::response_type;

namespace {

const char *const no_such_process = "nscp-no-such-process-b3f1";

// Render the given records through the check_process filter, with the
// thresholds and syntax passed as arguments the way a caller would.
response_type run_records(const std::vector<std::string> &args, const std::vector<filter_obj> &records) {
  const auto request = make_request("check_process", args);
  response_type response;
  modern_filter::data_container data;
  modern_filter::cli_helper<check_proc::check_proc_filter::filter> helper(request, &response, data);
  check_proc::check_proc_filter::filter filter;
  helper.add_options("", "", "", filter.get_filter_syntax(), "unknown");
  helper.add_syntax("${list}", "${exe}", "${exe}", "empty", "");
  if (!helper.parse_options()) return response;
  if (!helper.build_filter(filter)) return response;
  for (const filter_obj &r : records) filter.match(std::make_shared<filter_obj>(r));
  helper.post_process(filter);
  return response;
}

filter_obj sample_process() {
  filter_obj p("nginx");
  p.filename = "/usr/sbin/nginx";
  p.command_line = "nginx: master process";
  p.pid = 42;
  p.ppid = 1;
  p.uid = 1000;
  p.username = "alice";
  p.started = true;
  p.proc_state = 'S';
  p.virtual_size = 8 * 1024 * 1024;
  p.working_set = 4 * 1024 * 1024;
  p.peak_virtual_size = 16 * 1024 * 1024;
  p.peak_working_set = 6 * 1024 * 1024;
  p.page_faults = 7;
  p.user_time = 3;
  p.kernel_time = 2;
  p.total_time = 5;
  p.elapsed = 90;
  return p;
}

// The test binary as the kernel lists it.
filter_obj self() {
  const int pid = static_cast<int>(getpid());
  for (const filter_obj &p : enumerate_processes(true)) {
    if (p.pid == pid) return p;
  }
  return filter_obj();
}

response_type run_check(const std::vector<std::string> &args) {
  response_type response;
  check_proc::check_process(make_request("check_process", args), &response);
  return response;
}

}  // namespace

// --- the keyword registry ---------------------------------------------------

TEST(CheckProcessFilter, IdentityKeywordsRenderTheRecord) {
  const response_type r =
      run_records({"detail-syntax=${exe}|${filename}|${command_line}|${pid}|${ppid}|${uid}|${username}|${state}|${proc_state}"}, {sample_process()});
  EXPECT_EQ(PB::Common::ResultCode::OK, r.result()) << message_of(r);
  EXPECT_EQ("nginx|/usr/sbin/nginx|nginx: master process|42|1|1000|alice|started|sleeping", message_of(r));
}

TEST(CheckProcessFilter, CountersAreThresholdable) {
  const response_type warn = run_records({"warning=working_set > 1M", "critical=page_faults > 100"}, {sample_process()});
  EXPECT_EQ(PB::Common::ResultCode::WARNING, warn.result()) << message_of(warn);
  EXPECT_TRUE(has_perf_like(warn, "ws_size")) << warn.DebugString();

  const response_type crit = run_records({"warning=working_set > 1M", "critical=time >= 5"}, {sample_process()});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, crit.result()) << message_of(crit);
}

TEST(CheckProcessFilter, RssIsAnAliasForWorkingSet) {
  const response_type r = run_records({"critical=rss > 1M"}, {sample_process()});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.result()) << message_of(r);
}

TEST(CheckProcessFilter, StateNamesGoThroughTheConverters) {
  // 'running' is the documented synonym for 'started'.
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, run_records({"critical=state = 'running'"}, {sample_process()}).result());
  EXPECT_EQ(PB::Common::ResultCode::OK, run_records({"critical=state = 'stopped'"}, {sample_process()}).result());

  filter_obj zombie = sample_process();
  zombie.proc_state = 'Z';
  zombie.started = false;
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, run_records({"critical=proc_state = 'zombie'"}, {zombie}).result());
  EXPECT_EQ(PB::Common::ResultCode::OK, run_records({"critical=proc_state = 'zombie'"}, {sample_process()}).result());
}

TEST(CheckProcessFilter, FilterDropsRecordsAndLeavesTheEmptyState) {
  const response_type r = run_records({"filter=exe = 'apache'"}, {sample_process()});
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.result());
  EXPECT_EQ("empty", message_of(r));
}

TEST(CheckProcessFilter, UnreadableCountersRenderUnknownAndNeverTrip) {
  // macOS hands task info to a process's owner and root only. The counters
  // of such a process are not zero, they are not known: a threshold on them
  // must not fire and no perf data may be drawn from them.
  filter_obj hidden = sample_process();
  hidden.has_task_info = false;
  const response_type r =
      run_records({"detail-syntax=${working_set}|${virtual}|${peak_virtual}|${user}", "warning=working_set > 1M", "critical=working_set < 1M"}, {hidden});
  EXPECT_EQ(PB::Common::ResultCode::OK, r.result()) << message_of(r);
  EXPECT_EQ("unknown|unknown|unknown|unknown", message_of(r));
  EXPECT_FALSE(has_perf_like(r, "ws_size")) << r.DebugString();
}

TEST(CheckProcessFilter, MissingPeaksAreUnknownWhileTheRestRenders) {
  filter_obj no_peaks = sample_process();
  no_peaks.has_peaks = false;
  const response_type r = run_records({"detail-syntax=${peak_working_set}|${peak_virtual}|${page_faults}"}, {no_peaks});
  EXPECT_EQ("unknown|unknown|7", message_of(r));
}

TEST(CheckProcessFilter, AnUnknownKeywordIsRejected) {
  const response_type r = run_records({"warning=no_such_keyword > 1"}, {sample_process()});
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.result());
}

// --- the command, against this machine ---------------------------------------

TEST(CheckProcessCommand, EnumerationDescribesTheTestBinary) {
  const filter_obj me = self();
  ASSERT_EQ(static_cast<int>(getpid()), me.pid) << "the test process is missing from the process list";
  EXPECT_FALSE(me.exe.empty());
  EXPECT_TRUE(me.started);
  EXPECT_EQ(static_cast<int>(getppid()), me.ppid);
  EXPECT_EQ(static_cast<long long>(getuid()), me.uid);
  EXPECT_GT(me.working_set, 0u);
  EXPECT_GE(me.virtual_size, me.working_set);
  EXPECT_GT(me.creation_time, 0u);
  EXPECT_TRUE(contains(me.command_line, me.exe)) << me.command_line;
}

TEST(CheckProcessCommand, ARunningProcessIsOk) {
  const filter_obj me = self();
  ASSERT_FALSE(me.exe.empty());
  const response_type r = run_check({"process=" + me.exe});
  EXPECT_EQ(PB::Common::ResultCode::OK, r.result()) << message_of(r);
  EXPECT_EQ("OK: all processes are ok.", message_of(r));
}

TEST(CheckProcessCommand, AProcessThatIsNotRunningIsCritical) {
  const response_type r = run_check({std::string("process=") + no_such_process});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.result()) << message_of(r);
  EXPECT_EQ(std::string("CRITICAL: ") + no_such_process + "=stopped", message_of(r));
}

TEST(CheckProcessCommand, NamesMatchWithoutRegardToCase) {
  const filter_obj me = self();
  ASSERT_FALSE(me.exe.empty());
  std::string upper = me.exe;
  for (char &c : upper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  const response_type r = run_check({"process=" + upper});
  EXPECT_EQ(PB::Common::ResultCode::OK, r.result()) << message_of(r);
}

TEST(CheckProcessCommand, TotalAddsOneAggregateRow) {
  const filter_obj me = self();
  ASSERT_FALSE(me.exe.empty());
  // `total=true` as one token is how REST passes a flag.
  const response_type r = run_check({"process=" + me.exe, "total=true", "top-syntax=${list}", "detail-syntax=${exe}", "warning=none", "critical=none"});
  EXPECT_EQ(PB::Common::ResultCode::OK, r.result()) << message_of(r);
  EXPECT_TRUE(contains(message_of(r), me.exe)) << message_of(r);
  EXPECT_TRUE(contains(message_of(r), "total")) << message_of(r);
}

TEST(CheckProcessCommand, ResolveOwnerFillsInTheUsername) {
  const filter_obj me = self();
  ASSERT_FALSE(me.exe.empty());
  const std::vector<std::string> common = {"process=" + me.exe, "top-syntax=${list}", "detail-syntax=[${uid}:${username}]", "warning=none", "critical=none"};
  const std::string uid = std::to_string(getuid());

  const response_type plain = run_check(common);
  EXPECT_TRUE(contains(message_of(plain), "[" + uid + ":]")) << message_of(plain);

  if (!me.username.empty()) {
    std::vector<std::string> with_owner = common;
    with_owner.push_back("resolve-owner=true");
    const response_type resolved = run_check(with_owner);
    EXPECT_TRUE(contains(message_of(resolved), "[" + uid + ":" + me.username + "]")) << message_of(resolved);
  }
}

TEST(CheckProcessCommand, AFilterThatMatchesNothingIsUnknown) {
  const response_type r = run_check({"filter=exe = 'nscp-no-such-exe'"});
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.result());
  EXPECT_EQ("UNKNOWN: No processes found", message_of(r));
}

TEST(CheckProcessCommand, ABadOptionIsReportedNotIgnored) {
  const response_type r = run_check({"no-such-option=1"});
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.result());
}

TEST(CheckProcessCommand, FetchOnlyListsProcessesInPsSectionFormat) {
  const filter_obj me = self();
  ASSERT_FALSE(me.exe.empty());
  const response_type r = run_check({"fetch-only"});
  EXPECT_EQ(PB::Common::ResultCode::OK, r.result());
  // (user,vsz_kb,rss_kb,cputime,pid) command line
  EXPECT_TRUE(contains(message_of(r), "," + std::to_string(getpid()) + ") ")) << "own pid missing from the ps section";
}

TEST(CheckProcessCommand, DeltaReportsCpuAsAShareOfTheMachine) {
  const filter_obj me = self();
  ASSERT_FALSE(me.exe.empty());
  // One second between two samples: an idle test binary used next to nothing
  // of the machine in it, and never more than all of it.
  const response_type r =
      run_check({"process=" + me.exe, "delta=true", "top-syntax=${list}", "detail-syntax=${exe}", "warning=time > 100", "critical=time > 100"});
  EXPECT_EQ(PB::Common::ResultCode::OK, r.result()) << message_of(r);
  EXPECT_TRUE(contains(message_of(r), me.exe)) << message_of(r);
}
