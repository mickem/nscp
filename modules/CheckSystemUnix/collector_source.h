// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// The raw samples the 1 Hz collector (realtime_thread.cpp) turns into CPU
// load, memory, network rates and process history. Everything above this
// header is platform-neutral; each function below has one definition per
// platform: collector_source_linux.cpp reads procfs and sysfs,
// collector_source_darwin.cpp reads Mach host statistics, sysctl and libproc.
//
// A reader that cannot get its data returns an empty result (and logs why)
// rather than zeroes: the collector treats an empty sample as "nothing
// measured", which is what keeps a failed read from reaching a check as a
// measurement.

#include <map>
#include <set>
#include <string>

namespace collector_source {

// Cumulative CPU time for one row, in the platform's tick unit. The aggregate
// row is named "cpu", each core "cpu<N>" with N its zero-based index - the
// /proc/stat spelling, which the Darwin reader adopts so the load calculation
// does not care where the numbers came from.
//
// Darwin has user, nice, system and idle only. iowait, irq, softirq and steal
// stay 0 there: they are folded into the others by the Darwin scheduler, not
// measured as zero.
struct cpu_times {
  std::string name;
  unsigned long long user = 0;
  unsigned long long nice = 0;
  unsigned long long system = 0;
  unsigned long long idle = 0;
  unsigned long long iowait = 0;
  unsigned long long irq = 0;
  unsigned long long softirq = 0;
  unsigned long long steal = 0;

  unsigned long long total_idle() const { return idle + iowait; }
  unsigned long long total_busy() const { return user + nice + system + irq + softirq + steal; }
  unsigned long long total() const { return total_idle() + total_busy(); }
};

// The core index of a cpu_times row: -1 for the aggregate "cpu", N for
// "cpu<N>", and -2 for anything else. Parsed from the name rather than from
// the row's position, because the rows are keyed in a std::map and "cpu10"
// sorts before "cpu2".
int core_index(const std::string &name);

std::map<std::string, cpu_times> read_cpu_times();

// Memory in bytes, in the three views check_memory reports.
//
//  - physical: installed memory and the part of it nothing uses.
//  - cached: the same total, with memory the kernel holds only as a cache
//    (Linux Buffers + Cached, Darwin file-backed + purgeable pages) counted as
//    free, since it is handed back on demand.
//  - swap: swap space. Darwin grows and shrinks its swap files on demand, so
//    both numbers can be 0 on a host that has not needed any yet.
struct memory_sample {
  unsigned long long physical_total = 0;
  unsigned long long physical_free = 0;
  unsigned long long cached_free = 0;
  unsigned long long swap_total = 0;
  unsigned long long swap_free = 0;
};

memory_sample read_memory();

// Memory states a platform tracks beyond the three views above, keyed by a
// name that becomes the metric key under system.mem: Darwin reports `wired`
// (pinned by the kernel) and `compressed` (held by the memory compressor).
// Empty on Linux, which has no counterpart to either.
std::map<std::string, unsigned long long> read_memory_extras();

// Cumulative traffic counters and the link metadata for one interface.
// status is the operational state ("up", "down", "unknown"), mac the hardware
// address as the platform prints it, speed_bps the negotiated link speed (0
// when unknown).
struct net_sample {
  unsigned long long rx_bytes = 0, rx_packets = 0, rx_errors = 0;
  unsigned long long tx_bytes = 0, tx_packets = 0, tx_errors = 0;
  std::string status;
  std::string mac;
  long long speed_bps = 0;
};

std::map<std::string, net_sample> read_network();

// The executable name of every running process, for process history. The
// full name, not the truncated one a kernel keeps for accounting, so it
// matches what check_process reports as `exe`.
std::set<std::string> read_running_exes();

}  // namespace collector_source
