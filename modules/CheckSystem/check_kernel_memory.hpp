// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <memory>
#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <string>

namespace kernel_memory_check {

// Kernel memory-manager health: pool usage, file cache and fault rates from
// the PDH "Memory" object. Complements check_memory (used/free/size of
// physical/committed/virtual) — pool exhaustion and hard-fault storms are the
// failure modes free-RAM thresholds do not catch. Pools have no meaningful
// maximum on modern Windows, so thresholds are absolute bytes: baseline the
// host, then pin (e.g. crit=pool_nonpaged > 4G).
struct kernel_memory_obj {
  long long pool_paged;      // Pool Paged Bytes
  long long pool_nonpaged;   // Pool Nonpaged Bytes (the driver-leak signal)
  long long cache;           // Cache Bytes (system file cache working set)
  double page_faults;        // Page Faults/sec (soft + hard; huge on healthy hosts)
  double transition_faults;  // Transition Faults/sec (the dominant soft-fault kind)
  double hard_faults;        // Page Reads/sec (hard-fault events that had to hit disk)

  kernel_memory_obj() : pool_paged(0), pool_nonpaged(0), cache(0), page_faults(0.0), transition_faults(0.0), hard_faults(0.0) {}

  long long get_pool_paged() const { return pool_paged; }
  long long get_pool_nonpaged() const { return pool_nonpaged; }
  long long get_cache() const { return cache; }
  std::string get_pool_paged_human(parsers::where::evaluation_context context) const;
  std::string get_pool_nonpaged_human(parsers::where::evaluation_context context) const;
  std::string get_cache_human(parsers::where::evaluation_context context) const;
  double get_page_faults() const { return page_faults; }
  double get_transition_faults() const { return transition_faults; }
  double get_hard_faults() const { return hard_faults; }

  std::string show() const;
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<kernel_memory_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<kernel_memory_obj, filter_obj_handler> filter_type;

// Build the row from raw counter readings, clamping the negative values PDH
// can report on the first sample of a rate counter. Pure, exposed for tests.
kernel_memory_obj make_kernel_memory_obj(long long pool_paged, long long pool_nonpaged, long long cache, double page_faults, double transition_faults,
                                         double hard_faults);

// Testable core: renders / thresholds a pre-gathered row.
void check_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                const kernel_memory_obj &data);

// Live check: samples the PDH Memory counters over a 1 second window (the
// fault counters are rates and need two samples).
void check_kernel_memory(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

}  // namespace kernel_memory_check
