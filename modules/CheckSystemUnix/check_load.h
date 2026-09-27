// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <algorithm>
#include <boost/optional.hpp>
#include <functional>
#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <string>

namespace load_check {

// One row of load-average data. When percpu is requested the three averages
// are pre-divided by the CPU count and `type` becomes "scaled".
struct load_obj {
  std::string type;  // "total" or "scaled"
  double load1;
  double load5;
  double load15;
  // Runnable and total kernel scheduling entities (threads). Each is only
  // meaningful when its has_ flag is set: /proc/loadavg carries both, Darwin
  // knows the thread count but keeps no count of runnable threads.
  long long procs_running;
  long long procs_total;
  bool has_procs_running;
  bool has_procs_total;

  load_obj() : type("total"), load1(0), load5(0), load15(0), procs_running(0), procs_total(0), has_procs_running(false), has_procs_total(false) {}

  std::string get_type() const { return type; }
  double get_load1() const { return load1; }
  double get_load5() const { return load5; }
  double get_load15() const { return load15; }
  double get_load() const { return std::max(load1, std::max(load5, load15)); }
  boost::optional<long long> get_procs_running() const { return has_procs_running ? boost::optional<long long>(procs_running) : boost::none; }
  boost::optional<long long> get_procs_total() const { return has_procs_total ? boost::optional<long long>(procs_total) : boost::none; }

  std::string show() const {
    return type + " load average: " + std::to_string(load1) + ", " + std::to_string(load5) + ", " + std::to_string(load15);
  }
};

// Parse the contents of /proc/loadavg. `ncpu` (>=1) and `percpu` control
// whether the averages are divided by the CPU count. Returns false on a
// malformed first field.
bool parse_loadavg(const std::string &content, int ncpu, bool percpu, load_obj &out);

// Divide the three averages by `ncpu` when `percpu` is set (and there is more
// than one CPU), and set `type` to match. parse_loadavg ends with this; a
// reader that gets the averages some other way calls it itself.
void apply_percpu(load_obj &out, int ncpu, bool percpu);

typedef parsers::where::filter_handler_impl<std::shared_ptr<load_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<load_obj, filter_obj_handler> filter_type;

// Reads one load sample: fills `out` (applying --percpu) and returns true, or
// returns false with `error` saying what could not be read.
typedef std::function<bool(bool percpu, load_obj &out, std::string &error)> load_reader;

// The check, given a way to read the load: parses the options, reads, and
// renders the thresholds.
void check_load_with(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                     const load_reader &read);

// Testable variant: reads the load average from an explicit file path in
// /proc/loadavg format.
void check_load_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                     const std::string &loadavg_path);

// The live check. Defined per platform (live_source_linux.cpp reads
// /proc/loadavg, live_source_darwin.cpp getloadavg(3) and the Mach thread
// count).
void check_load(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

}  // namespace load_check
