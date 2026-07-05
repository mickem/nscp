// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <list>
#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <string>

namespace kernel_stats_check {

// Cumulative kernel counters from /proc/stat.
struct kstat_counters {
  unsigned long long ctxt;       // total context switches since boot
  unsigned long long processes;  // total forks since boot
  bool valid;
  kstat_counters() : ctxt(0), processes(0), valid(false) {}
};

// One reported metric row (ctxt / processes / threads).
struct kstat_row {
  std::string name;
  std::string label;
  std::string human;
  double rate;         // per-second (0 for the instantaneous threads row)
  long long current;   // raw current value (cumulative counter, or thread count)

  kstat_row() : rate(0), current(0) {}

  std::string get_name() const { return name; }
  std::string get_label() const { return label; }
  std::string get_human() const { return human; }
  double get_rate() const { return rate; }
  long long get_current() const { return current; }
  std::string show() const { return label + " " + human; }
};

typedef std::list<kstat_row> rows_type;

// Parse the `ctxt` and `processes` lines from /proc/stat contents.
kstat_counters parse_proc_stat_counters(const std::string &content);

// Count live threads by summing the entries under <proc_root>/<pid>/task for
// every numeric pid. Testable against a fake /proc tree.
long long count_threads_from(const std::string &proc_root);

// Build the metric rows from two counter snapshots taken `elapsed_seconds`
// apart plus a live thread count. `types` (empty = all) selects which of
// ctxt/processes/threads to include.
rows_type build_rows(const kstat_counters &prev, const kstat_counters &cur, double elapsed_seconds, long long thread_count,
                     const std::vector<std::string> &types);

typedef parsers::where::filter_handler_impl<std::shared_ptr<kstat_row> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<kstat_row, filter_obj_handler> filter_type;

// Testable variant: builds rows from pre-sampled snapshots + thread count.
void check_kernel_stats_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                             const kstat_counters &prev, const kstat_counters &cur, double elapsed_seconds, long long thread_count);

void check_kernel_stats(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

}  // namespace kernel_stats_check
