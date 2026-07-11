// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <string>

namespace swap_io_check {

// One aggregate row of paging (swap) I/O rates. On Windows the source is the
// system paging counters (\Memory\Pages Input/sec and \Memory\Pages Output/sec):
// pages moved between disk and physical memory. Windows has no per-pagefile I/O
// counter, so this is a single system-wide row. Field names deliberately mirror
// the Linux `check_swap_io` so warning/critical expressions and detail-syntax
// port between platforms.
struct swap_obj {
  long long swap_count;      // number of page files on the system
  double swap_in;            // pages/s paged in from disk (Pages Input/sec)
  double swap_out;           // pages/s paged out to disk (Pages Output/sec)
  long long swap_in_bytes;   // bytes/s paged in
  long long swap_out_bytes;  // bytes/s paged out

  swap_obj() : swap_count(0), swap_in(0), swap_out(0), swap_in_bytes(0), swap_out_bytes(0) {}

  std::string get_name() const { return "swap"; }
  long long get_swap_count() const { return swap_count; }
  double get_swap_in() const { return swap_in; }
  double get_swap_out() const { return swap_out; }
  long long get_swap_in_bytes() const { return swap_in_bytes; }
  long long get_swap_out_bytes() const { return swap_out_bytes; }
  std::string show() const { return "swap in " + std::to_string(swap_in) + " pages/s, out " + std::to_string(swap_out) + " pages/s"; }
};

// Build a row from sampled paging rates (pages/s) and the system page size
// (bytes). Negative rates (which PDH can briefly report around counter wrap)
// are clamped to 0.
swap_obj make_swap_obj(double pages_in_per_sec, double pages_out_per_sec, long long swap_count, long long page_size);

typedef parsers::where::filter_handler_impl<std::shared_ptr<swap_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<swap_obj, filter_obj_handler> filter_type;

// Testable core: render / threshold a pre-sampled row without touching PDH.
void check_swap_io_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                        double pages_in_per_sec, double pages_out_per_sec, long long swap_count, long long page_size);

// Live check: samples the Windows paging counters over ~1s and thresholds them.
void check_swap_io(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

}  // namespace swap_io_check
