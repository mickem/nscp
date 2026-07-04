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

#include "check_cpu_frequency.h"

#include <boost/filesystem.hpp>

#include <cctype>
#include <fstream>
#include <locale>
#include <nscapi/nscapi_metrics_helper.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/where/filter_handler_impl.hpp>

namespace cpu_frequency_check {

namespace {
// Reads a single integer value (one number, in kHz) from a sysfs file and
// returns it converted to MHz. Returns false (leaving out_mhz untouched) when
// the file is missing, unreadable or does not contain a valid number. The
// stream is imbued with the "C" locale so digit grouping/decimal conventions
// of the host locale never interfere with parsing the raw kernel value.
bool read_khz_file_as_mhz(const boost::filesystem::path &file, long long &out_mhz) {
  boost::system::error_code ec;
  if (!boost::filesystem::exists(file, ec) || ec) return false;
  std::ifstream is(file.string().c_str());
  if (!is.is_open()) return false;
  is.imbue(std::locale("C"));
  long long khz = 0;
  if (!(is >> khz)) return false;
  out_mhz = khz / 1000;
  return true;
}
}  // namespace

cpus_type read_cpu_frequency() {
  cpus_type result;

  const boost::filesystem::path base("/sys/devices/system/cpu");
  boost::system::error_code ec;
  if (!boost::filesystem::is_directory(base, ec) || ec) return result;

  long long sum_current = 0;
  long long sum_max = 0;
  long long sum_min = 0;
  long long count = 0;

  boost::filesystem::directory_iterator end;
  for (boost::filesystem::directory_iterator it(base, ec); it != end && !ec; it.increment(ec)) {
    const boost::filesystem::path entry = it->path();
    const std::string name = entry.filename().string();
    // Only consider cpuN directories (cpu followed by a digit), skipping
    // pseudo-entries such as "cpuidle", "cpufreq" or "cpu_capacity".
    if (name.size() < 4 || name.compare(0, 3, "cpu") != 0) continue;
    if (!std::isdigit(static_cast<unsigned char>(name[3]))) continue;

    const boost::filesystem::path cpufreq = entry / "cpufreq";
    if (!boost::filesystem::is_directory(cpufreq, ec) || ec) continue;

    cpu_frequency cpu;
    cpu.name = name;

    // Current frequency: prefer scaling_cur_freq, fall back to cpuinfo_cur_freq.
    // Some hosts (VMs, ARM SoCs, root-only cpuinfo_cur_freq) expose the cpufreq
    // directory but no readable current frequency: skip such cores rather than
    // reporting a bogus 0 MHz that trips frequency_pct thresholds and drags the
    // "total" average down. All cores unreadable then means data.empty(), i.e.
    // the same UNKNOWN as hosts without cpufreq support.
    if (!read_khz_file_as_mhz(cpufreq / "scaling_cur_freq", cpu.current_mhz) && !read_khz_file_as_mhz(cpufreq / "cpuinfo_cur_freq", cpu.current_mhz)) {
      continue;
    }
    read_khz_file_as_mhz(cpufreq / "cpuinfo_max_freq", cpu.max_mhz);
    read_khz_file_as_mhz(cpufreq / "cpuinfo_min_freq", cpu.min_mhz);

    sum_current += cpu.current_mhz;
    sum_max += cpu.max_mhz;
    sum_min += cpu.min_mhz;
    ++count;

    result.push_back(cpu);
  }

  // Append an aggregate "total" entry holding the average across all cores.
  if (count > 0) {
    cpu_frequency total;
    total.name = "total";
    total.current_mhz = sum_current / count;
    total.max_mhz = sum_max / count;
    total.min_mhz = sum_min / count;
    result.push_back(total);
  }

  return result;
}

typedef cpu_frequency filter_obj;

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter_type;

filter_obj_handler::filter_obj_handler() {
  registry_.add_string_var("name", &filter_obj::get_name, "CPU core name");

  // add_int_perf chains onto the last-registered variable, marking it as also
  // emitting a perf entry; so the idiom is add_int_var(...).add_int_perf("UOM").
  registry_.add_int_var("current_mhz", &filter_obj::get_current_mhz, "Current frequency in MHz")
      .add_int_perf("MHz")
      .add_int_var("max_mhz", &filter_obj::get_max_mhz, "Maximum frequency in MHz")
      .add_int_perf("MHz")
      .add_int_var("min_mhz", &filter_obj::get_min_mhz, "Minimum frequency in MHz")
      .add_int_var("frequency_pct", &filter_obj::get_frequency_pct, "Current frequency as percent of max")
      .add_int_perf("%");
}

void check_cpu_frequency(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  cpus_type data = read_cpu_frequency();
  if (data.empty()) {
    return nscapi::protobuf::functions::set_response_bad(*response, "No CPU frequency data available (no cpufreq support?)");
  }

  modern_filter::data_container container;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, container);

  filter_type filter;
  // No default warn/crit thresholds; the user supplies their own. The default
  // filter excludes the synthetic "total" aggregate from per-core matching.
  filter_helper.add_options("", "", "name != 'total'", filter.get_filter_syntax(), "ignored");
  filter_helper.add_syntax("${status}: ${list}", "${name}: ${current_mhz}MHz (${frequency_pct}%)", "${name}", "", "%(status): CPU frequency is ok.");

  if (!filter_helper.parse_options()) return;

  if (!filter_helper.build_filter(filter)) return;

  for (const cpus_type::value_type &v : data) {
    const std::shared_ptr<filter_obj> record(new filter_obj(v));
    filter.match(record);
  }

  filter_helper.post_process(filter);
}

void build_cpu_frequency_metrics(PB::Metrics::MetricsBundle *parent) {
  using namespace nscapi::metrics;

  PB::Metrics::MetricsBundle *bundle = parent->add_children();
  bundle->set_key("cpu_frequency");

  const cpus_type data = read_cpu_frequency();
  for (const cpus_type::value_type &v : data) {
    add_metric(bundle, v.name + ".current_mhz", v.current_mhz);
    add_metric(bundle, v.name + ".max_mhz", v.max_mhz);
    add_metric(bundle, v.name + ".frequency_pct", v.get_frequency_pct());
  }
}

}  // namespace cpu_frequency_check
