// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The collector's samples on Darwin: per-core ticks from
// host_processor_info, memory from host_statistics64 and sysctl, interface
// counters from the NET_RT_IFLIST2 sysctl, and the process list from libproc.
// Every call works for an unprivileged account, which is what the launchd
// daemon runs as.

#include <libproc.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/processor_info.h>
#include <mach/vm_statistics.h>
#include <sys/sysctl.h>
#include <sys/types.h>

#include <cstring>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <string>
#include <vector>

#include "collector_source.h"
#include "interfaces_darwin.h"
#include "mach_stats_darwin.h"

namespace collector_source {

std::map<std::string, cpu_times> read_cpu_times() {
  std::map<std::string, cpu_times> result;
  natural_t cpu_count = 0;
  processor_info_array_t info = nullptr;
  mach_msg_type_number_t info_count = 0;
  const kern_return_t kr = host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &cpu_count, &info, &info_count);
  if (kr != KERN_SUCCESS || info == nullptr) {
    NSC_LOG_ERROR("Failed to read CPU times: host_processor_info returned " + std::to_string(kr));
    return result;
  }

  cpu_times total;
  total.name = "cpu";
  const processor_cpu_load_info_t load = reinterpret_cast<processor_cpu_load_info_t>(info);
  for (natural_t i = 0; i < cpu_count; ++i) {
    cpu_times core;
    core.name = "cpu" + std::to_string(i);
    core.user = load[i].cpu_ticks[CPU_STATE_USER];
    core.nice = load[i].cpu_ticks[CPU_STATE_NICE];
    core.system = load[i].cpu_ticks[CPU_STATE_SYSTEM];
    core.idle = load[i].cpu_ticks[CPU_STATE_IDLE];
    total.user += core.user;
    total.nice += core.nice;
    total.system += core.system;
    total.idle += core.idle;
    result[core.name] = core;
  }
  result[total.name] = total;

  vm_deallocate(mach_task_self(), reinterpret_cast<vm_address_t>(info), static_cast<vm_size_t>(info_count) * sizeof(integer_t));
  return result;
}

namespace {

struct vm_pages {
  bool ok = false;
  unsigned long long page_size = 0;
  vm_statistics64_data_t stats;
};

vm_pages read_vm_pages() {
  vm_pages out;
  std::string error;
  out.ok = mach_stats::read_vm_statistics(out.stats, out.page_size, error);
  if (!out.ok) NSC_LOG_ERROR("Failed to read memory info: " + error);
  return out;
}

unsigned long long read_memsize() {
  unsigned long long memsize = 0;
  std::size_t length = sizeof(memsize);
  if (sysctlbyname("hw.memsize", &memsize, &length, nullptr, 0) != 0) return 0;
  return memsize;
}

}  // namespace

memory_sample read_memory() {
  memory_sample result;
  const vm_pages vm = read_vm_pages();
  const unsigned long long total = read_memsize();
  if (!vm.ok || total == 0) return result;

  const unsigned long long page = vm.page_size;
  result.physical_total = total;
  // free_count already includes the speculative pages (vm_stat subtracts
  // them back out to print "Pages free"): read-ahead the kernel drops first.
  result.physical_free = std::min(total, static_cast<unsigned long long>(vm.stats.free_count) * page);
  // The closest Darwin has to the Linux page cache: file-backed pages, which
  // are the file cache, and purgeable pages, which an application has marked
  // as discardable. Activity Monitor adds up the same two as "Cached Files".
  const unsigned long long cache = (static_cast<unsigned long long>(vm.stats.external_page_count) + vm.stats.purgeable_count) * page;
  result.cached_free = std::min(total, result.physical_free + cache);

  struct xsw_usage swap;
  std::memset(&swap, 0, sizeof(swap));
  std::size_t length = sizeof(swap);
  if (sysctlbyname("vm.swapusage", &swap, &length, nullptr, 0) == 0) {
    result.swap_total = swap.xsu_total;
    result.swap_free = swap.xsu_avail;
  }
  return result;
}

std::map<std::string, unsigned long long> read_memory_extras() {
  std::map<std::string, unsigned long long> result;
  const vm_pages vm = read_vm_pages();
  if (!vm.ok) return result;
  result["wired"] = static_cast<unsigned long long>(vm.stats.wire_count) * vm.page_size;
  result["compressed"] = static_cast<unsigned long long>(vm.stats.compressor_page_count) * vm.page_size;
  return result;
}

std::map<std::string, net_sample> read_network() {
  std::map<std::string, net_sample> result;
  try {
    for (const darwin_interfaces::interface_info &nic : darwin_interfaces::read()) {
      net_sample s;
      s.rx_bytes = nic.rx_bytes;
      s.rx_packets = nic.rx_packets;
      s.rx_errors = nic.rx_errors;
      s.tx_bytes = nic.tx_bytes;
      s.tx_packets = nic.tx_packets;
      s.tx_errors = nic.tx_errors;
      s.status = nic.status;
      s.mac = nic.mac;
      s.speed_bps = nic.speed_bps;
      result[nic.name] = s;
    }
  } catch (const std::exception &e) {
    NSC_LOG_ERROR("Failed to read network counters: " + std::string(e.what()));
  }
  return result;
}

std::set<std::string> read_running_exes() {
  std::set<std::string> result;
  const int bytes = proc_listpids(PROC_ALL_PIDS, 0, nullptr, 0);
  if (bytes <= 0) return result;
  // Headroom for processes started between the two calls.
  std::vector<pid_t> pids(static_cast<std::size_t>(bytes) / sizeof(pid_t) + 64);
  const int filled = proc_listpids(PROC_ALL_PIDS, 0, pids.data(), static_cast<int>(pids.size() * sizeof(pid_t)));
  if (filled <= 0) return result;
  pids.resize(static_cast<std::size_t>(filled) / sizeof(pid_t));

  char path[PROC_PIDPATHINFO_MAXSIZE];
  for (const pid_t pid : pids) {
    if (pid <= 0) continue;
    std::string exe;
    // The executable path is readable for every process, whoever owns it.
    if (proc_pidpath(pid, path, sizeof(path)) > 0) {
      const std::string full(path);
      const std::size_t pos = full.find_last_of('/');
      exe = pos == std::string::npos ? full : full.substr(pos + 1);
    } else {
      // Exited in the meantime, or a kernel task without an image: the
      // accounting name (p_comm, up to 16 characters) is better than nothing.
      char name[2 * MAXCOMLEN + 1] = {0};
      if (proc_name(pid, name, sizeof(name)) > 0) exe = name;
    }
    if (!exe.empty()) result.insert(exe);
  }
  return result;
}

}  // namespace collector_source
