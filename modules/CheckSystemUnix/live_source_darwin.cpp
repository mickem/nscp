// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The live entry points of the one-shot checks on Darwin: sysctl and the Mach
// host statistics in place of procfs, handed to the same platform-neutral
// `_from` half of each check that Linux uses.

#include <mach/vm_statistics.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <nscapi/protobuf/functions_response.hpp>
#include <string>
#include <thread>

#include "check_cpu_utilization.h"
#include "check_kernel_memory.h"
#include "check_kernel_stats.h"
#include "check_load.h"
#include "check_os_version.h"
#include "check_swap_io.h"
#include "check_uptime.h"
#include "collector_source.h"
#include "mach_stats_darwin.h"

namespace {

std::string sysctl_string(const char *name) {
  std::size_t length = 0;
  if (sysctlbyname(name, nullptr, &length, nullptr, 0) != 0 || length == 0) return "";
  std::string value(length, '\0');
  if (sysctlbyname(name, &value[0], &length, nullptr, 0) != 0) return "";
  value.resize(std::strlen(value.c_str()));
  return value;
}

long online_cpus() {
  const long ncpu = sysconf(_SC_NPROCESSORS_ONLN);
  return ncpu < 1 ? 1 : ncpu;
}

}  // namespace

bool checks::read_uptime_seconds(double &uptime_secs, std::string &error) {
  struct timeval boot;
  std::memset(&boot, 0, sizeof(boot));
  std::size_t length = sizeof(boot);
  int mib[2] = {CTL_KERN, KERN_BOOTTIME};
  if (sysctl(mib, 2, &boot, &length, nullptr, 0) != 0 || boot.tv_sec == 0) {
    error = "Failed to read kern.boottime";
    return false;
  }
  struct timeval now;
  gettimeofday(&now, nullptr);
  uptime_secs = static_cast<double>(now.tv_sec - boot.tv_sec) + static_cast<double>(now.tv_usec - boot.tv_usec) / 1e6;
  if (uptime_secs < 0) uptime_secs = 0;
  return true;
}

os_version::os_release_info os_version::read_os_release() {
  os_release_info out;
  // kern.osproductversion is the marketing version ("14.5") and
  // kern.osversion the build ("23F79"); both are what About This Mac shows.
  const std::string version = sysctl_string("kern.osproductversion");
  const std::string build = sysctl_string("kern.osversion");
  if (version.empty()) return out;
  out.distribution = "macos";
  out.family = "macos";
  out.distribution_name = "macOS";
  out.version = version;
  out.pretty = "macOS " + version + (build.empty() ? "" : " (" + build + ")");
  return out;
}

void load_check::check_load(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_load_with(request, response, [](const bool percpu, load_obj &out, std::string &error) {
    double averages[3] = {0, 0, 0};
    if (getloadavg(averages, 3) != 3) {
      error = "Failed to read the load average (getloadavg)";
      return false;
    }
    out.load1 = averages[0];
    out.load5 = averages[1];
    out.load15 = averages[2];
    // The thread count is the Darwin counterpart of the total in
    // /proc/loadavg. There is no count of runnable threads to go with it, so
    // procs_running stays unknown rather than 0.
    long long threads = 0;
    std::string ignored;
    if (mach_stats::read_thread_count(threads, ignored)) {
      out.procs_total = threads;
      out.has_procs_total = true;
    }
    apply_percpu(out, static_cast<int>(online_cpus()), percpu);
    return true;
  });
}

namespace {

bool read_cpu_jiffies(cpu_utilization_check::cpu_jiffies &out) {
  const std::map<std::string, collector_source::cpu_times> times = collector_source::read_cpu_times();
  const auto it = times.find("cpu");
  if (it == times.end()) return false;
  // user, nice, system and idle are all Darwin accounts; the Linux-only
  // buckets stay 0.
  out.user = it->second.user;
  out.nice = it->second.nice;
  out.system = it->second.system;
  out.idle = it->second.idle;
  out.valid = true;
  return true;
}

}  // namespace

void cpu_utilization_check::check_cpu_utilization(const PB::Commands::QueryRequestMessage::Request &request,
                                                  PB::Commands::QueryResponseMessage::Response *response) {
  cpu_jiffies prev, cur;
  if (!read_cpu_jiffies(prev)) {
    return nscapi::protobuf::functions::set_response_bad(*response, "Failed to read CPU times (host_processor_info)");
  }
  std::this_thread::sleep_for(std::chrono::seconds(1));
  if (!read_cpu_jiffies(cur)) {
    return nscapi::protobuf::functions::set_response_bad(*response, "Failed to read CPU times (host_processor_info)");
  }
  check_cpu_utilization_from(request, response, prev, cur);
}

void kernel_stats_check::check_kernel_stats(const PB::Commands::QueryRequestMessage::Request &request,
                                            PB::Commands::QueryResponseMessage::Response *response) {
  // Darwin keeps no unprivileged system-wide count of context switches or
  // forks, so the counters stay invalid and only the threads row is built.
  // No one-second sample either: the thread count is a gauge.
  long long threads = 0;
  std::string error;
  if (!mach_stats::read_thread_count(threads, error)) {
    return nscapi::protobuf::functions::set_response_bad(*response, "Failed to read the thread count: " + error);
  }
  const kstat_counters none;
  check_kernel_stats_from(request, response, none, none, 1.0, threads);
}

void swap_io_check::check_swap_io(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  vm_statistics64_data_t before, after;
  unsigned long long page_size = 0;
  std::string error;
  if (!mach_stats::read_vm_statistics(before, page_size, error)) {
    return nscapi::protobuf::functions::set_response_bad(*response, "Failed to read VM statistics: " + error);
  }
  const std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
  std::this_thread::sleep_for(std::chrono::seconds(1));
  const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
  if (!mach_stats::read_vm_statistics(after, page_size, error)) {
    return nscapi::protobuf::functions::set_response_bad(*response, "Failed to read VM statistics: " + error);
  }
  vmstat_swap prev, cur;
  prev.pswpin = before.swapins;
  prev.pswpout = before.swapouts;
  prev.valid = true;
  cur.pswpin = after.swapins;
  cur.pswpout = after.swapouts;
  cur.valid = true;

  // Darwin swaps to files it creates and removes on demand; there is one
  // swap "device" while any of them exists.
  long long swap_count = 0;
  struct xsw_usage usage;
  std::memset(&usage, 0, sizeof(usage));
  std::size_t length = sizeof(usage);
  if (sysctlbyname("vm.swapusage", &usage, &length, nullptr, 0) == 0 && usage.xsu_total > 0) swap_count = 1;

  check_swap_io_from(request, response, prev, cur, elapsed, swap_count, static_cast<long long>(page_size));
}

void kernel_memory_check::check_kernel_memory(const PB::Commands::QueryRequestMessage::Request &request,
                                              PB::Commands::QueryResponseMessage::Response *response) {
  vm_statistics64_data_t before, after;
  unsigned long long page_size = 0;
  std::string error;
  if (!mach_stats::read_vm_statistics(before, page_size, error)) {
    return nscapi::protobuf::functions::set_response_bad(*response, "Failed to read VM statistics: " + error);
  }
  const std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
  std::this_thread::sleep_for(std::chrono::seconds(1));
  const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
  if (!mach_stats::read_vm_statistics(after, page_size, error)) {
    return nscapi::protobuf::functions::set_response_bad(*response, "Failed to read VM statistics: " + error);
  }

  // The fault counters go through the same rate computation Linux uses:
  // faults for every fault, pageins for the ones that had to read from disk.
  vmstat_faults prev, cur;
  prev.pgfault = before.faults;
  prev.pgmajfault = before.pageins;
  prev.valid = true;
  cur.pgfault = after.faults;
  cur.pgmajfault = after.pageins;
  cur.valid = true;
  kernel_memory_obj row = compute_kernel_memory(meminfo_kernel(), prev, cur, elapsed);

  // No slab on Darwin: those stay absent, and wired and compressed take
  // their place. The cache is the file-backed pages.
  row.slab = boost::none;
  row.slab_reclaimable = boost::none;
  row.slab_unreclaimable = boost::none;
  row.wired = static_cast<long long>(static_cast<unsigned long long>(after.wire_count) * page_size);
  row.compressed = static_cast<long long>(static_cast<unsigned long long>(after.compressor_page_count) * page_size);
  row.cache = static_cast<long long>(static_cast<unsigned long long>(after.external_page_count) * page_size);
  check_from(request, response, row);
}
