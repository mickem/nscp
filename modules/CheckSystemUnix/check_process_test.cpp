// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_process.h"

#include <gtest/gtest.h>

using check_proc::check_proc_filter::filter_obj;
using check_proc::check_proc_filter::parse_proc_pid_stat;
using check_proc::check_proc_filter::parse_proc_stat_btime;
using check_proc::check_proc_filter::parse_proc_stat_cpu_total;
using check_proc::check_proc_filter::parse_proc_status_bytes;
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
