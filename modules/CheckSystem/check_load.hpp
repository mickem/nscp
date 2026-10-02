// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/optional.hpp>
#include <functional>
#include <memory>
#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <string>

class pdh_thread;

namespace load_check {

// Unix-style 1/5/15-minute load averages synthesised on the CheckSystem 1 Hz
// collector tick. Each tick folds the instantaneous value
//
//   load = processor queue length + busy cores
//
// into three exponential moving averages, reproducing the Linux load-average
// semantics (running + runnable tasks): a fully-busy 8-core box reads ~8.0 and
// a saturated one reads above it. The queue length comes from the PDH counter
// `\System\Processor Queue Length` (threads waiting to run, system-wide) and
// busy cores from the same tick's CPU sample (cores x busy%).
struct load_avg_state {
  double load1;           // 1-minute EMA of queue + busy cores
  double load5;           // 5-minute EMA
  double load15;          // 15-minute EMA
  double queue1;          // 1-minute EMA of the raw queue depth alone
  double last_instant;    // last tick's instantaneous queue + busy cores
  long long samples;      // ticks folded in so far (0 = no data yet)
  long long procs_total;  // total threads on the system (scheduling entities)
  long long cores;        // logical processor count

  load_avg_state() : load1(0.0), load5(0.0), load15(0.0), queue1(0.0), last_instant(0.0), samples(0), procs_total(0), cores(0) {}

  // Fold one collector sample into the averages. The first sample seeds the
  // EMAs directly (an unbiased point estimate) instead of decaying up from
  // zero, so a freshly restarted agent does not under-report load for minutes.
  //
  // elapsed_seconds is the wall time since the previous sample: the collector
  // aims for 1 Hz but explicitly runs long when a tick overruns, and assuming
  // a fixed second there would under-decay the averages exactly on the busy
  // hosts this check exists for. Values outside [0.001, 900] are clamped.
  void update(double queue, double busy_cores, double elapsed_seconds);
};

// One aggregate row exposed to the filter (mirrors the Unix check_load
// keywords, plus the Windows-specific queue/cores/samples extras).
struct load_obj {
  std::string type;  // 'total' or (with --percpu) 'scaled'
  double load1;
  double load5;
  double load15;
  double queue;             // smoothed (1-minute EMA) processor queue length
  long long procs_running;  // last tick's instantaneous runnable + running estimate
  long long procs_total;    // total threads on the system
  long long cores;          // logical processor count
  long long samples;        // collector ticks folded into the averages

  load_obj() : load1(0.0), load5(0.0), load15(0.0), queue(0.0), procs_running(0), procs_total(0), cores(0), samples(0) {}

  std::string get_type() const { return type; }
  double get_load1() const { return load1; }
  double get_load5() const { return load5; }
  double get_load15() const { return load15; }
  double get_load() const { return load1 > load5 ? (load1 > load15 ? load1 : load15) : (load5 > load15 ? load5 : load15); }
  double get_queue() const { return queue; }
  long long get_procs_running() const { return procs_running; }
  long long get_procs_total() const { return procs_total; }
  long long get_cores() const { return cores; }
  long long get_samples() const { return samples; }

  std::string show() const;
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<load_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<load_obj, filter_obj_handler> filter_type;

// Build the filter row from the collector state; percpu divides the three
// averages by the core count (type becomes 'scaled'). The queue is left
// unscaled: it is an absolute thread count, not a per-core ratio.
load_obj make_load_obj(const load_avg_state &state, bool percpu);

// What check_load gathered from the collector. Each condition is only acted
// on after the options are parsed, so help, show-default and an invalid option
// are answered as such whatever state the collector is in.
struct load_reading {
  bool collector_running = true;
  bool disabled = false;
  // None when the collector's lock could not be had: a busy collector is
  // neither a fresh one nor a disabled one.
  boost::optional<load_avg_state> state;

  load_reading() = default;
  // A plain snapshot, as the unit tests hand it in.
  load_reading(const load_avg_state &s) : state(s) {}  // NOLINT(google-explicit-constructor)
};

// Testable core: renders / thresholds what `read` gathers from the collector.
// `read` is only called once the options have been parsed, so help,
// show-default and an invalid option never wait on the collector's lock.
void check_load_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                     const std::function<load_reading()> &read);
// The same, for a snapshot gathered up front (the unit tests).
void check_load_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                     const load_reading &reading);

// Live check: reads the collector's load-average state.
void check_load(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                const std::shared_ptr<pdh_thread> &collector);

}  // namespace load_check
