// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "mach_stats_darwin.h"

#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/mach_time.h>
#include <mach/processor_set.h>

#include <cerrno>
#include <cstring>

namespace mach_stats {

bool read_vm_statistics(vm_statistics64_data_t &stats, unsigned long long &page_size, std::string &error) {
  std::memset(&stats, 0, sizeof(stats));
  vm_size_t size = 0;
  if (host_page_size(mach_host_self(), &size) != KERN_SUCCESS || size == 0) {
    error = "host_page_size failed";
    return false;
  }
  mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
  const kern_return_t kr = host_statistics64(mach_host_self(), HOST_VM_INFO64, reinterpret_cast<host_info64_t>(&stats), &count);
  if (kr != KERN_SUCCESS) {
    error = "host_statistics64 failed: " + std::string(mach_error_string(kr));
    return false;
  }
  page_size = size;
  return true;
}

bool read_thread_count(long long &threads, std::string &error) {
  processor_set_name_t pset = MACH_PORT_NULL;
  kern_return_t kr = processor_set_default(mach_host_self(), &pset);
  if (kr != KERN_SUCCESS) {
    error = "processor_set_default failed: " + std::string(mach_error_string(kr));
    return false;
  }
  processor_set_load_info_data_t info;
  std::memset(&info, 0, sizeof(info));
  mach_msg_type_number_t count = PROCESSOR_SET_LOAD_INFO_COUNT;
  kr = processor_set_statistics(pset, PROCESSOR_SET_LOAD_INFO, reinterpret_cast<processor_set_info_t>(&info), &count);
  mach_port_deallocate(mach_task_self(), pset);
  if (kr != KERN_SUCCESS) {
    error = "processor_set_statistics failed: " + std::string(mach_error_string(kr));
    return false;
  }
  threads = info.thread_count;
  return true;
}

unsigned long long mach_ticks_to_ns(const unsigned long long ticks) {
  static const mach_timebase_info_data_t timebase = [] {
    mach_timebase_info_data_t tb{0, 0};
    if (mach_timebase_info(&tb) != KERN_SUCCESS || tb.denom == 0) tb = mach_timebase_info_data_t{1, 1};
    return tb;
  }();
  // The ratio ticks are scaled by: its two members, in declaration order.
  const auto [scale, divisor] = timebase;
  if (scale == divisor) return ticks;
  // Split to keep ticks * scale from overflowing for long-running processes.
  const unsigned long long whole = ticks / divisor;
  const unsigned long long rest = ticks % divisor;
  return whole * scale + rest * scale / divisor;
}

std::vector<struct kinfo_proc> read_all_processes() {
  int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0};
  // The table can grow between the size query and the read; retry with the
  // new size a few times, with headroom.
  for (int attempt = 0; attempt < 4; ++attempt) {
    std::size_t length = 0;
    if (sysctl(mib, 4, nullptr, &length, nullptr, 0) != 0 || length == 0) return {};
    std::vector<struct kinfo_proc> procs(length / sizeof(struct kinfo_proc) + 32);
    length = procs.size() * sizeof(struct kinfo_proc);
    if (sysctl(mib, 4, procs.data(), &length, nullptr, 0) == 0) {
      procs.resize(length / sizeof(struct kinfo_proc));
      return procs;
    }
    if (errno != ENOMEM) return {};
  }
  return {};
}

bool read_process(const pid_t pid, struct kinfo_proc &out) {
  int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, pid};
  std::memset(&out, 0, sizeof(out));
  std::size_t length = sizeof(out);
  // A pid that does not exist answers success with no data.
  return sysctl(mib, 4, &out, &length, nullptr, 0) == 0 && length == sizeof(out) && out.kp_proc.p_pid == pid;
}

}  // namespace mach_stats
