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

#include "check_cpu_frequency.hpp"

#include <boost/thread/locks.hpp>
#include <nscapi/nscapi_metrics_helper.hpp>
#include <nsclient/nsclient_exception.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

namespace cpu_frequency_check {

// Win32_Processor provides per-socket CPU info including current and max clock speed.
std::string helper::query =
    "select Name, CurrentClockSpeed, MaxClockSpeed, NumberOfCores, NumberOfLogicalProcessors"
    " from Win32_Processor";
std::string helper::ns = "root\\CIMV2";

void cpu_frequency::read_wmi(const wmi_impl::row &r) {
  name = r.get_string("Name");
  current_mhz = r.get_int("CurrentClockSpeed");
  max_mhz = r.get_int("MaxClockSpeed");
  number_of_cores = r.get_int("NumberOfCores");
  number_of_logical_processors = r.get_int("NumberOfLogicalProcessors");
}

void cpu_frequency::build_metrics(PB::Metrics::MetricsBundle *section) const {
  using namespace nscapi::metrics;
  add_metric(section, name + ".current_mhz", current_mhz);
  add_metric(section, name + ".max_mhz", max_mhz);
  add_metric(section, name + ".frequency_pct", get_frequency_pct());
  add_metric(section, name + ".cores", number_of_cores);
  add_metric(section, name + ".logical_processors", number_of_logical_processors);
}

cpus_type cpu_frequency_data::query_wmi() {
  wmi_impl::query wmi_q(helper::query, helper::ns, "", "");
  wmi_impl::row_enumerator row = wmi_q.execute();
  cpus_type cpus;
  while (row.has_next()) {
    const wmi_impl::row r = row.get_next();
    cpu_frequency c;
    c.read_wmi(r);
    cpus.push_back(c);
  }
  return cpus;
}

void cpu_frequency_data::fetch() {
  if (!fetch_cpu_frequency_) return;

  try {
    const cpus_type tmp = query_wmi();
    const boost::unique_lock<boost::shared_mutex> write_lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
    if (!write_lock.owns_lock()) throw nsclient::nsclient_exception("Failed to get mutex for writing CPU frequency data");
    cpus_ = tmp;
  } catch (const wmi_impl::wmi_exception &e) {
    if (e.get_code() == WBEM_E_INVALID_QUERY || e.get_code() == WBEM_E_NOT_FOUND) {
      fetch_cpu_frequency_ = false;
      throw nsclient::nsclient_exception("Failed to fetch CPU frequency metrics (WMI class not available), disabling...");
    }
    throw nsclient::nsclient_exception("Failed to fetch CPU frequency metrics: " + e.reason());
  }
}

cpus_type cpu_frequency_data::get() {
  const boost::shared_lock<boost::shared_mutex> read_lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!read_lock.owns_lock()) throw nsclient::nsclient_exception("Failed to get mutex for reading CPU frequency data");
  return cpus_;
}

namespace check {

typedef cpu_frequency filter_obj;

typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj>> native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter_type;

filter_obj_handler::filter_obj_handler() {
  registry_.add_string("name", &filter_obj::get_name, "CPU name / model string");

  registry_.add_int_x("current_mhz", &filter_obj::get_current_mhz, "Current clock speed in MHz")
      .add_int_perf("MHz")
      .add_int_x("max_mhz", &filter_obj::get_max_mhz, "Maximum clock speed in MHz")
      .add_int_perf("MHz")
      .add_int_x("frequency_pct", &filter_obj::get_frequency_pct, "Current frequency as percentage of maximum")
      .add_int_perf("%")
      .add_int_x("cores", &filter_obj::get_number_of_cores, "Number of physical cores")
      .add_int_x("logical_processors", &filter_obj::get_number_of_logical_processors, "Number of logical processors (threads)");
}

void check_cpu_frequency(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                         const cpus_type &data) {
  modern_filter::data_container mdata;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, mdata);

  filter_type filter;
  filter_helper.add_options("frequency_pct < 50", "frequency_pct < 30", "", filter.get_filter_syntax(), "warning");
  filter_helper.add_syntax("${status}: ${list}", "${name}: ${current_mhz}/${max_mhz} MHz (${frequency_pct}%)", "${name}", "",
                           "%(status): All CPU frequencies seem ok.");

  if (!filter_helper.parse_options()) return;

  if (!filter_helper.build_filter(filter)) return;
  for (const cpu_frequency &c : data) {
    const boost::shared_ptr<filter_obj> record(new filter_obj(c));
    filter.match(record);
  }
  filter_helper.post_process(filter);
}

}  // namespace check

}  // namespace cpu_frequency_check

