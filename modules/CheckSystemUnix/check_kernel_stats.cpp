// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_kernel_stats.h"

#include <algorithm>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <locale>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <sstream>
#include <thread>

namespace fs = boost::filesystem;
namespace po = boost::program_options;

namespace kernel_stats_check {

namespace {
std::string read_proc_stat() {
  std::ifstream ifs("/proc/stat");
  if (!ifs.is_open()) return "";
  std::stringstream ss;
  ss << ifs.rdbuf();
  return ss.str();
}

std::string human_rate(double rate) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.1f/s", rate);
  return buf;
}

bool wanted(const std::vector<std::string> &types, const std::string &name) {
  return types.empty() || std::find(types.begin(), types.end(), name) != types.end();
}
}  // namespace

kstat_counters parse_proc_stat_counters(const std::string &content) {
  kstat_counters out;
  std::istringstream lines(content);
  std::string line;
  bool have_ctxt = false, have_processes = false;
  while (std::getline(lines, line)) {
    std::istringstream is(line);
    is.imbue(std::locale("C"));
    std::string key;
    is >> key;
    if (key == "ctxt") {
      is >> out.ctxt;
      have_ctxt = true;
    } else if (key == "processes") {
      is >> out.processes;
      have_processes = true;
    }
  }
  out.valid = have_ctxt && have_processes;
  return out;
}

long long count_threads_from(const std::string &proc_root) {
  long long total = 0;
  boost::system::error_code ec;
  fs::directory_iterator it(proc_root, ec), end;
  if (ec) return 0;
  for (; it != end; it.increment(ec)) {
    if (ec) break;
    const std::string name = it->path().filename().string();
    if (name.empty() || !std::all_of(name.begin(), name.end(), ::isdigit)) continue;
    const fs::path task = it->path() / "task";
    boost::system::error_code ec2;
    fs::directory_iterator tit(task, ec2), tend;
    if (ec2) continue;  // process exited between listing and open
    for (; tit != tend; tit.increment(ec2)) {
      if (ec2) break;
      ++total;
    }
  }
  return total;
}

rows_type build_rows(const kstat_counters &prev, const kstat_counters &cur, double elapsed_seconds, long long thread_count,
                     const std::vector<std::string> &types) {
  rows_type rows;
  const double dt = elapsed_seconds > 0 ? elapsed_seconds : 1.0;

  if (wanted(types, "ctxt")) {
    kstat_row r;
    r.name = "ctxt";
    r.label = "Context Switches";
    r.rate = (cur.ctxt >= prev.ctxt) ? static_cast<double>(cur.ctxt - prev.ctxt) / dt : 0.0;
    r.current = static_cast<long long>(cur.ctxt);
    r.human = human_rate(r.rate);
    rows.push_back(r);
  }
  if (wanted(types, "processes")) {
    kstat_row r;
    r.name = "processes";
    r.label = "Process Creations";
    r.rate = (cur.processes >= prev.processes) ? static_cast<double>(cur.processes - prev.processes) / dt : 0.0;
    r.current = static_cast<long long>(cur.processes);
    r.human = human_rate(r.rate);
    rows.push_back(r);
  }
  if (wanted(types, "threads")) {
    kstat_row r;
    r.name = "threads";
    r.label = "Threads";
    r.rate = 0.0;
    r.current = thread_count;
    r.human = std::to_string(thread_count);
    rows.push_back(r);
  }
  return rows;
}

filter_obj_handler::filter_obj_handler() {
  registry_.add_string_var("name", &kstat_row::get_name, "Metric name: ctxt, processes or threads")
      .add_string_var("label", &kstat_row::get_label, "Human-friendly metric label")
      .add_string_var("human", &kstat_row::get_human, "Human-readable value");
  registry_.add_float("rate", &kstat_row::get_rate, "Per-second rate (0 for the threads row)").add_float_perf("/s");
  registry_.add_int_var("current", &kstat_row::get_current, "Current raw value (cumulative counter, or thread count)").add_int_perf("");
}

void check_kernel_stats_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                             const kstat_counters &prev, const kstat_counters &cur, double elapsed_seconds, long long thread_count) {
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
  std::vector<std::string> types;

  filter_type filter;
  filter_helper.add_options("name = 'threads' and current > 8000", "name = 'threads' and current > 10000", "", filter.get_filter_syntax(), "ignored");
  filter_helper.add_syntax("${status} - ${list}", "${label} ${human}", "${name}", "", "");
  // clang-format off
  filter_helper.get_desc().add_options()
    ("type", po::value<std::vector<std::string>>(&types), "Select metric type(s) to show: ctxt, processes or threads (repeatable; default: all)")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;

  if (!filter_helper.build_filter(filter)) return;

  const rows_type rows = build_rows(prev, cur, elapsed_seconds, thread_count, types);
  for (const kstat_row &row : rows) {
    const std::shared_ptr<kstat_row> record(new kstat_row(row));
    filter.match(record);
  }

  filter_helper.post_process(filter);
}

void check_kernel_stats(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  const kstat_counters prev = parse_proc_stat_counters(read_proc_stat());
  if (!prev.valid) {
    return nscapi::protobuf::functions::set_response_bad(*response, "Failed to read /proc/stat");
  }
  std::this_thread::sleep_for(std::chrono::seconds(1));
  const kstat_counters cur = parse_proc_stat_counters(read_proc_stat());
  if (!cur.valid) {
    return nscapi::protobuf::functions::set_response_bad(*response, "Failed to read /proc/stat");
  }
  const long long threads = count_threads_from("/proc");
  check_kernel_stats_from(request, response, prev, cur, 1.0, threads);
}

}  // namespace kernel_stats_check
