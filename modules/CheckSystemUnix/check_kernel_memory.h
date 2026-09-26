// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#ifndef NSCP_CHECK_KERNEL_MEMORY_H
#define NSCP_CHECK_KERNEL_MEMORY_H

#include <boost/optional.hpp>
#include <memory>
#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <string>

namespace kernel_memory_check {

// Kernel memory-manager health: slab usage, file cache and fault rates from
// /proc/meminfo and /proc/vmstat. The unix counterpart to the Windows
// check_kernel_memory: cache and the fault-rate keywords are shared; the
// kernel-allocation gauges keep their platform-native names (slab_* here,
// pool_* on Windows), the same convention as hive vs manager elsewhere.
struct kernel_memory_obj {
  // Linux slab allocator; absent on macOS, whose kernel has no slab.
  boost::optional<long long> slab;                // Slab: total kernel slab allocator bytes
  boost::optional<long long> slab_reclaimable;    // SReclaimable: slab that can be reclaimed (caches)
  boost::optional<long long> slab_unreclaimable;  // SUnreclaim: pinned kernel slab — the leak signal
  // Darwin memory states; absent on Linux. Wired memory is what the kernel
  // has pinned (its own allocations included), the nearest Darwin has to
  // unreclaimable slab; compressed is what the memory compressor holds.
  boost::optional<long long> wired;
  boost::optional<long long> compressed;
  long long cache;      // page-cache bytes (Linux Cached, Darwin file-backed pages)
  double page_faults;   // faults/s (soft + hard; huge on healthy hosts)
  double major_faults;  // faults/s that had to hit disk (Darwin: pageins)

  kernel_memory_obj() : cache(0), page_faults(0.0), major_faults(0.0) {}

  boost::optional<long long> get_slab() const { return slab; }
  boost::optional<long long> get_slab_reclaimable() const { return slab_reclaimable; }
  boost::optional<long long> get_slab_unreclaimable() const { return slab_unreclaimable; }
  boost::optional<long long> get_wired() const { return wired; }
  boost::optional<long long> get_compressed() const { return compressed; }
  long long get_cache() const { return cache; }
  std::string get_slab_human(parsers::where::evaluation_context context) const;
  std::string get_slab_unreclaimable_human(parsers::where::evaluation_context context) const;
  std::string get_wired_human(parsers::where::evaluation_context context) const;
  std::string get_compressed_human(parsers::where::evaluation_context context) const;
  std::string get_cache_human(parsers::where::evaluation_context context) const;
  double get_page_faults() const { return page_faults; }
  double get_major_faults() const { return major_faults; }

  std::string show() const;
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<kernel_memory_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<kernel_memory_obj, filter_obj_handler> filter;

// Byte gauges parsed from /proc/meminfo (values are reported in kB there).
struct meminfo_kernel {
  long long slab;
  long long slab_reclaimable;
  long long slab_unreclaimable;
  long long cache;
  bool valid;
  meminfo_kernel() : slab(0), slab_reclaimable(0), slab_unreclaimable(0), cache(0), valid(false) {}
};

// Cumulative fault counters parsed from /proc/vmstat.
struct vmstat_faults {
  unsigned long long pgfault;
  unsigned long long pgmajfault;
  bool valid;
  vmstat_faults() : pgfault(0), pgmajfault(0), valid(false) {}
};

// Pure parsers / builder (exposed for unit tests).
meminfo_kernel parse_meminfo_kernel(const std::string &content);
vmstat_faults parse_vmstat_faults(const std::string &content);
kernel_memory_obj compute_kernel_memory(const meminfo_kernel &mem, const vmstat_faults &prev, const vmstat_faults &cur, double elapsed_seconds);

// Testable core: renders / thresholds a pre-gathered row. The default detail
// line names the gauges the row has: slab on Linux, wired and compressed on
// macOS.
void check_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                const kernel_memory_obj &data);

// Live check: samples the fault counters over 1 second and reads the gauges.
// Defined per platform (/proc/vmstat and /proc/meminfo on Linux,
// host_statistics64 on Darwin).
void check_kernel_memory(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

}  // namespace kernel_memory_check

#endif  // NSCP_CHECK_KERNEL_MEMORY_H
