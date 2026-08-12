// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_kernel_stats.hpp"

#include <algorithm>
#include <boost/program_options.hpp>
#include <cmath>
#include <cstdio>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <win/pdh/pdh_interface.hpp>
#include <win/pdh/pdh_query.hpp>
#include <win/sysinfo/win_sysinfo.hpp>  // pulls <win/windows.hpp> (Sleep)

namespace po = boost::program_options;

namespace kernel_stats_check {

namespace {
std::string human_rate(const double rate) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.1f/s", rate);
  return buf;
}

bool wanted(const std::vector<std::string> &types, const std::string &name) {
  return types.empty() || std::find(types.begin(), types.end(), name) != types.end();
}

kstat_row rate_row(const std::string &name, const std::string &label, const double rate) {
  kstat_row r;
  r.name = name;
  r.label = label;
  r.rate = rate < 0 ? 0.0 : rate;
  r.current = static_cast<long long>(std::llround(r.rate));
  r.human = human_rate(r.rate);
  return r;
}

kstat_row gauge_row(const std::string &name, const std::string &label, const long long value) {
  kstat_row r;
  r.name = name;
  r.label = label;
  r.rate = 0.0;
  r.current = value < 0 ? 0 : value;
  r.human = std::to_string(r.current);
  return r;
}
}  // namespace

rows_type build_rows(const double ctxt_rate, const double syscalls_rate, const long long processes, const long long threads,
                     const std::vector<std::string> &types) {
  rows_type rows;
  if (wanted(types, "ctxt")) rows.push_back(rate_row("ctxt", "Context Switches", ctxt_rate));
  if (wanted(types, "syscalls")) rows.push_back(rate_row("syscalls", "System Calls", syscalls_rate));
  if (wanted(types, "processes")) rows.push_back(gauge_row("processes", "Processes", processes));
  if (wanted(types, "threads")) rows.push_back(gauge_row("threads", "Threads", threads));
  return rows;
}

filter_obj_handler::filter_obj_handler() {
  registry_.add_string_var("name", &kstat_row::get_name, "Metric name: ctxt, syscalls, processes or threads")
      .add_string_var("label", &kstat_row::get_label, "Human-friendly metric label")
      .add_string_var("human", &kstat_row::get_human, "Human-readable value");
  registry_.add_float("rate", &kstat_row::get_rate, "Per-second rate (0 for the processes/threads gauge rows)").add_float_perf("/s", "", "_rate");
  registry_.add_int_var("current", &kstat_row::get_current, "Gauge value (process/thread count); for the rate rows the rounded per-second rate")
      .add_int_perf("", "", "_current");
}

void check_kernel_stats_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                             const double ctxt_rate, const double syscalls_rate, const long long processes, const long long threads) {
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
  std::vector<std::string> types;

  filter_type filter;
  // Same default thread-count guardrails as the unix check_kernel_stats.
  filter_helper.add_options("name = 'threads' and current > 8000", "name = 'threads' and current > 10000", "", filter.get_filter_syntax(), "ignored");
  filter_helper.add_syntax("${status} - ${list}", "${label} ${human}", "${name}", "", "");
  // clang-format off
  filter_helper.get_desc().add_options()
    ("type", po::value<std::vector<std::string>>(&types), "Select metric type(s) to show: ctxt, syscalls, processes or threads (repeatable; default: all)")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;

  if (!filter_helper.build_filter(filter)) return;

  const rows_type rows = build_rows(ctxt_rate, syscalls_rate, processes, threads, types);
  for (const kstat_row &row : rows) {
    const std::shared_ptr<kstat_row> record(new kstat_row(row));
    filter.match(record);
  }

  filter_helper.post_process(filter);
}

namespace {
// Create a static, English-named counter (same recipe as check_swap_io).
PDH::pdh_instance make_system_counter(PDH::PDHQuery &pdh, const std::string &path, const std::string &alias) {
  PDH::pdh_object obj;
  obj.set_counter(path);
  obj.set_alias(alias);
  obj.set_strategy_static();
  obj.set_type("double");
  obj.set_resolution("english");
  PDH::pdh_instance instance = PDH::factory::create(obj);
  pdh.addCounter(instance);
  return instance;
}
}  // namespace

void check_kernel_stats(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  try {
    PDH::PDHQuery pdh;
    const PDH::pdh_instance ctxt = make_system_counter(pdh, "\\System\\Context Switches/sec", "ctxt");
    const PDH::pdh_instance syscalls = make_system_counter(pdh, "\\System\\System Calls/sec", "syscalls");
    const PDH::pdh_instance processes = make_system_counter(pdh, "\\System\\Processes", "processes");
    const PDH::pdh_instance threads = make_system_counter(pdh, "\\System\\Threads", "threads");

    // The rate counters need two samples an interval apart; the gauges simply
    // read their current value on the second sample.
    pdh.open();
    pdh.collect();
    Sleep(1000);
    pdh.gatherData();
    pdh.close();

    check_kernel_stats_from(request, response, ctxt->get_float_value(), syscalls->get_float_value(), static_cast<long long>(processes->get_float_value()),
                            static_cast<long long>(threads->get_float_value()));
  } catch (const std::exception &e) {
    nscapi::protobuf::functions::set_response_bad(*response, "Failed to sample kernel activity counters: " + std::string(e.what()));
  }
}

}  // namespace kernel_stats_check
