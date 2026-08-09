// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_process.h"

#include <gtest/gtest.h>

using check_proc::check_proc_filter::filter_obj;
using check_proc::check_proc_filter::lookup_username;
using check_proc::check_proc_filter::parse_proc_pid_stat;
using check_proc::check_proc_filter::parse_proc_stat_btime;
using check_proc::check_proc_filter::parse_proc_stat_cpu_total;
using check_proc::check_proc_filter::parse_proc_status_bytes;
using check_proc::check_proc_filter::parse_proc_status_uid;
using check_proc::check_proc_filter::proc_stat_data;

// ============================================================================
// /proc/[pid]/stat parsing
//
// Format: pid (comm) state ppid pgrp session tty_nr tpgid flags minflt cminflt
// majflt cmajflt utime stime cutime cstime priority nice num_threads
// itrealvalue starttime ...
// The comm field may contain spaces and parentheses, so it is delimited by the
// first '(' and the LAST ')'.
// ============================================================================

TEST(ParseProcPidStat, TypicalLine) {
  proc_stat_data data;
  ASSERT_TRUE(parse_proc_pid_stat(
      "1234 (bash) S 1000 1234 1234 34816 1234 4194304 1500 0 7 0 250 125 3 2 20 0 1 0 9876 8192000 500 18446744073709551615", data));
  EXPECT_EQ("bash", data.comm);
  EXPECT_EQ('S', data.state);
  EXPECT_EQ(1000, data.ppid);
  EXPECT_EQ(7ull, data.major_faults);
  EXPECT_EQ(250ull, data.utime_jiffies);
  EXPECT_EQ(125ull, data.stime_jiffies);
  EXPECT_EQ(9876ull, data.starttime_jiffies);
}

TEST(ParseProcPidStat, CommWithSpaces) {
  proc_stat_data data;
  ASSERT_TRUE(parse_proc_pid_stat("42 (Web Content) R 1 42 42 0 -1 4194560 100 0 5 0 60 40 0 0 20 0 1 0 12345 0 0", data));
  EXPECT_EQ("Web Content", data.comm);
  EXPECT_EQ('R', data.state);
  EXPECT_EQ(1, data.ppid);
}

TEST(ParseProcPidStat, PpidIsReadFromTheFieldAfterState) {
  // ppid is field 4, immediately after the state character. Kernel threads are
  // children of kthreadd (pid 2), which is how they can be filtered out.
  proc_stat_data data;
  ASSERT_TRUE(parse_proc_pid_stat("15 (kworker/0:1) I 2 0 0 0 -1 69238880 0 0 0 0 0 3 0 0 20 0 1 0 22 0 0", data));
  EXPECT_EQ(2, data.ppid);
  EXPECT_EQ('I', data.state);
}

TEST(ParseProcPidStat, CommWithCloseOpenParenSequence) {
  // A process can rename itself to contain ") (" — everything up to the LAST
  // ')' is still the name; a naive find(')') would misparse every field after.
  proc_stat_data data;
  ASSERT_TRUE(parse_proc_pid_stat("77 (a) (b) R 1 77 77 0 -1 4194560 100 0 5 0 60 40 0 0 20 0 1 0 12345 0 0", data));
  EXPECT_EQ("a) (b", data.comm);
  EXPECT_EQ('R', data.state);
  EXPECT_EQ(60ull, data.utime_jiffies);
  EXPECT_EQ(40ull, data.stime_jiffies);
  EXPECT_EQ(12345ull, data.starttime_jiffies);
}

TEST(ParseProcPidStat, CommWithNestedParens) {
  proc_stat_data data;
  ASSERT_TRUE(parse_proc_pid_stat("99 ((sd-pam)) S 98 98 98 0 -1 1077936448 54 0 0 0 0 0 0 0 20 0 1 0 333 0 0", data));
  EXPECT_EQ("(sd-pam)", data.comm);
  EXPECT_EQ(333ull, data.starttime_jiffies);
}

TEST(ParseProcPidStat, MalformedLinesAreRejected) {
  proc_stat_data data;
  EXPECT_FALSE(parse_proc_pid_stat("", data));
  EXPECT_FALSE(parse_proc_pid_stat("1234", data));
  EXPECT_FALSE(parse_proc_pid_stat("1234 (bash S 1 2 3", data));  // no closing paren
  EXPECT_FALSE(parse_proc_pid_stat("1234 (bash)", data));         // nothing after comm
  // Truncated before starttime (field 22) — a delta over a half-parsed record
  // would silently use starttime=0, so short lines must be rejected outright.
  EXPECT_FALSE(parse_proc_pid_stat("1234 (bash) S 1000 1234 1234 34816 1234 4194304 1500 0 7 0 250 125", data));
}

// ============================================================================
// /proc/[pid]/status parsing (VmPeak / VmHWM, values are in kB)
// ============================================================================

namespace {
const std::string status_content =
    "Name:\tbash\n"
    "State:\tS (sleeping)\n"
    "Uid:\t1000\t1000\t1000\t1000\n"
    "Gid:\t1000\t1000\t1000\t1000\n"
    "VmPeak:\t   10240 kB\n"
    "VmSize:\t    8192 kB\n"
    "VmHWM:\t      512 kB\n"
    "VmRSS:\t      400 kB\n"
    "Threads:\t1\n";
}  // namespace

TEST(ParseProcStatusBytes, ExtractsValuesInBytes) {
  unsigned long long bytes = 0;
  ASSERT_TRUE(parse_proc_status_bytes(status_content, "VmPeak", bytes));
  EXPECT_EQ(10240ull * 1024ull, bytes);
  ASSERT_TRUE(parse_proc_status_bytes(status_content, "VmHWM", bytes));
  EXPECT_EQ(512ull * 1024ull, bytes);
}

TEST(ParseProcStatusBytes, MissingKeyLeavesValueUntouched) {
  // Kernel threads have no Vm* entries at all.
  unsigned long long bytes = 42;
  EXPECT_FALSE(parse_proc_status_bytes("Name:\tkthreadd\nState:\tS (sleeping)\n", "VmPeak", bytes));
  EXPECT_EQ(42ull, bytes);
}

TEST(ParseProcStatusBytes, KeyMustMatchWholeLabel) {
  // "VmPeak" must not match the "VmP" prefix query or vice versa.
  unsigned long long bytes = 0;
  EXPECT_FALSE(parse_proc_status_bytes(status_content, "VmP", bytes));
  EXPECT_FALSE(parse_proc_status_bytes(status_content, "VmSwap", bytes));
}

// ============================================================================
// /proc/[pid]/status Uid: parsing and uid -> name resolution
// ============================================================================

TEST(ParseProcStatusUid, ExtractsTheRealUid) {
  // The line holds real, effective, saved and filesystem uid; the owner is the
  // first (real) one.
  long long uid = -1;
  ASSERT_TRUE(parse_proc_status_uid(status_content, uid));
  EXPECT_EQ(1000, uid);
}

TEST(ParseProcStatusUid, RootIsZeroNotMissing) {
  long long uid = -1;
  ASSERT_TRUE(parse_proc_status_uid("Name:\tsystemd\nUid:\t0\t0\t0\t0\n", uid));
  EXPECT_EQ(0, uid);
}

TEST(ParseProcStatusUid, EffectiveUidDiffersFromRealUid) {
  // A setuid binary: real 1000, effective 0. We report the real uid (the owner
  // who started it), which is what `ps -o uid` shows.
  long long uid = -1;
  ASSERT_TRUE(parse_proc_status_uid("Uid:\t1000\t0\t0\t0\n", uid));
  EXPECT_EQ(1000, uid);
}

TEST(ParseProcStatusUid, MissingOrMalformedLineLeavesValueUntouched) {
  long long uid = -1;
  EXPECT_FALSE(parse_proc_status_uid("Name:\tkthreadd\nState:\tS (sleeping)\n", uid));
  EXPECT_EQ(-1, uid);
  EXPECT_FALSE(parse_proc_status_uid("Uid:\tnotanumber\n", uid));
  EXPECT_EQ(-1, uid);
  // "Uid:" must match the whole label, not a prefix of another key.
  EXPECT_FALSE(parse_proc_status_uid("NStgid:\t42\n", uid));
  EXPECT_EQ(-1, uid);
}

TEST(LookupUsername, NegativeUidResolvesToEmpty) {
  // -1 is the "not known" sentinel used by the synthetic not-found/total rows;
  // it must never reach getpwuid_r.
  EXPECT_EQ("", lookup_username(-1));
}

TEST(LookupUsername, ResolvesRootAndIsStableAcrossCalls) {
  // uid 0 exists on every Unix; the second call must come from the cache and
  // return exactly the same answer.
  const std::string first = lookup_username(0);
  EXPECT_EQ("root", first);
  EXPECT_EQ(first, lookup_username(0));
}

TEST(LookupUsername, UnknownUidResolvesToEmptyRatherThanFailing) {
  // A uid with no passwd entry (e.g. a deleted user still owning processes in
  // a container) must degrade to an empty name, not an error.
  EXPECT_EQ("", lookup_username(4294967000LL));
}

// ============================================================================
// Raw Linux process state (`proc_state`)
// ============================================================================

TEST(ProcState, StateCharactersMapToNames) {
  filter_obj p;
  const struct {
    char c;
    const char *name;
  } cases[] = {{'R', "running"},      {'S', "sleeping"}, {'D', "disk_sleep"}, {'Z', "zombie"}, {'T', "stopped"},
               {'t', "tracing_stop"}, {'X', "dead"},     {'I', "idle"},       {'P', "parked"}};
  for (const auto &c : cases) {
    p.proc_state = c.c;
    EXPECT_EQ(c.name, p.get_proc_state_s()) << "state char " << c.c;
  }
}

TEST(ProcState, UnsetOrUnrecognisedStateIsUnknown) {
  filter_obj p;  // default '?'
  EXPECT_EQ("unknown", p.get_proc_state_s());
  EXPECT_EQ(filter_obj::proc_state_unknown, p.get_proc_state_i());
  p.proc_state = 'W';  // obsolete "paging"/"wakekill"
  EXPECT_EQ("unknown", p.get_proc_state_s());
}

TEST(ProcState, ParseRoundTripsEveryRenderedName) {
  // Whatever get_proc_state_s() renders must be usable verbatim in an
  // expression such as `proc_state = 'zombie'`.
  filter_obj p;
  for (const char c : std::string("RSDZTtXIP")) {
    p.proc_state = c;
    EXPECT_EQ(p.get_proc_state_i(), filter_obj::parse_proc_state(p.get_proc_state_s())) << "state char " << c;
  }
}

TEST(ProcState, ParseAcceptsCommonSynonyms) {
  EXPECT_EQ(filter_obj::proc_state_disk_sleep, filter_obj::parse_proc_state("uninterruptible"));
  EXPECT_EQ(filter_obj::proc_state_zombie, filter_obj::parse_proc_state("defunct"));
  EXPECT_EQ(filter_obj::proc_state_tracing_stop, filter_obj::parse_proc_state("traced"));
}

TEST(ProcState, UnknownNameParsesToUnknownRatherThanMatchingSomething) {
  EXPECT_EQ(filter_obj::proc_state_unknown, filter_obj::parse_proc_state("nonsense"));
  EXPECT_EQ(filter_obj::proc_state_unknown, filter_obj::parse_proc_state(""));
}

TEST(ProcState, IsIndependentOfTheCrossPlatformStateKeyword) {
  // `state` keeps its started/stopped meaning: a zombie is not "started", but
  // it is precisely distinguishable through `proc_state`. This separation is
  // why proc_state exists as its own keyword.
  filter_obj zombie;
  zombie.proc_state = 'Z';
  zombie.started = false;
  EXPECT_EQ("stopped", zombie.get_state_s());
  EXPECT_EQ("zombie", zombie.get_proc_state_s());

  filter_obj blocked;
  blocked.proc_state = 'D';
  blocked.started = true;  // D counts as started for `state`
  EXPECT_EQ("started", blocked.get_state_s());
  EXPECT_EQ("disk_sleep", blocked.get_proc_state_s());
}

TEST(ParseState, AcceptsRunningAsSynonymForStarted) {
  // Matches the Windows check; the rendered value stays "started".
  EXPECT_EQ(filter_obj::state_started, filter_obj::parse_state("running"));
  EXPECT_EQ(filter_obj::state_started, filter_obj::parse_state("started"));
  EXPECT_EQ(filter_obj::state_stopped, filter_obj::parse_state("stopped"));
  EXPECT_EQ(filter_obj::state_unknown, filter_obj::parse_state("zombie"));

  filter_obj p;
  p.started = true;
  EXPECT_EQ("started", p.get_state_s());
}

// ============================================================================
// /proc/stat parsing (total CPU jiffies and boot time)
// ============================================================================

namespace {
const std::string proc_stat_content =
    "cpu  100 20 30 400 50 6 7 8 90 10\n"
    "cpu0 50 10 15 200 25 3 3 4 45 5\n"
    "cpu1 50 10 15 200 25 3 3 4 45 5\n"
    "intr 12345 0 0\n"
    "ctxt 987654\n"
    "btime 1719800000\n"
    "processes 4242\n";
}  // namespace

TEST(ParseProcStatCpuTotal, SumsFirstEightFieldsOfAggregateLine) {
  // guest (90) and guest_nice (10) are already included in user/nice and must
  // NOT be added again: 100+20+30+400+50+6+7+8 = 621.
  unsigned long long total = 0;
  ASSERT_TRUE(parse_proc_stat_cpu_total(proc_stat_content, total));
  EXPECT_EQ(621ull, total);
}

TEST(ParseProcStatCpuTotal, IgnoresPerCoreLines) {
  // Only "cpu " (the aggregate) counts; "cpu0"/"cpu1" must not match.
  unsigned long long total = 0;
  EXPECT_FALSE(parse_proc_stat_cpu_total("cpu0 50 10 15 200 25 3 3 4 45 5\n", total));
}

TEST(ParseProcStatCpuTotal, OldKernelsWithFewerFields) {
  unsigned long long total = 0;
  ASSERT_TRUE(parse_proc_stat_cpu_total("cpu  1 2 3 4\n", total));
  EXPECT_EQ(10ull, total);
}

TEST(ParseProcStatBtime, ExtractsBootTime) {
  unsigned long long btime = 0;
  ASSERT_TRUE(parse_proc_stat_btime(proc_stat_content, btime));
  EXPECT_EQ(1719800000ull, btime);
}

TEST(ParseProcStatBtime, MissingBtimeIsRejected) {
  unsigned long long btime = 0;
  EXPECT_FALSE(parse_proc_stat_btime("cpu  1 2 3 4\n", btime));
}

// ============================================================================
// Per-process CPU delta (filter_obj::make_cpu_delta / to_percent)
//
// delta=true redefines the time/user/kernel fields from "cumulative CPU
// seconds" to "percent of total CPU consumed during the one second sample
// window". The math mirrors the Windows check after the delta=true
// reliability fix: capacity is the total system jiffies (all cores, incl.
// idle), percentages are rounded to the nearest whole percent, and deltas
// that are not meaningful (PID reuse / backwards counters / no capacity) are
// rejected so the caller drops the process instead of emitting garbage.
// ============================================================================

namespace {
filter_obj make_cpu_sample(unsigned long long kernel_raw, unsigned long long user_raw, unsigned long long start_time = 1000) {
  filter_obj p;
  p.kernel_time_raw = kernel_raw;
  p.user_time_raw = user_raw;
  p.start_time_jiffies = start_time;
  return p;
}
}  // namespace

TEST(CpuDeltaToPercent, ExactPercentages) {
  EXPECT_EQ(0ull, filter_obj::to_percent(0, 100));
  EXPECT_EQ(50ull, filter_obj::to_percent(50, 100));
  EXPECT_EQ(100ull, filter_obj::to_percent(100, 100));
}

TEST(CpuDeltaToPercent, RoundsToNearestRatherThanTruncating) {
  // 0.6% must round up to 1% so small-but-real usage stays visible.
  EXPECT_EQ(1ull, filter_obj::to_percent(6, 1000));
  // 0.4% still rounds down to 0%.
  EXPECT_EQ(0ull, filter_obj::to_percent(4, 1000));
  // 0.5% rounds up (round-half-up).
  EXPECT_EQ(1ull, filter_obj::to_percent(5, 1000));
}

TEST(CpuDeltaToPercent, ZeroWholeIsGuarded) { EXPECT_EQ(0ull, filter_obj::to_percent(1234, 0)); }

TEST(MakeCpuDelta, NormalUsageProducesPercentages) {
  // Process burned 50 kernel + 100 user jiffies against a capacity of 1000
  // jiffies (e.g. one second on a ten-core box at 100 Hz).
  filter_obj previous = make_cpu_sample(500, 700);
  filter_obj current = make_cpu_sample(500 + 50, 700 + 100);

  ASSERT_TRUE(current.make_cpu_delta(previous, 1000));
  EXPECT_EQ(5, current.get_kernel_time());
  EXPECT_EQ(10, current.get_user_time());
  EXPECT_EQ(15, current.get_total_time());
}

TEST(MakeCpuDelta, TotalIsRoundedFromTheSummedDelta) {
  // 0.5% kernel + 0.5% user: the parts round to 1% each but the total is
  // rounded from the summed delta (1.0% -> 1%), matching Windows.
  filter_obj previous = make_cpu_sample(0, 0);
  filter_obj current = make_cpu_sample(5, 5);

  ASSERT_TRUE(current.make_cpu_delta(previous, 1000));
  EXPECT_EQ(1, current.get_kernel_time());
  EXPECT_EQ(1, current.get_user_time());
  EXPECT_EQ(1, current.get_total_time());
}

TEST(MakeCpuDelta, SubOnePercentUsageIsNotLostToTruncation) {
  filter_obj previous = make_cpu_sample(0, 0);
  filter_obj current = make_cpu_sample(6, 0);

  ASSERT_TRUE(current.make_cpu_delta(previous, 1000));
  EXPECT_EQ(1, current.get_total_time());
}

TEST(MakeCpuDelta, BackwardsCounterIsRejected) {
  // A process exits mid-window and its PID is recycled to a new process whose
  // cumulative CPU time is LOWER: unsigned subtraction would wrap to a
  // gigantic percentage, so the delta is rejected and the fields zeroed.
  filter_obj previous = make_cpu_sample(5000, 5000);
  filter_obj current = make_cpu_sample(10, 10);
  current.total_time = 999;  // sentinel: must be cleared on rejection

  EXPECT_FALSE(current.make_cpu_delta(previous, 1000));
  EXPECT_EQ(0, current.get_total_time());
  EXPECT_EQ(0, current.get_kernel_time());
  EXPECT_EQ(0, current.get_user_time());
}

TEST(MakeCpuDelta, UserCounterGoingBackwardsIsRejected) {
  filter_obj previous = make_cpu_sample(10, 5000);
  filter_obj current = make_cpu_sample(200, 10);

  EXPECT_FALSE(current.make_cpu_delta(previous, 1000));
  EXPECT_EQ(0, current.get_total_time());
}

TEST(MakeCpuDelta, ZeroCapacityIsRejected) {
  filter_obj previous = make_cpu_sample(0, 0);
  filter_obj current = make_cpu_sample(100, 100);

  EXPECT_FALSE(current.make_cpu_delta(previous, 0));
  EXPECT_EQ(0, current.get_total_time());
}

TEST(MakeCpuDelta, FullyBusyAcrossWholeCapacityIsHundredPercent) {
  filter_obj previous = make_cpu_sample(0, 0);
  filter_obj current = make_cpu_sample(600, 400);

  ASSERT_TRUE(current.make_cpu_delta(previous, 1000));
  EXPECT_EQ(100, current.get_total_time());
}

// ============================================================================
// Aggregation (`total`) covers the new counters
// ============================================================================

TEST(FilterObjAggregation, PlusEqualsIncludesPeaksAndTotalTime) {
  filter_obj a;
  a.peak_virtual_size = 100;
  a.peak_working_set = 10;
  a.total_time = 3;
  filter_obj b;
  b.peak_virtual_size = 200;
  b.peak_working_set = 20;
  b.total_time = 4;

  a += b;

  EXPECT_EQ(300, a.get_peak_virtual_size());
  EXPECT_EQ(30, a.get_peak_working_set());
  EXPECT_EQ(7, a.get_total_time());
}
