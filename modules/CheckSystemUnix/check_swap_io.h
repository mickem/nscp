/*
 * Copyright (C) 2004-2026 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

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
