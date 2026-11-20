// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The Linux readers (`*_source_linux.cpp`) against the procfs of the machine
// running the test. Each is a thin loop over /proc around a parser that is
// tested on captured content; what can only be checked here is that the loop
// finds the files where it looks for them. Every case looks for the test
// binary itself, or for something every Linux kernel has.

#include <gtest/gtest.h>
#include <limits.h>
#include <unistd.h>

#include <set>
#include <string>

#include "check_process.h"
#include "collector_source.h"
#include "diagnostics_thread.hpp"
#include "pressure.h"
#include "process_top.h"

namespace {
// What /proc/self/exe ends in: the name the collector records for a process.
std::string own_exe() {
  char path[PATH_MAX] = {0};
  const ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
  if (len <= 0) return "";
  const std::string full(path, static_cast<std::size_t>(len));
  const std::size_t pos = full.find_last_of('/');
  return pos == std::string::npos ? full : full.substr(pos + 1);
}
}  // namespace

TEST(LinuxSources, ProcessTopReadsTheTestBinary) {
  const process_top::table table = process_top::read_processes();
  const auto me = table.find(static_cast<long long>(getpid()));
  ASSERT_NE(table.end(), me) << "own pid missing from a table of " << table.size();
  EXPECT_GT(me->second.working_set, 0u);
}

TEST(LinuxSources, RunningExesNameTheTestBinaryInFull) {
  const std::string exe = own_exe();
  ASSERT_FALSE(exe.empty());
  // Longer than the 15 characters the kernel keeps in `comm`: the name has
  // to come from the exe link, or it would not match what check_process
  // reports as `exe`.
  ASSERT_GT(exe.size(), 15u);
  const std::set<std::string> running = collector_source::read_running_exes();
  EXPECT_EQ(1u, running.count(exe)) << exe << " not among " << running.size() << " names";
}

TEST(LinuxSources, CpuTimesHaveTheAggregateAndACore) {
  const auto times = collector_source::read_cpu_times();
  ASSERT_EQ(1u, times.count("cpu"));
  ASSERT_EQ(1u, times.count("cpu0"));
  EXPECT_GT(times.at("cpu").total(), 0u);
  EXPECT_EQ(-1, collector_source::core_index("cpu"));
  EXPECT_EQ(0, collector_source::core_index("cpu0"));
}

TEST(LinuxSources, PressureReadsTheFaultCounterAndWhateverPsiThereIs) {
  const pressure::sample sample = pressure::read_pressure();
  // /proc/vmstat is always there; /proc/pressure is not (older kernels, PSI
  // off, containers that hide it), and a source that is missing is left out.
  EXPECT_TRUE(sample.has_major_faults);
  EXPECT_LE(sample.resources.size(), 3u);
  for (const pressure::resource &r : sample.resources) {
    EXPECT_TRUE(r.name == "cpu" || r.name == "memory" || r.name == "io") << r.name;
  }
}

TEST(LinuxSources, CpuCapacityAdvances) {
  unsigned long long capacity = 0;
  ASSERT_TRUE(check_proc::check_proc_filter::read_cpu_capacity(capacity));
  EXPECT_GT(capacity, 0u);
}

TEST(LinuxSources, MonotonicClockMovesForward) {
  const std::int64_t first = systemd_diagnostics::source::monotonic_now_us();
  EXPECT_GT(first, 0);
  EXPECT_GE(systemd_diagnostics::source::monotonic_now_us(), first);
}

TEST(LinuxSources, SystemdDetectionMatchesItsRuntimeDirectory) {
  // The directory systemd itself documents as the test (sd_booted(3)).
  EXPECT_EQ(access("/run/systemd/system", F_OK) == 0, systemd_diagnostics::source::has_systemd());
}
