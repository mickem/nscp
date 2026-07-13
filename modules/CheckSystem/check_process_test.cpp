// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_process.hpp"

#include <gtest/gtest.h>

#include <list>
#include <string>
#include <win/processes.hpp>

using process_checks::realtime::process_name_matches_any;
using win_list_processes::process_info;

// ============================================================================
// process_name_matches_any — case-insensitive any-of match
//
// Backs the realtime check_process matcher (issues #587, #552). Windows
// process / executable names are case-insensitive, so the realtime path must
// match `notepad.exe` against `NOTEPAD.EXE` and any case combination. The
// active check path already does this via CaseBlindCompare; these tests pin
// the realtime path's behaviour to the same contract.
// ============================================================================

TEST(ProcessNameMatchesAny, EmptyListReturnsFalse) { EXPECT_FALSE(process_name_matches_any({}, "notepad.exe")); }

TEST(ProcessNameMatchesAny, EmptyCandidateAgainstNonEmptyList) { EXPECT_FALSE(process_name_matches_any({"notepad.exe"}, "")); }

TEST(ProcessNameMatchesAny, ExactMatch) { EXPECT_TRUE(process_name_matches_any({"notepad.exe"}, "notepad.exe")); }

TEST(ProcessNameMatchesAny, UpperCaseCandidateMatchesLowerName) { EXPECT_TRUE(process_name_matches_any({"notepad.exe"}, "NOTEPAD.EXE")); }

TEST(ProcessNameMatchesAny, LowerCaseCandidateMatchesUpperName) { EXPECT_TRUE(process_name_matches_any({"NOTEPAD.EXE"}, "notepad.exe")); }

TEST(ProcessNameMatchesAny, MixedCaseMatches) {
  // Examples drawn straight from the issues: WinLogon.exe vs winlogon.exe and
  // NoTePaD.eXe — all the same process to Windows.
  EXPECT_TRUE(process_name_matches_any({"winlogon.exe"}, "WinLogon.exe"));
  EXPECT_TRUE(process_name_matches_any({"WinLogon.exe"}, "winlogon.exe"));
  EXPECT_TRUE(process_name_matches_any({"notepad.exe"}, "NoTePaD.eXe"));
}

TEST(ProcessNameMatchesAny, NoMatchOnDifferentName) { EXPECT_FALSE(process_name_matches_any({"notepad.exe"}, "calc.exe")); }

TEST(ProcessNameMatchesAny, NoMatchOnSubstring) {
  // The matcher must compare full names — 'note.exe' is not the same process
  // as 'notepad.exe' and the user is configuring exact names.
  EXPECT_FALSE(process_name_matches_any({"note.exe"}, "notepad.exe"));
  EXPECT_FALSE(process_name_matches_any({"notepad"}, "notepad.exe"));
}

TEST(ProcessNameMatchesAny, MatchesAnyOfMultiple) {
  const std::list<std::string> names = {"calc.exe", "notepad.exe", "explorer.exe"};

  EXPECT_TRUE(process_name_matches_any(names, "calc.exe"));
  EXPECT_TRUE(process_name_matches_any(names, "NOTEPAD.EXE"));
  EXPECT_TRUE(process_name_matches_any(names, "Explorer.EXE"));
  EXPECT_FALSE(process_name_matches_any(names, "csrss.exe"));
}

TEST(ProcessNameMatchesAny, FirstMatchInListIsEnough) {
  // any_of semantics — even with the match in position 0 the function must
  // return true (and not iterate further).
  const std::list<std::string> names = {"notepad.exe", "another.exe"};
  EXPECT_TRUE(process_name_matches_any(names, "NOTEPAD.EXE"));
}

TEST(ProcessNameMatchesAny, LastMatchInListIsEnough) {
  const std::list<std::string> names = {"a.exe", "b.exe", "notepad.exe"};
  EXPECT_TRUE(process_name_matches_any(names, "NOTEPAD.EXE"));
}

TEST(ProcessNameMatchesAny, DuplicatesInListDoNotBreakSemantics) {
  const std::list<std::string> names = {"notepad.exe", "notepad.exe", "NOTEPAD.EXE"};
  EXPECT_TRUE(process_name_matches_any(names, "notepad.exe"));
  EXPECT_FALSE(process_name_matches_any(names, "calc.exe"));
}

// ============================================================================
// Per-process CPU delta (process_info::make_cpu_delta / to_percent)
//
// `check_process ... delta=true` redefines the `time` field from "cumulative
// CPU seconds" to "percent of total CPU consumed during the sample interval".
// The implementation used to produce wildly unreliable results: absurd values
// (e.g. cpu=72562484370%) from unsigned underflow on PID reuse, everything
// reading 0% from integer truncation, and a denominator that double-counted
// idle time. These tests pin the corrected behaviour.
//
// CPU times are in 100ns FILETIME ticks: one fully-busy core-second is
// 10,000,000 ticks. A snapshot's raw counters are cumulative since process
// start; the delta is the difference between two snapshots of the SAME process.
// ============================================================================

namespace {
// One fully-busy core for one second, in 100ns FILETIME ticks.
constexpr unsigned long long ticks_per_core_second = 10000000ull;

process_info make_cpu_sample(unsigned long long kernel_raw, unsigned long long user_raw, long long creation_time = 1000) {
  process_info p;
  p.kernel_time_raw = kernel_raw;
  p.user_time_raw = user_raw;
  p.creation_time = creation_time;
  return p;
}
}  // namespace

// --- to_percent: rounding instead of truncation (the "everything at 0" fix) ---

TEST(CpuDeltaToPercent, ZeroPartIsZero) { EXPECT_EQ(0ull, process_info::to_percent(0, 100)); }

TEST(CpuDeltaToPercent, ExactPercentages) {
  EXPECT_EQ(50ull, process_info::to_percent(50, 100));
  EXPECT_EQ(100ull, process_info::to_percent(100, 100));
  EXPECT_EQ(25ull, process_info::to_percent(2500000, ticks_per_core_second));
}

TEST(CpuDeltaToPercent, RoundsToNearestRatherThanTruncating) {
  // 0.6% must round up to 1% — old code truncated it to 0, which is exactly why
  // small-but-real processes used to disappear at "everything at 0".
  EXPECT_EQ(1ull, process_info::to_percent(6, 1000));
  // 0.4% still rounds down to 0%.
  EXPECT_EQ(0ull, process_info::to_percent(4, 1000));
  // 0.5% rounds up (round-half-up).
  EXPECT_EQ(1ull, process_info::to_percent(5, 1000));
}

TEST(CpuDeltaToPercent, ZeroWholeIsGuarded) {
  // No measured capacity -> 0, never a divide-by-zero.
  EXPECT_EQ(0ull, process_info::to_percent(1234, 0));
}

// --- make_cpu_delta: the per-process percentage computation ------------------

TEST(MakeCpuDelta, NormalUsageProducesPercentages) {
  // Process burned 1,000,000 kernel + 1,000,000 user ticks against a capacity
  // of 10,000,000 ticks (e.g. one busy core out of a one-second window).
  process_info previous = make_cpu_sample(500, 700);
  process_info current = make_cpu_sample(500 + 1000000, 700 + 1000000);

  const bool ok = current.make_cpu_delta(previous, /*sys_kernel=*/6000000, /*sys_user=*/4000000);

  EXPECT_TRUE(ok);
  EXPECT_EQ(10, current.get_kernel_time());  // 1,000,000 / 10,000,000
  EXPECT_EQ(10, current.get_user_time());    // 1,000,000 / 10,000,000
  EXPECT_EQ(20, current.get_total_time());   // 2,000,000 / 10,000,000
}

TEST(MakeCpuDelta, SubOnePercentUsageIsNotLostToTruncation) {
  // 0.6% of capacity. Old integer-truncating code reported 0; rounded result
  // is 1, so the process is still visible / sortable.
  process_info previous = make_cpu_sample(0, 0);
  process_info current = make_cpu_sample(60000, 0);

  EXPECT_TRUE(current.make_cpu_delta(previous, ticks_per_core_second, 0));
  EXPECT_EQ(1, current.get_total_time());
}

TEST(MakeCpuDelta, DenominatorIsKernelPlusUserOnly) {
  // The same total capacity split differently between the kernel and user
  // arguments must give the same result: idle time is never a parameter, so it
  // cannot be double-counted the way the old kernel+user+idle denominator did.
  process_info previous = make_cpu_sample(0, 0);
  process_info a = make_cpu_sample(2500000, 0);
  process_info b = make_cpu_sample(2500000, 0);

  EXPECT_TRUE(a.make_cpu_delta(previous, ticks_per_core_second, 0));
  EXPECT_TRUE(b.make_cpu_delta(previous, ticks_per_core_second / 2, ticks_per_core_second / 2));
  EXPECT_EQ(a.get_total_time(), b.get_total_time());
  EXPECT_EQ(25, a.get_total_time());
}

TEST(MakeCpuDelta, PidReuseWithLowerCounterDoesNotUnderflow) {
  // The headline bug: a PID is recycled to a new process whose cumulative CPU
  // time is LOWER than the previous occupant's. The unsigned subtraction used
  // to wrap around to a gigantic number (cpu=72562484370%). Now it is rejected.
  process_info previous = make_cpu_sample(5000000, 5000000);
  process_info current = make_cpu_sample(1000, 1000);
  current.total_time = 999;  // sentinel: must be cleared on rejection

  const bool ok = current.make_cpu_delta(previous, ticks_per_core_second, ticks_per_core_second);

  EXPECT_FALSE(ok);
  EXPECT_EQ(0, current.get_total_time());
  EXPECT_EQ(0, current.get_kernel_time());
  EXPECT_EQ(0, current.get_user_time());
}

TEST(MakeCpuDelta, UserCounterGoingBackwardsIsRejected) {
  // Kernel advanced normally but user time regressed -> still not meaningful.
  process_info previous = make_cpu_sample(1000, 5000000);
  process_info current = make_cpu_sample(2000000, 1000);

  EXPECT_FALSE(current.make_cpu_delta(previous, ticks_per_core_second, ticks_per_core_second));
  EXPECT_EQ(0, current.get_total_time());
}

TEST(MakeCpuDelta, ZeroCapacityIsRejected) {
  process_info previous = make_cpu_sample(0, 0);
  process_info current = make_cpu_sample(1000000, 1000000);

  EXPECT_FALSE(current.make_cpu_delta(previous, 0, 0));
  EXPECT_EQ(0, current.get_total_time());
}

TEST(MakeCpuDelta, FullyBusyAcrossWholeCapacityIsHundredPercent) {
  process_info previous = make_cpu_sample(0, 0);
  process_info current = make_cpu_sample(6000000, 4000000);  // 10,000,000 total

  EXPECT_TRUE(current.make_cpu_delta(previous, 6000000, 4000000));
  EXPECT_EQ(100, current.get_total_time());
}

// --- operator-=: CPU raw counters must survive so make_cpu_delta can use them -

TEST(ProcessInfoMinus, DoesNotTouchRawCpuCounters) {
  // operator-= turns the gauge fields (working set, ...) into deltas for delta
  // mode, but it must leave the raw CPU counters absolute — the per-process CPU
  // delta is computed from the two raw snapshots in make_cpu_delta(), and
  // subtracting them here too would underflow on PID reuse.
  process_info later = make_cpu_sample(2000000, 1500000);
  later.WorkingSetSize = 4096;
  process_info earlier = make_cpu_sample(500000, 300000);
  earlier.WorkingSetSize = 1024;

  later -= earlier;

  EXPECT_EQ(2000000ull, later.kernel_time_raw);  // unchanged (absolute)
  EXPECT_EQ(1500000ull, later.user_time_raw);    // unchanged (absolute)
  EXPECT_EQ(3072, later.get_WorkingSetSize());   // gauge became a delta
}

TEST(ProcessInfoMinus, RawCountersStillUsableByMakeCpuDeltaAfterSubtraction) {
  // End-to-end: subtract (as enumerate_processes_delta does) then compute the
  // CPU delta. The result must reflect the real per-process delta, not garbage.
  process_info earlier = make_cpu_sample(500000, 300000);
  process_info later = make_cpu_sample(500000 + 1000000, 300000 + 1000000);

  later -= earlier;
  ASSERT_TRUE(later.make_cpu_delta(earlier, 6000000, 4000000));
  EXPECT_EQ(20, later.get_total_time());  // 2,000,000 / 10,000,000
}

// --- thread_count / working_set_pct / pagefile_pct -----------

TEST(ProcessInfoGauges, ThreadCountAccessor) {
  process_info p;
  EXPECT_EQ(0, p.get_threadCount());
  p.threadCount = 37;
  EXPECT_EQ(37, p.get_threadCount());
}

TEST(ProcessInfoGauges, WorkingSetPctUsesTotalPhysicalMemory) {
  process_info p;
  p.WorkingSetSize = 2ull * 1024 * 1024 * 1024;         // 2 GB working set
  p.total_physical_memory = 8ull * 1024 * 1024 * 1024;  // 8 GB RAM
  EXPECT_EQ(25, p.get_working_set_pct());               // 2/8 = 25%
}

TEST(ProcessInfoGauges, PagefilePctUsesCommitLimit) {
  process_info p;
  p.PageFileUsage = 1ull * 1024 * 1024 * 1024;    // 1 GB committed
  p.total_pagefile = 10ull * 1024 * 1024 * 1024;  // 10 GB commit limit
  EXPECT_EQ(10, p.get_pagefile_pct());            // 1/10 = 10%
}

TEST(ProcessInfoGauges, PctGettersGuardZeroBase) {
  // No totals captured (e.g. GlobalMemoryStatusEx failed) -> 0, never divide-by-zero.
  process_info p;
  p.WorkingSetSize = 4096;
  p.PageFileUsage = 4096;
  EXPECT_EQ(0, p.get_working_set_pct());
  EXPECT_EQ(0, p.get_pagefile_pct());
}

TEST(ProcessInfoPlus, AggregatesThreadCountAndCarriesTotals) {
  auto total = process_info::get_total();
  process_info a;
  a.threadCount = 10;
  a.total_physical_memory = 8ull * 1024 * 1024 * 1024;
  a.total_pagefile = 10ull * 1024 * 1024 * 1024;
  process_info b;
  b.threadCount = 15;
  b.total_physical_memory = 8ull * 1024 * 1024 * 1024;
  b.total_pagefile = 10ull * 1024 * 1024 * 1024;

  *total += a;
  *total += b;

  EXPECT_EQ(25, total->get_threadCount());  // summed
  // Totals are constants carried onto the aggregate, not summed (would otherwise
  // double the base and halve every percentage).
  EXPECT_EQ(8ull * 1024 * 1024 * 1024, total->total_physical_memory);
  EXPECT_EQ(10ull * 1024 * 1024 * 1024, total->total_pagefile);
}

// --- state parsing: 'running' is a synonym for 'started' (snclient parity) ---

TEST(ProcessState, RunningIsSynonymForStarted) {
  EXPECT_EQ(process_info::parse_state("started"), process_info::parse_state("running"));
  EXPECT_EQ(process_info::state_started, process_info::parse_state("running"));
  // The other states are unchanged.
  EXPECT_EQ(process_info::state_stopped, process_info::parse_state("stopped"));
  EXPECT_EQ(process_info::state_hung, process_info::parse_state("hung"));
  EXPECT_EQ(process_info::state_unreadable, process_info::parse_state("unreadable"));
  EXPECT_EQ(process_info::state_unknown, process_info::parse_state("nonsense"));
}

TEST(ProcessState, StartedProcessStillRendersAsStarted) {
  // Accepting 'running' as input must not change the rendered value (back-compat).
  process_info p;
  p.started = true;
  EXPECT_EQ("started", p.get_state_s());
  EXPECT_EQ(process_info::state_started, p.get_state_i());
}

// --- owner accessors (uid = SID string; empty until resolve-owner runs) ------

TEST(ProcessOwner, DefaultsEmptyAndUidMirrorsSid) {
  process_info p;
  EXPECT_EQ("", p.get_username());
  EXPECT_EQ("", p.get_uid());
  p.username = "PRICER\\michael.medin";
  p.sid = "S-1-5-21-1-2-3-1001";
  EXPECT_EQ("PRICER\\michael.medin", p.get_username());
  EXPECT_EQ("S-1-5-21-1-2-3-1001", p.get_uid());  // uid is the SID string
}
