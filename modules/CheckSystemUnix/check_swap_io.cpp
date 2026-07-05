// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_swap_io.h"

#include <unistd.h>

#include <chrono>
#include <fstream>
#include <locale>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <sstream>
#include <thread>

namespace swap_io_check {

namespace {
std::string read_file(const std::string &path) {
  std::ifstream ifs(path.c_str());
  if (!ifs.is_open()) return "";
  std::stringstream ss;
  ss << ifs.rdbuf();
  return ss.str();
}

double rate_of(unsigned long long cur, unsigned long long prev, double dt) {
  if (cur < prev || dt <= 0) return 0.0;
  return static_cast<double>(cur - prev) / dt;
}
}  // namespace

vmstat_swap parse_vmstat_swap(const std::string &content) {
  vmstat_swap out;
  std::istringstream lines(content);
  std::string line;
  bool have_in = false, have_out = false;
  while (std::getline(lines, line)) {
    std::istringstream is(line);
    is.imbue(std::locale("C"));
    std::string key;
    is >> key;
    if (key == "pswpin") {
      is >> out.pswpin;
      have_in = true;
    } else if (key == "pswpout") {
      is >> out.pswpout;
      have_out = true;
    }
  }
  out.valid = have_in && have_out;
  return out;
}

long long count_swaps(const std::string &proc_swaps_content) {
  std::istringstream lines(proc_swaps_content);
  std::string line;
  long long count = 0;
  bool first = true;
  while (std::getline(lines, line)) {
    if (first) {  // header: "Filename  Type  Size  Used  Priority"
      first = false;
      continue;
    }
    if (!line.empty()) ++count;
  }
  return count;
}

swap_obj compute_swap_io(const vmstat_swap &prev, const vmstat_swap &cur, double elapsed_seconds, long long swap_count, long long page_size) {
  swap_obj o;
  o.swap_count = swap_count;
  o.swap_in = rate_of(cur.pswpin, prev.pswpin, elapsed_seconds);
  o.swap_out = rate_of(cur.pswpout, prev.pswpout, elapsed_seconds);
  o.swap_in_bytes = static_cast<long long>(o.swap_in * static_cast<double>(page_size));
  o.swap_out_bytes = static_cast<long long>(o.swap_out * static_cast<double>(page_size));
  return o;
}

filter_obj_handler::filter_obj_handler() {
  registry_.add_string_var("name", &swap_obj::get_name, "Always 'swap' (single aggregate row)");
  registry_.add_int_var("swap_count", &swap_obj::get_swap_count, "Number of active swap devices");
  // Perf is emitted via the extra() perf-config; the default perf generator
  // names each metric "io_<keyword>" (e.g. io_swap_in, io_swap_in_bytes).
  registry_.add_float("swap_in", &swap_obj::get_swap_in, "Pages swapped in per second");
  registry_.add_float("swap_out", &swap_obj::get_swap_out, "Pages swapped out per second");
  registry_.add_int_var("swap_in_bytes", &swap_obj::get_swap_in_bytes, "Bytes swapped in per second");
  registry_.add_int_var("swap_out_bytes", &swap_obj::get_swap_out_bytes, "Bytes swapped out per second");
}

void check_swap_io_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                        const vmstat_swap &prev, const vmstat_swap &cur, double elapsed_seconds, long long swap_count, long long page_size) {
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);

  filter_type filter;
  filter_helper.add_options("", "", "", filter.get_filter_syntax(), "ignored");
  filter_helper.add_syntax("${status}: ${list}", "${swap_count} swap device(s) in ${swap_in} pages/s, out ${swap_out} pages/s", "io", "", "");
  filter_helper.set_default_perf_config("extra(swap_in;swap_out;swap_in_bytes;swap_out_bytes)");

  if (!filter_helper.parse_options()) return;

  if (!filter_helper.build_filter(filter)) return;

  const std::shared_ptr<swap_obj> record(new swap_obj(compute_swap_io(prev, cur, elapsed_seconds, swap_count, page_size)));
  filter.match(record);

  filter_helper.post_process(filter);
}

void check_swap_io(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  const vmstat_swap prev = parse_vmstat_swap(read_file("/proc/vmstat"));
  if (!prev.valid) {
    return nscapi::protobuf::functions::set_response_bad(*response, "Failed to read /proc/vmstat");
  }
  std::this_thread::sleep_for(std::chrono::seconds(1));
  const vmstat_swap cur = parse_vmstat_swap(read_file("/proc/vmstat"));
  if (!cur.valid) {
    return nscapi::protobuf::functions::set_response_bad(*response, "Failed to read /proc/vmstat");
  }
  long long page_size = sysconf(_SC_PAGESIZE);
  if (page_size <= 0) page_size = 4096;
  const long long swap_count = count_swaps(read_file("/proc/swaps"));
  check_swap_io_from(request, response, prev, cur, 1.0, swap_count, page_size);
}

}  // namespace swap_io_check
