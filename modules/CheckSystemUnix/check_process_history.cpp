/*
 * Copyright (C) 2004-2016 Michael Medin
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

// check_process_history / check_process_history_new render the history of
// processes (by executable name) seen since startup. The collector
// (realtime_thread) accumulates this history once per second when
// `process history = true`; this file mirrors the Windows checks of the same
// name.

#include "check_process_history.h"

#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/program_options.hpp>
#include <nscapi/nscapi_metrics_helper.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <set>
#include <str/format.hpp>

#include "realtime_thread.hpp"

namespace po = boost::program_options;

namespace process_history_check {

static const boost::posix_time::ptime EPOCH(boost::gregorian::date(1970, 1, 1));

std::string process_record::get_first_seen_s() const {
  if (first_seen == 0) return "never";
  return str::format::format_date(EPOCH + boost::posix_time::seconds(first_seen));
}
std::string process_record::get_last_seen_s() const {
  if (last_seen == 0) return "never";
  return str::format::format_date(EPOCH + boost::posix_time::seconds(last_seen));
}

typedef process_record filter_obj;

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter_type;

filter_obj_handler::filter_obj_handler() {
  registry_.add_string_var("exe", &filter_obj::get_exe, "The name of the executable")
      .add_string_var("running", &filter_obj::get_currently_running, "Whether the process is currently running: 'true' or 'false'");

  registry_.add_int_var("first_seen", parsers::where::type_date, &filter_obj::get_first_seen, "Unix timestamp when process was first seen")
      .add_int_var("last_seen", parsers::where::type_date, &filter_obj::get_last_seen, "Unix timestamp when process was last seen")
      .add_int_var("times_seen", &filter_obj::get_times_seen, "Number of times the process has been observed running")
      .add_int_perf("")
      .add_int_var("currently_running", parsers::where::type_bool, &filter_obj::get_currently_running_i, "Whether the process is currently running (1/0)");

  registry_.add_human_string("first_seen", &filter_obj::get_first_seen_s, "When the process was first seen (human readable)")
      .add_human_string("last_seen", &filter_obj::get_last_seen_s, "When the process was last seen (human readable)");
}

namespace {
bool ensure_collector(std::shared_ptr<pdh_thread> collector, PB::Commands::QueryResponseMessage::Response *response) {
  if (!collector) {
    nscapi::protobuf::functions::set_response_bad(*response, "Process history collector not initialized");
    return false;
  }
  if (!collector->has_process_history()) {
    const std::string path = collector->get_settings_path().empty() ? "the module settings" : collector->get_settings_path();
    nscapi::protobuf::functions::set_response_bad(*response, "Process history is not enabled (set 'process history = true' under " + path + ")");
    return false;
  }
  return true;
}
}  // namespace

void check_process_history(std::shared_ptr<pdh_thread> collector, const PB::Commands::QueryRequestMessage::Request &request,
                           PB::Commands::QueryResponseMessage::Response *response) {
  modern_filter::data_container mdata;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, mdata);
  std::vector<std::string> processes;

  filter_type filter;
  filter_helper.add_options("", "", "", filter.get_filter_syntax(), "ok");
  filter_helper.add_syntax("${status}: ${problem_list}", "${exe} (${running})", "${exe}", "", "%(status): ${count} processes in history.");
  // clang-format off
  filter_helper.get_desc().add_options()
      ("process", po::value<std::vector<std::string>>(&processes),
       "Filter to specific process names. Can be specified multiple times. If not specified, all processes in history are shown.")
      ;
  // clang-format on

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;
  if (!ensure_collector(collector, response)) return;

  const history_type data = collector->get_process_history();

  std::set<std::string> requested;
  for (const std::string &p : processes) requested.insert(boost::algorithm::to_lower_copy(p));

  std::set<std::string> found;
  for (const process_record &rec : data) {
    if (!requested.empty()) {
      const std::string exe_lower = boost::algorithm::to_lower_copy(rec.exe);
      found.insert(exe_lower);
      if (requested.find(exe_lower) == requested.end()) continue;
    }
    std::shared_ptr<filter_obj> record(new filter_obj(rec));
    filter.match(record);
  }

  // Any explicitly requested process never seen -> synthesise a not-found row.
  if (!requested.empty()) {
    for (const std::string &proc : requested) {
      if (found.find(proc) == found.end()) {
        process_record not_found_rec;
        not_found_rec.exe = proc;
        std::shared_ptr<filter_obj> record(new filter_obj(not_found_rec));
        filter.match(record);
      }
    }
  }

  filter_helper.post_process(filter);
}

void check_process_history_new(std::shared_ptr<pdh_thread> collector, const PB::Commands::QueryRequestMessage::Request &request,
                               PB::Commands::QueryResponseMessage::Response *response) {
  modern_filter::data_container mdata;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, mdata);
  std::string time_window = "5m";

  filter_type filter;
  filter_helper.add_options("", "", "", filter.get_filter_syntax(), "ok");
  filter_helper.add_syntax("${status}: ${list}", "${exe} (first seen: ${first_seen})", "${exe}", "", "%(status): No new processes found.");
  // clang-format off
  filter_helper.get_desc().add_options()
      ("time", po::value<std::string>(&time_window)->default_value("5m"),
       "Time window to check for new processes (e.g., 5m, 1h, 30s). Processes first seen within this window are considered new.")
      ;
  // clang-format on

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;
  if (!ensure_collector(collector, response)) return;

  const boost::posix_time::ptime now = boost::posix_time::second_clock::universal_time();
  const long long now_ts = (now - EPOCH).total_seconds();

  long long window_seconds;
  try {
    window_seconds = str::format::stox_as_time_sec<long long>(time_window, "s");
  } catch (const std::exception &e) {
    return nscapi::protobuf::functions::set_response_bad(*response, "Invalid time window '" + time_window + "': " + e.what());
  }
  const long long cutoff_ts = now_ts - window_seconds;

  for (const process_record &rec : collector->get_process_history()) {
    if (rec.first_seen >= cutoff_ts) {
      const std::shared_ptr<filter_obj> record(new filter_obj(rec));
      filter.match(record);
    }
  }

  filter_helper.post_process(filter);
}

void build_process_history_metrics(PB::Metrics::MetricsBundle *parent, const history_type &data) {
  using namespace nscapi::metrics;

  PB::Metrics::MetricsBundle *bundle = parent->add_children();
  bundle->set_key("process_history");
  long long running = 0;
  for (const process_record &rec : data) {
    add_metric(bundle, rec.exe + ".times_seen", rec.times_seen);
    add_metric(bundle, rec.exe + ".currently_running", rec.get_currently_running_i());
    if (rec.currently_running) ++running;
  }
  add_metric(bundle, "count", static_cast<long long>(data.size()));
  add_metric(bundle, "running", running);
}

}  // namespace process_history_check
