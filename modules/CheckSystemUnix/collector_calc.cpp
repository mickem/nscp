// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The pure half of the 1 Hz collector: turning two raw samples from
// collector_source into the CPU load, memory and network figures the checks
// report. Platform-neutral, and unit-tested apart from the thread.

#include <algorithm>

#include "collector_source.h"
#include "realtime_thread.hpp"

namespace collector_source {
int core_index(const std::string &name) {
  if (name == "cpu") return -1;
  if (name.size() <= 3 || name.compare(0, 3, "cpu") != 0) return -2;
  int index = 0;
  for (std::string::size_type i = 3; i < name.size(); ++i) {
    if (name[i] < '0' || name[i] > '9') return -2;
    if (index > 100000) return -2;
    index = index * 10 + (name[i] - '0');
  }
  return index;
}
}  // namespace collector_source

namespace collector_calc {

cpu_load calculate_cpu_load(const std::map<std::string, collector_source::cpu_times> &old_times,
                            const std::map<std::string, collector_source::cpu_times> &new_times) {
  cpu_load result;

  for (const auto &entry : new_times) {
    const std::string &name = entry.first;
    const collector_source::cpu_times &new_ct = entry.second;
    const int index = collector_source::core_index(name);
    if (index == -2) continue;

    auto old_it = old_times.find(name);
    if (old_it == old_times.end()) continue;

    const collector_source::cpu_times &old_ct = old_it->second;

    unsigned long long total_diff = new_ct.total() - old_ct.total();
    if (total_diff == 0) total_diff = 1;

    double user_pct = 100.0 * (new_ct.user + new_ct.nice - old_ct.user - old_ct.nice) / total_diff;
    double kernel_pct =
        100.0 * (new_ct.system + new_ct.irq + new_ct.softirq + new_ct.steal - old_ct.system - old_ct.irq - old_ct.softirq - old_ct.steal) / total_diff;
    double idle_pct = 100.0 * (new_ct.total_idle() - old_ct.total_idle()) / total_diff;

    // Clamp values
    user_pct = std::max(0.0, std::min(100.0, user_pct));
    kernel_pct = std::max(0.0, std::min(100.0, kernel_pct));
    idle_pct = std::max(0.0, std::min(100.0, idle_pct));

    if (index == -1) {
      // Total CPU
      result.total = load_entry(idle_pct, user_pct, kernel_pct, -1);
    } else {
      // Individual core (cpuN), numbered by N rather than by position: the
      // map orders "cpu10" before "cpu2", which used to hand core 2 the
      // numbers of core 10 on any machine with more than ten cores.
      result.core.push_back(load_entry(idle_pct, user_pct, kernel_pct, index));
    }
  }
  std::sort(result.core.begin(), result.core.end(), [](const load_entry &a, const load_entry &b) { return a.core < b.core; });

  result.cores = static_cast<int>(result.core.size());
  return result;
}

memory_info to_memory_info(const collector_source::memory_sample &sample) {
  memory_info result;
  result.physical.total = sample.physical_total;
  result.physical.free = sample.physical_free;
  result.cached.total = sample.physical_total;
  result.cached.free = std::min(sample.cached_free, sample.physical_total);
  result.swap.total = sample.swap_total;
  result.swap.free = sample.swap_free;
  return result;
}

network_check::nics_type calculate_network(const std::map<std::string, collector_source::net_sample> &old_c,
                                           const std::map<std::string, collector_source::net_sample> &new_c, double dt) {
  network_check::nics_type result;
  if (dt <= 0) dt = 1.0;
  auto delta = [](unsigned long long cur, unsigned long long old) { return cur >= old ? cur - old : 0ull; };

  for (const auto &entry : new_c) {
    const std::string &name = entry.first;
    const collector_source::net_sample &nc = entry.second;

    network_check::network_interface nif;
    nif.name = name;
    nif.rx_errors = static_cast<long long>(nc.rx_errors);
    nif.tx_errors = static_cast<long long>(nc.tx_errors);

    auto it = old_c.find(name);
    if (it != old_c.end()) {
      const collector_source::net_sample &oc = it->second;
      nif.rx_bytes_per_sec = static_cast<long long>(delta(nc.rx_bytes, oc.rx_bytes) / dt);
      nif.tx_bytes_per_sec = static_cast<long long>(delta(nc.tx_bytes, oc.tx_bytes) / dt);
      nif.rx_packets_per_sec = static_cast<long long>(delta(nc.rx_packets, oc.rx_packets) / dt);
      nif.tx_packets_per_sec = static_cast<long long>(delta(nc.tx_packets, oc.tx_packets) / dt);
    }

    nif.status = nc.status.empty() ? "unknown" : nc.status;
    nif.mac = nc.mac;
    nif.speed_bps = nc.speed_bps;

    result.push_back(nif);
  }
  return result;
}

}  // namespace collector_calc
