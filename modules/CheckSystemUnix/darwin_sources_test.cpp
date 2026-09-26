// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Smoke tests of the Darwin readers against the Mac running the tests. What
// they read depends on the machine, so each asserts the shape a working
// reader has to produce - a positive total, our own process found - rather
// than particular values. Built only on macOS; the pure code above the
// readers is covered by the platform-neutral tests.

#include <gtest/gtest.h>
#include <unistd.h>

#include "check_kernel_stats.h"
#include "check_os_version.h"
#include "check_uptime.h"
#include "collector_source.h"
#include "interfaces_darwin.h"
#include "mach_stats_darwin.h"

TEST(darwin_collector, cpu_times_has_an_aggregate_and_every_core) {
  const std::map<std::string, collector_source::cpu_times> times = collector_source::read_cpu_times();
  ASSERT_EQ(times.count("cpu"), 1u);
  const long cores = sysconf(_SC_NPROCESSORS_ONLN);
  EXPECT_EQ(static_cast<long>(times.size()), cores + 1);
  EXPECT_GT(times.at("cpu").total(), 0u);
  // Darwin has no iowait or steal accounting; the reader leaves them at 0.
  EXPECT_EQ(times.at("cpu").iowait, 0u);
  EXPECT_EQ(times.at("cpu").steal, 0u);
}

TEST(darwin_collector, memory_is_measured) {
  const collector_source::memory_sample m = collector_source::read_memory();
  EXPECT_GT(m.physical_total, 0u);
  EXPECT_LE(m.physical_free, m.physical_total);
  EXPECT_GE(m.cached_free, m.physical_free);
  EXPECT_LE(m.cached_free, m.physical_total);
  EXPECT_LE(m.swap_free, m.swap_total);
}

TEST(darwin_collector, memory_extras_are_wired_and_compressed) {
  const std::map<std::string, unsigned long long> extras = collector_source::read_memory_extras();
  ASSERT_EQ(extras.count("wired"), 1u);
  ASSERT_EQ(extras.count("compressed"), 1u);
  // A running kernel always has wired memory.
  EXPECT_GT(extras.at("wired"), 0u);
}

TEST(darwin_collector, network_lists_the_loopback_with_traffic_counters) {
  const std::map<std::string, collector_source::net_sample> nics = collector_source::read_network();
  ASSERT_EQ(nics.count("lo0"), 1u);
  EXPECT_EQ(nics.at("lo0").status, "up");
  for (const auto &nic : nics) {
    EXPECT_FALSE(nic.first.empty());
    EXPECT_TRUE(nic.second.status == "up" || nic.second.status == "down" || nic.second.status == "unknown") << nic.first << ": " << nic.second.status;
  }
}

TEST(darwin_collector, interfaces_mark_the_loopback) {
  bool found = false;
  for (const darwin_interfaces::interface_info &nic : darwin_interfaces::read()) {
    if (nic.name == "lo0") {
      found = true;
      EXPECT_TRUE(nic.loopback);
    }
  }
  EXPECT_TRUE(found);
}

TEST(darwin_collector, running_exes_include_this_test) {
  const std::set<std::string> exes = collector_source::read_running_exes();
  EXPECT_EQ(exes.count("check_system_unix_test"), 1u);
  EXPECT_EQ(exes.count("launchd"), 1u);
}

TEST(darwin_live, uptime_is_positive) {
  double uptime = 0;
  std::string error;
  ASSERT_TRUE(checks::read_uptime_seconds(uptime, error)) << error;
  EXPECT_GT(uptime, 0.0);
}

TEST(darwin_live, os_release_is_macos) {
  const os_version::os_release_info info = os_version::read_os_release();
  EXPECT_EQ(info.distribution, "macos");
  EXPECT_EQ(info.distribution_name, "macOS");
  EXPECT_FALSE(info.version.empty());
  EXPECT_EQ(info.pretty.compare(0, 6, "macOS "), 0) << info.pretty;
}

TEST(darwin_live, thread_count_is_positive) {
  long long threads = 0;
  std::string error;
  ASSERT_TRUE(mach_stats::read_thread_count(threads, error)) << error;
  EXPECT_GT(threads, 1);
}

TEST(darwin_live, vm_statistics_are_readable) {
  vm_statistics64_data_t stats;
  unsigned long long page_size = 0;
  std::string error;
  ASSERT_TRUE(mach_stats::read_vm_statistics(stats, page_size, error)) << error;
  EXPECT_GT(page_size, 0u);
  EXPECT_GT(stats.wire_count, 0u);
  EXPECT_GT(stats.faults, 0u);
}

TEST(darwin_live, kernel_stats_reports_threads_only) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_kernel_stats");
  PB::Commands::QueryResponseMessage::Response response;
  kernel_stats_check::check_kernel_stats(request, &response);
  ASSERT_GT(response.lines_size(), 0);
  EXPECT_NE(response.lines(0).message().find("Threads"), std::string::npos) << response.lines(0).message();
  EXPECT_EQ(response.lines(0).message().find("Context"), std::string::npos) << response.lines(0).message();
}
