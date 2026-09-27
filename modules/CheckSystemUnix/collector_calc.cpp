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

unsigned long long counter_delta(const unsigned long long cur, const unsigned long long prev, const bool counters_32bit) {
  if (cur >= prev) return cur - prev;
  if (counters_32bit && prev <= 0xFFFFFFFFull) return cur + 0x100000000ull - prev;
  return 0ull;
}

cpu_times times_delta(const cpu_times &prev, const cpu_times &cur) {
  const bool w = cur.counters_32bit;
  cpu_times d;
  d.name = cur.name;
  d.counters_32bit = w;
  d.user = counter_delta(cur.user, prev.user, w);
  d.nice = counter_delta(cur.nice, prev.nice, w);
  d.system = counter_delta(cur.system, prev.system, w);
  d.idle = counter_delta(cur.idle, prev.idle, w);
  d.iowait = counter_delta(cur.iowait, prev.iowait, w);
  d.irq = counter_delta(cur.irq, prev.irq, w);
  d.softirq = counter_delta(cur.softirq, prev.softirq, w);
  d.steal = counter_delta(cur.steal, prev.steal, w);
  return d;
}

bool aggregate_delta(const std::map<std::string, cpu_times> &prev, const std::map<std::string, cpu_times> &cur, cpu_times &out) {
  out = cpu_times();
  out.name = "cpu";
  const auto total_prev = prev.find("cpu");
  const auto total_cur = cur.find("cpu");
  if (total_prev != prev.end() && total_cur != cur.end()) {
    out = times_delta(total_prev->second, total_cur->second);
    return true;
  }
  bool any = false;
  for (const auto &entry : cur) {
    if (core_index(entry.first) < 0) continue;
    const auto old_it = prev.find(entry.first);
    if (old_it == prev.end()) continue;
    const cpu_times d = times_delta(old_it->second, entry.second);
    out.user += d.user;
    out.nice += d.nice;
    out.system += d.system;
    out.idle += d.idle;
    out.iowait += d.iowait;
    out.irq += d.irq;
    out.softirq += d.softirq;
    out.steal += d.steal;
    any = true;
  }
  return any;
}
}  // namespace collector_source

namespace collector_calc {

namespace {
load_entry to_load_entry(const collector_source::cpu_times &d, const int index) {
  const unsigned long long user = d.user + d.nice;
  const unsigned long long kernel = d.system + d.irq + d.softirq + d.steal;
  const unsigned long long idle = d.idle + d.iowait;
  unsigned long long total_diff = user + kernel + idle;
  if (total_diff == 0) total_diff = 1;

  const auto pct = [total_diff](const unsigned long long v) {
    return std::max(0.0, std::min(100.0, 100.0 * static_cast<double>(v) / static_cast<double>(total_diff)));
  };
  return load_entry(pct(idle), pct(user), pct(kernel), index);
}
}  // namespace

cpu_load calculate_cpu_load(const std::map<std::string, collector_source::cpu_times> &old_times,
                            const std::map<std::string, collector_source::cpu_times> &new_times) {
  cpu_load result;

  // Per core, each counter measured across a wrap where the platform's
  // counters are 32-bit (see collector_source::counter_delta).
  for (const auto &entry : new_times) {
    const int index = collector_source::core_index(entry.first);
    if (index < 0) continue;
    const auto old_it = old_times.find(entry.first);
    if (old_it == old_times.end()) continue;
    // Numbered by N rather than by position: the map orders "cpu10" before
    // "cpu2", which used to hand core 2 the numbers of core 10 on any machine
    // with more than ten cores.
    result.core.push_back(to_load_entry(collector_source::times_delta(old_it->second, entry.second), index));
  }
  std::sort(result.core.begin(), result.core.end(), [](const load_entry &a, const load_entry &b) { return a.core < b.core; });

  // The whole machine: the "cpu" row on Linux, the sum of the corrected
  // per-core deltas on Darwin, which has no aggregate row to compare.
  collector_source::cpu_times total;
  if (collector_source::aggregate_delta(old_times, new_times, total)) result.total = to_load_entry(total, -1);

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
