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
#include "check_process.h"
#include "check_service.h"
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

TEST(darwin_process, finds_this_test_with_its_counters) {
  const std::vector<check_proc::check_proc_filter::filter_obj> procs = check_proc::check_proc_filter::enumerate_processes(true);
  bool found = false;
  for (const auto &p : procs) {
    if (p.pid != getpid()) continue;
    found = true;
    EXPECT_EQ(p.exe, "check_system_unix_test");
    EXPECT_FALSE(p.filename.empty());
    EXPECT_NE(p.command_line.find("check_system_unix_test"), std::string::npos) << p.command_line;
    EXPECT_EQ(p.ppid, getppid());
    EXPECT_EQ(p.uid, static_cast<long long>(getuid()));
    EXPECT_FALSE(p.username.empty());
    EXPECT_TRUE(p.started);
    // Our own task info is always readable.
    EXPECT_TRUE(p.has_task_info);
    EXPECT_GT(p.working_set, 0u);
    EXPECT_GT(p.virtual_size, p.working_set);
    EXPECT_GT(p.creation_time, 0u);
    EXPECT_EQ(p.proc_state, 'R') << "the test is running";
    EXPECT_FALSE(p.has_peaks);
  }
  EXPECT_TRUE(found);
}

TEST(darwin_process, launchd_is_listed_with_its_owner) {
  bool found = false;
  for (const auto &p : check_proc::check_proc_filter::enumerate_processes()) {
    if (p.pid != 1) continue;
    found = true;
    EXPECT_EQ(p.exe, "launchd");
    EXPECT_EQ(p.uid, 0);
    EXPECT_TRUE(p.started);
    // Not ours unless the tests run as root: then the counters are unknown
    // rather than 0.
    if (getuid() != 0) {
      EXPECT_FALSE(p.has_task_info);
    }
  }
  EXPECT_TRUE(found);
}

TEST(darwin_process, cpu_capacity_advances_by_cores_times_wall_clock) {
  unsigned long long a = 0, b = 0;
  ASSERT_TRUE(check_proc::check_proc_filter::read_cpu_capacity(a));
  usleep(100 * 1000);
  ASSERT_TRUE(check_proc::check_proc_filter::read_cpu_capacity(b));
  const long cores = sysconf(_SC_NPROCESSORS_ONLN);
  EXPECT_GE(b - a, 90ull * 1000 * 1000 * static_cast<unsigned long long>(cores));
}

TEST(darwin_service, logd_is_a_running_launchd_job) {
  const checks::check_svc_filter::filter_obj info = checks::check_svc_filter::get_service_info("com.apple.logd");
  EXPECT_EQ(info.name, "com.apple.logd");
  EXPECT_EQ(info.load_state, "loaded");
  EXPECT_EQ(info.state, "running");
  EXPECT_GT(info.pid, 0);
  EXPECT_GT(info.created, 0);
}

TEST(darwin_service, a_missing_job_is_not_found) {
  const checks::check_svc_filter::filter_obj info = checks::check_svc_filter::get_service_info("org.nsclient.no-such-job");
  EXPECT_EQ(info.load_state, "not-found");
  EXPECT_EQ(info.state, "stopped");
}

TEST(darwin_service, the_system_domain_lists_jobs) {
  const std::vector<checks::check_svc_filter::filter_obj> all = checks::check_svc_filter::enumerate_services("all");
  EXPECT_GT(all.size(), 20u);
  bool logd = false;
  for (const auto &s : all) {
    if (s.name == "com.apple.logd") logd = s.state == "running";
  }
  EXPECT_TRUE(logd);
  const std::set<std::string> active = checks::check_svc_filter::active_units({"com.apple.logd", "org.nsclient.no-such-job"});
  EXPECT_EQ(active.count("com.apple.logd"), 1u);
  EXPECT_EQ(active.count("org.nsclient.no-such-job"), 0u);
}
