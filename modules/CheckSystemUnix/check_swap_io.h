// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <string>

namespace swap_io_check {

// Cumulative page-swap counters from /proc/vmstat.
struct vmstat_swap {
  unsigned long long pswpin;   // pages swapped in since boot
  unsigned long long pswpout;  // pages swapped out since boot
  bool valid;
  vmstat_swap() : pswpin(0), pswpout(0), valid(false) {}
};

// One row of computed swap-paging rates.
struct swap_obj {
  long long swap_count;
  double swap_in;         // pages/s
  double swap_out;        // pages/s
  long long swap_in_bytes;   // bytes/s
  long long swap_out_bytes;  // bytes/s

  swap_obj() : swap_count(0), swap_in(0), swap_out(0), swap_in_bytes(0), swap_out_bytes(0) {}

  std::string get_name() const { return "swap"; }
  long long get_swap_count() const { return swap_count; }
  double get_swap_in() const { return swap_in; }
  double get_swap_out() const { return swap_out; }
  long long get_swap_in_bytes() const { return swap_in_bytes; }
  long long get_swap_out_bytes() const { return swap_out_bytes; }
  std::string show() const { return "swap in " + std::to_string(swap_in) + " pages/s, out " + std::to_string(swap_out) + " pages/s"; }
};

// Parse the pswpin/pswpout lines from /proc/vmstat contents.
vmstat_swap parse_vmstat_swap(const std::string &content);

// Count swap devices from /proc/swaps contents (header line excluded).
long long count_swaps(const std::string &proc_swaps_content);

// Compute paging rates from two snapshots taken `elapsed_seconds` apart.
swap_obj compute_swap_io(const vmstat_swap &prev, const vmstat_swap &cur, double elapsed_seconds, long long swap_count, long long page_size);

typedef parsers::where::filter_handler_impl<std::shared_ptr<swap_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<swap_obj, filter_obj_handler> filter_type;

// Testable variant: computes from pre-sampled snapshots.
void check_swap_io_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                        const vmstat_swap &prev, const vmstat_swap &cur, double elapsed_seconds, long long swap_count, long long page_size);

void check_swap_io(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

}  // namespace swap_io_check
