// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The live entry points of the one-shot checks on Linux: each reads procfs
// and hands the sample to the platform-neutral `_from` half of its check.

#include <unistd.h>

#include <chrono>
#include <fstream>
#include <locale>
#include <nscapi/protobuf/functions_response.hpp>
#include <sstream>
#include <string>
#include <thread>

#include "check_cpu_utilization.h"
#include "check_kernel_memory.h"
#include "check_kernel_stats.h"
#include "check_load.h"
#include "check_os_version.h"
#include "check_swap_io.h"
#include "check_uptime.h"

namespace {

std::string read_file(const std::string &path) {
  std::ifstream ifs(path.c_str());
  if (!ifs.is_open()) return "";
  std::stringstream ss;
  ss << ifs.rdbuf();
  return ss.str();
}

}  // namespace

bool checks::read_uptime_seconds(double &uptime_secs, std::string &error) {
  try {
    std::locale c_locale("C");
    std::ifstream f;
    f.imbue(c_locale);
    f.open("/proc/uptime");
    double idle_secs = 0;
    if (f.is_open() && (f >> uptime_secs >> idle_secs)) return true;
  } catch (...) {
  }
  error = "Failed to read /proc/uptime";
  return false;
}

os_version::os_release_info os_version::read_os_release() {
  // /etc/os-release is the documented location; /usr/lib/os-release is the
  // vendor copy a stateless or read-only-root system ships instead.
  std::string content = read_file("/etc/os-release");
  if (content.empty()) content = read_file("/usr/lib/os-release");
  return parse_os_release(content);
}

void load_check::check_load(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_load_from(request, response, "/proc/loadavg");
}

void cpu_utilization_check::check_cpu_utilization(const PB::Commands::QueryRequestMessage::Request &request,
                                                  PB::Commands::QueryResponseMessage::Response *response) {
  const cpu_jiffies prev = parse_proc_stat_cpu(read_file("/proc/stat"));
  if (!prev.valid) {
    return nscapi::protobuf::functions::set_response_bad(*response, "Failed to read /proc/stat");
  }
  std::this_thread::sleep_for(std::chrono::seconds(1));
  const cpu_jiffies cur = parse_proc_stat_cpu(read_file("/proc/stat"));
  if (!cur.valid) {
    return nscapi::protobuf::functions::set_response_bad(*response, "Failed to read /proc/stat");
  }
  check_cpu_utilization_from(request, response, prev, cur);
}

void kernel_stats_check::check_kernel_stats(const PB::Commands::QueryRequestMessage::Request &request,
                                            PB::Commands::QueryResponseMessage::Response *response) {
  const kstat_counters prev = parse_proc_stat_counters(read_file("/proc/stat"));
  if (!prev.valid) {
    return nscapi::protobuf::functions::set_response_bad(*response, "Failed to read /proc/stat");
  }
  std::this_thread::sleep_for(std::chrono::seconds(1));
  const kstat_counters cur = parse_proc_stat_counters(read_file("/proc/stat"));
  if (!cur.valid) {
    return nscapi::protobuf::functions::set_response_bad(*response, "Failed to read /proc/stat");
  }
  const long long threads = count_threads_from("/proc");
  check_kernel_stats_from(request, response, prev, cur, 1.0, threads);
}

void swap_io_check::check_swap_io(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  const vmstat_swap prev = parse_vmstat_swap(read_file("/proc/vmstat"));
  if (!prev.valid) {
    return nscapi::protobuf::functions::set_response_bad(*response, "Failed to read /proc/vmstat");
  }
  std::this_thread::sleep_for(std::chrono::seconds(1));
  const vmstat_swap cur = parse_vmstat_swap(read_file("/proc/vmstat"));
  if (!cur.valid) {
    return nscapi::protobuf::functions::set_response_bad(*response, "Failed to read /proc/vmstat");
  }
  long long page_size = sysconf(_SC_PAGESIZE);
  if (page_size <= 0) page_size = 4096;
  const long long swap_count = count_swaps(read_file("/proc/swaps"));
  check_swap_io_from(request, response, prev, cur, 1.0, swap_count, page_size);
}

void kernel_memory_check::check_kernel_memory(const PB::Commands::QueryRequestMessage::Request &request,
                                              PB::Commands::QueryResponseMessage::Response *response) {
  const vmstat_faults prev = parse_vmstat_faults(read_file("/proc/vmstat"));
  if (!prev.valid) {
    return nscapi::protobuf::functions::set_response_bad(*response, "Failed to read /proc/vmstat");
  }
  // Rates are divided by the interval actually slept, not the requested one:
  // on a loaded host the wake-up can be noticeably late and a fixed 1.0 would
  // overstate the fault rates.
  const std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
  std::this_thread::sleep_for(std::chrono::seconds(1));
  const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
  const vmstat_faults cur = parse_vmstat_faults(read_file("/proc/vmstat"));
  const meminfo_kernel mem = parse_meminfo_kernel(read_file("/proc/meminfo"));
  if (!cur.valid || !mem.valid) {
    return nscapi::protobuf::functions::set_response_bad(*response, "Failed to read /proc/vmstat or /proc/meminfo");
  }
  check_from(request, response, compute_kernel_memory(mem, prev, cur, elapsed));
}
