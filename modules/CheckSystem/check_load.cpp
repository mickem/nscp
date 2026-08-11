// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_load.hpp"

#include <cmath>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <str/xtos.hpp>

#include "pdh_thread.hpp"

namespace po = boost::program_options;

namespace load_check {

void load_avg_state::update(const double queue, const double busy_cores, const double elapsed_seconds) {
  const double v = queue + busy_cores;
  if (samples == 0) {
    load1 = load5 = load15 = v;
    queue1 = queue;
  } else {
    // Decay over the interval actually elapsed (the kernel's loadavg formula,
    // sampled on the collector tick instead of every 5 s). Clamped so a stalled
    // or backwards clock cannot freeze the averages or wipe them in one step.
    double dt = elapsed_seconds;
    if (!(dt > 0.001)) dt = 0.001;  // also catches NaN
    if (dt > 900.0) dt = 900.0;
    const double f1 = std::exp(-dt / 60.0);
    const double f5 = std::exp(-dt / 300.0);
    const double f15 = std::exp(-dt / 900.0);
    load1 = load1 * f1 + v * (1.0 - f1);
    load5 = load5 * f5 + v * (1.0 - f5);
    load15 = load15 * f15 + v * (1.0 - f15);
    queue1 = queue1 * f1 + queue * (1.0 - f1);
  }
  last_instant = v;
  ++samples;
}

std::string load_obj::show() const {
  return type + " load average: " + str::xtos_non_sci(load1) + ", " + str::xtos_non_sci(load5) + ", " + str::xtos_non_sci(load15);
}

filter_obj_handler::filter_obj_handler() {
  registry_.add_string_var("type", &load_obj::get_type, "'total' or (with --percpu) 'scaled'");

  // Perf is emitted via the extra() perf-config below; the default perf
  // generator names each metric "<perf-syntax>_<keyword>" (e.g. total_load1),
  // matching the Linux check_load.
  registry_.add_float("load1", &load_obj::get_load1, "Load average over the last 1 minute");
  registry_.add_float("load5", &load_obj::get_load5, "Load average over the last 5 minutes");
  registry_.add_float("load15", &load_obj::get_load15, "Load average over the last 15 minutes");
  registry_.add_float("load", &load_obj::get_load, "The largest of load1, load5 and load15");
  registry_.add_float("queue", &load_obj::get_queue, "Smoothed (1-minute) processor queue length: threads waiting to run, excluding those running");

  registry_.add_int_var("procs_running", &load_obj::get_procs_running, "Number of currently runnable kernel scheduling entities (last-tick estimate)")
      .add_int_var("procs_total", &load_obj::get_procs_total, "Total number of kernel scheduling entities (threads)")
      .add_int_var("cores", &load_obj::get_cores, "Number of logical processors")
      .add_int_var("samples", &load_obj::get_samples, "Collector ticks folded into the averages (the 5/15m values converge as this grows)");
}

load_obj make_load_obj(const load_avg_state &state, const bool percpu) {
  load_obj o;
  o.load1 = state.load1;
  o.load5 = state.load5;
  o.load15 = state.load15;
  o.queue = state.queue1;
  o.procs_running = static_cast<long long>(state.last_instant + 0.5);
  o.procs_total = state.procs_total;
  o.cores = state.cores;
  o.samples = state.samples;
  if (percpu && state.cores > 1) {
    const double d = static_cast<double>(state.cores);
    o.load1 /= d;
    o.load5 /= d;
    o.load15 /= d;
    o.type = "scaled";
  } else {
    o.type = "total";
  }
  return o;
}

void check_load_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                     const load_avg_state &state) {
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);

  filter_type filter;
  bool percpu = false;
  filter_helper.add_options("", "", "", filter.get_filter_syntax(), "ignored");
  filter_helper.add_syntax("${status}: ${list}", "${type} load average: ${load1}, ${load5}, ${load15}", "${type}", "", "");
  // Always emit the three averages as perf data even without warn/crit set.
  filter_helper.set_default_perf_config("extra(load1;load5;load15)");
  // clang-format off
  filter_helper.get_desc().add_options()
    ("percpu", po::value<bool>(&percpu)->implicit_value(true)->default_value(false),
     "Divide the load averages by the number of CPUs (reports the 'scaled' per-core load)")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;

  if (!filter_helper.build_filter(filter)) return;

  const std::shared_ptr<load_obj> record(new load_obj(make_load_obj(state, percpu)));
  filter.match(record);

  filter_helper.post_process(filter);
}

void check_load(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                const std::shared_ptr<pdh_thread> &collector) {
  if (!collector) {
    return nscapi::protobuf::functions::set_response_bad(*response, "Load average data is not available (the CheckSystem collector is not running)");
  }
  const load_avg_state state = collector->get_load_avg();
  if (state.samples == 0) {
    // Zero samples means the collector has not completed a tick yet, or load
    // sampling is turned off. Reporting load=0 here would be a false OK.
    return nscapi::protobuf::functions::set_response_bad(
        *response, "Load average data is not available yet (the collector just started, or load is listed in disable in /settings/system/windows)");
  }
  check_load_from(request, response, state);
}

}  // namespace load_check
