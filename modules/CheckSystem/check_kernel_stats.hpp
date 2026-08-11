// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <list>
#include <memory>
#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <string>
#include <vector>

namespace kernel_stats_check {

// System-wide kernel activity from the PDH "System" counter set — the Windows
// counterpart to the unix check_kernel_stats (/proc/stat). Shares the row
// model and the ctxt/threads rows; Windows adds syscalls, and its `processes`
// row is a *gauge* (current process count) where the unix row is a fork rate
// (Windows has no process-creation-rate counter in this set). Processor Queue
// Length and System Up Time are deliberately not duplicated here — check_load
// and check_uptime own those.
//
// One reported metric row (ctxt / syscalls / processes / threads).
struct kstat_row {
  std::string name;
  std::string label;
  std::string human;
  double rate;        // per-second (0 for the gauge rows)
  long long current;  // gauge value; for the rate rows the rounded rate (Windows exposes no cumulative counter)

  kstat_row() : rate(0), current(0) {}

  std::string get_name() const { return name; }
  std::string get_label() const { return label; }
  std::string get_human() const { return human; }
  double get_rate() const { return rate; }
  long long get_current() const { return current; }
  std::string show() const { return label + " " + human; }
};

typedef std::list<kstat_row> rows_type;

// Build the metric rows from sampled counter values. `types` (empty = all)
// selects which of ctxt/syscalls/processes/threads to include. Pure, exposed
// for unit tests.
rows_type build_rows(double ctxt_rate, double syscalls_rate, long long processes, long long threads, const std::vector<std::string> &types);

typedef parsers::where::filter_handler_impl<std::shared_ptr<kstat_row> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<kstat_row, filter_obj_handler> filter_type;

// Testable variant: builds rows from pre-sampled values.
void check_kernel_stats_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                             double ctxt_rate, double syscalls_rate, long long processes, long long threads);

// Live check: samples the PDH System counters over a 1 second window (the
// rate counters need two samples; the gauges read on the second).
void check_kernel_stats(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

}  // namespace kernel_stats_check
