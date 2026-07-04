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

#include "check_load.h"

#include <unistd.h>

#include <boost/program_options.hpp>
#include <fstream>
#include <locale>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <sstream>
#include <str/utils.hpp>

namespace po = boost::program_options;

namespace load_check {

bool parse_loadavg(const std::string &content, int ncpu, bool percpu, load_obj &out) {
  // /proc/loadavg: "0.52 0.58 0.59 1/834 12345"
  std::istringstream is(content);
  is.imbue(std::locale("C"));
  double l1 = 0, l5 = 0, l15 = 0;
  if (!(is >> l1 >> l5 >> l15)) return false;

  std::string procs;
  if (is >> procs) {
    const std::string::size_type slash = procs.find('/');
    if (slash != std::string::npos) {
      try {
        out.procs_running = std::stoll(procs.substr(0, slash));
        out.procs_total = std::stoll(procs.substr(slash + 1));
      } catch (...) {
      }
    }
  }

  if (percpu && ncpu > 1) {
    const double d = static_cast<double>(ncpu);
    l1 /= d;
    l5 /= d;
    l15 /= d;
    out.type = "scaled";
  } else {
    out.type = "total";
  }
  out.load1 = l1;
  out.load5 = l5;
  out.load15 = l15;
  return true;
}

filter_obj_handler::filter_obj_handler() {
  registry_.add_string_var("type", &load_obj::get_type, "'total' or (with --percpu) 'scaled'");

  // Perf is emitted via the extra() perf-config below; the default perf
  // generator names each metric "<perf-syntax>_<keyword>" (e.g. total_load1).
  registry_.add_float("load1", &load_obj::get_load1, "Load average over the last 1 minute");
  registry_.add_float("load5", &load_obj::get_load5, "Load average over the last 5 minutes");
  registry_.add_float("load15", &load_obj::get_load15, "Load average over the last 15 minutes");
  registry_.add_float("load", &load_obj::get_load, "The largest of load1, load5 and load15");

  registry_.add_int_var("procs_running", &load_obj::get_procs_running, "Number of currently runnable kernel scheduling entities");
  registry_.add_int_var("procs_total", &load_obj::get_procs_total, "Total number of kernel scheduling entities");
}

void check_load_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                     const std::string &loadavg_path) {
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

  std::string content;
  {
    std::ifstream ifs(loadavg_path.c_str());
    if (!ifs.is_open()) {
      return nscapi::protobuf::functions::set_response_bad(*response, "Failed to read " + loadavg_path);
    }
    std::getline(ifs, content);
  }

  long ncpu = sysconf(_SC_NPROCESSORS_ONLN);
  if (ncpu < 1) ncpu = 1;

  const std::shared_ptr<load_obj> record(new load_obj());
  if (!parse_loadavg(content, static_cast<int>(ncpu), percpu, *record)) {
    return nscapi::protobuf::functions::set_response_bad(*response, "Failed to parse " + loadavg_path + ": '" + content + "'");
  }
  filter.match(record);

  filter_helper.post_process(filter);
}

void check_load(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_load_from(request, response, "/proc/loadavg");
}

}  // namespace load_check
