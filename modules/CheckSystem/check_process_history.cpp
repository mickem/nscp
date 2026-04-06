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

#include "check_process_history.hpp"

#include <Windows.h>

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/locks.hpp>
#include <nscapi/nscapi_metrics_helper.hpp>
#include <nsclient/nsclient_exception.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <parsers/where/node.hpp>
#include <str/format.hpp>
#include <str/xtos.hpp>
#include <win/processes.hpp>

constexpr boost::posix_time::ptime EPOCH(boost::gregorian::date(1970, 1, 1));

namespace process_history_check {

std::string process_record::get_first_seen_s() const {
  const boost::posix_time::ptime t = EPOCH + boost::posix_time::seconds(first_seen);
  return str::format::format_date(t);
}

std::string process_record::get_last_seen_s() const {
  const boost::posix_time::ptime t = EPOCH + boost::posix_time::seconds(last_seen);
  return str::format::format_date(t);
}

std::string process_record::show() const {
  return exe + " (seen " + str::xtos(times_seen) + " times, " + (currently_running ? "running" : "not running") + ")";
}

void process_record::build_metrics(PB::Metrics::MetricsBundle *section) const {
  using namespace nscapi::metrics;
  add_metric(section, exe + ".first_seen", first_seen);
  add_metric(section, exe + ".last_seen", last_seen);
  add_metric(section, exe + ".times_seen", times_seen);
  add_metric(section, exe + ".currently_running", get_currently_running());
}

// Simple error reporter that does nothing (we don't want to spam logs during enumeration)
struct silent_error_reporter : public win_list_processes::error_reporter {
  void report_error(std::string) override {}
  void report_warning(std::string) override {}
  void report_debug(std::string) override {}
};

void process_history_data::fetch() {
  if (!fetch_history_) return;

  // Get current timestamp
  const boost::posix_time::ptime now = boost::posix_time::second_clock::universal_time();
  const long long now_ts = (now - EPOCH).total_seconds();

  // Enumerate current processes (fast scan, no deep info needed)
  silent_error_reporter err;
  const win_list_processes::process_list current_processes = win_list_processes::enumerate_processes(true, false, false, &err);

  // Build set of currently running processes (lowercase for case-insensitive comparison)
  std::set<std::string> running_now;
  for (const auto &proc : current_processes) {
    std::string exe_lower = boost::algorithm::to_lower_copy(proc.exe.get());
    if (!exe_lower.empty()) {
      running_now.insert(exe_lower);
    }
  }

  {
    boost::unique_lock<boost::shared_mutex> write_lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
    if (!write_lock.owns_lock()) {
      throw nsclient::nsclient_exception("Failed to get mutex for writing process history data");
    }

    // First pass: Update history with current processes (before marking anything as not running)
    for (const auto &proc : current_processes) {
      std::string exe_lower = boost::algorithm::to_lower_copy(proc.exe.get());
      if (exe_lower.empty()) continue;

      auto it = history_.find(exe_lower);
      if (it == history_.end()) {
        // New process - add to history
        process_record record;
        record.exe = proc.exe.get();  // Keep original case
        record.first_seen = now_ts;
        record.last_seen = now_ts;
        record.times_seen = 1;
        record.currently_running = true;
        history_[exe_lower] = record;
      } else {
        // Existing process - update
        it->second.last_seen = now_ts;
        if (!it->second.currently_running) {
          it->second.times_seen++;  // Only increment if process wasn't running last check (counts "starts")
        }
        it->second.currently_running = true;
      }
    }

    // Second pass: Mark processes not in running_now as not currently running
    for (auto &entry : history_) {
      if (running_now.find(boost::algorithm::to_lower_copy(entry.second.exe)) == running_now.end()) {
        entry.second.currently_running = false;
      }
    }
  }
}

history_type process_history_data::get() {
  boost::shared_lock<boost::shared_mutex> read_lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!read_lock.owns_lock()) {
    throw nsclient::nsclient_exception("Failed to get mutex for reading process history data");
  }

  history_type result;
  for (const auto &entry : history_) {
    result.push_back(entry.second);
  }
  return result;
}

void process_history_data::clear() {
  boost::unique_lock<boost::shared_mutex> write_lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!write_lock.owns_lock()) {
    throw nsclient::nsclient_exception("Failed to get mutex for clearing process history data");
  }
  history_.clear();
}

long long process_history_data::get_count() {
  boost::shared_lock<boost::shared_mutex> read_lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!read_lock.owns_lock()) {
    return 0;
  }
  return static_cast<long long>(history_.size());
}

void process_history_data::build_metrics(PB::Metrics::MetricsBundle *section) {
  using namespace nscapi::metrics;
  add_metric(section, "unique_processes", get_count());
}

namespace check {

typedef process_record filter_obj;

typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj>> native_context;
struct filter_obj_handler final : native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter_type;

filter_obj_handler::filter_obj_handler() {
  // clang-format off
  registry_.add_string("exe", &filter_obj::get_exe, "The name of the executable")
      .add_string("running", &filter_obj::get_currently_running, "Whether the process is currently running: 'true' or 'false'");

  registry_.add_int_x("first_seen", parsers::where::type_date, &filter_obj::get_first_seen, "Unix timestamp when process was first seen")
      .add_int_x("last_seen", parsers::where::type_date, &filter_obj::get_last_seen, "Unix timestamp when process was last seen")
      .add_int_perf("")
      .add_int_x("times_seen", &filter_obj::get_times_seen, "Number of times the process has been observed running")
      .add_int_perf("")
      .add_int_x("currently_running", parsers::where::type_bool, &filter_obj::get_currently_running_i, "Whether the process is currently running (1/0)")
      .add_int_perf("");
  // clang-format on

  registry_.add_human_string("first_seen", &filter_obj::get_first_seen_s, "When the process was first seen (human readable)")
      .add_human_string("last_seen", &filter_obj::get_last_seen_s, "When the process was last seen (human readable)");
}

void check_process_history(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                           history_type data) {
  namespace po = boost::program_options;

  modern_filter::data_container mdata;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, mdata);
  std::vector<std::string> processes;

  filter_type filter;
  filter_helper.add_options("", "", "", filter.get_filter_syntax(), "unknown");
  filter_helper.add_syntax("${status}: ${problem_list}", "${exe} (${running})", "${exe}", "", "%(status): ${count} processes in history.");

  // clang-format off
  filter_helper.get_desc().add_options()
      ("process", po::value<std::vector<std::string>>(&processes),
       "Filter to specific process names. Can be specified multiple times. If not specified, all processes in history are shown.")
      ;
  // clang-format on

  if (!filter_helper.parse_options()) return;

  if (!filter_helper.build_filter(filter)) return;

  // Build a set of requested processes for filtering (case-insensitive)
  std::set<std::string> requested;
  for (const std::string &p : processes) {
    requested.insert(boost::algorithm::to_lower_copy(p));
  }

  for (const process_record &rec : data) {
    // If specific processes requested, filter to only those
    if (!requested.empty()) {
      std::string exe_lower = boost::algorithm::to_lower_copy(rec.exe);
      if (requested.find(exe_lower) == requested.end()) {
        continue;
      }
    }

    boost::shared_ptr<filter_obj> record(new filter_obj(rec));
    filter.match(record);
  }

  // If specific processes were requested, check for any that were NOT found in history
  if (!requested.empty()) {
    std::set<std::string> found;
    for (const process_record &rec : data) {
      found.insert(boost::algorithm::to_lower_copy(rec.exe));
    }

    for (const std::string &proc : requested) {
      if (found.find(proc) == found.end()) {
        // Process was requested but never seen - create a placeholder record
        process_record not_found_rec;
        not_found_rec.exe = proc;
        not_found_rec.first_seen = 0;
        not_found_rec.last_seen = 0;
        not_found_rec.times_seen = 0;
        not_found_rec.currently_running = false;

        boost::shared_ptr<filter_obj> record(new filter_obj(not_found_rec));
        filter.match(record);
      }
    }
  }

  filter_helper.post_process(filter);
}

void check_process_history_new(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                               const history_type &data) {
  namespace po = boost::program_options;

  modern_filter::data_container mdata;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, mdata);
  std::string time_window = "5m";

  filter_type filter;
  filter_helper.add_options("", "", "", filter.get_filter_syntax(), "warning");
  filter_helper.add_syntax("${status}: ${list}", "${exe} (first seen: ${first_seen})", "${exe}", "", "%(status): No new processes found.");

  // clang-format off
  filter_helper.get_desc().add_options()
      ("time", po::value<std::string>(&time_window)->default_value("5m"),
       "Time window to check for new processes (e.g., 5m, 1h, 30s). Processes first seen within this window are considered new.")
      ;
  // clang-format on

  if (!filter_helper.parse_options()) return;

  if (!filter_helper.build_filter(filter)) return;

  // Get current timestamp
  const boost::posix_time::ptime now = boost::posix_time::second_clock::universal_time();
  const long long now_ts = (now - EPOCH).total_seconds();

  // Parse time window to seconds
  const long long window_seconds = str::format::stox_as_time_sec<long long>(time_window, "s");
  const long long cutoff_ts = now_ts - window_seconds;

  for (const process_record &rec : data) {
    // Only include processes first seen within the time window
    if (rec.first_seen >= cutoff_ts) {
      const boost::shared_ptr<filter_obj> record(new filter_obj(rec));
      filter.match(record);
    }
  }

  filter_helper.post_process(filter);
}

}  // namespace check

}  // namespace process_history_check
