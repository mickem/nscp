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

#include "check_temperature.hpp"

#include <boost/thread/locks.hpp>
#include <nscapi/nscapi_metrics_helper.hpp>
#include <nsclient/nsclient_exception.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <parsers/where/node.hpp>
#include <str/xtos.hpp>

namespace temperature_check {

// Primary source: MSAcpi_ThermalZoneTemperature in root\WMI.
// Requires the ACPI thermal zone driver; not present on all machines.
// CurrentTemperature is in tenths of Kelvin.
std::string helper::acpi_query = "select InstanceName, Active, CurrentTemperature, ThrottleReasons from MSAcpi_ThermalZoneTemperature";
std::string helper::acpi_namespace = "root\\WMI";

// Fallback source: Win32_PerfFormattedData_Counters_ThermalZoneInformation in root\CIMV2.
// Available on Windows 10 / Server 2016 and later.
// Temperature is in Kelvin (integer, not tenths).
std::string helper::perf_query =
    "select Name, HighPrecisionTemperature, Temperature, ThrottleReasons from Win32_PerfFormattedData_Counters_ThermalZoneInformation";
std::string helper::perf_namespace = "root\\CIMV2";

std::string helper::parse_zone_name(const std::string &raw) {
  // Raw names come from ACPI namespace paths such as:
  //   "\_TZ.THM"   "\_TZ.THM0"   "\_TZ.TZ00"   "\_SB.TZ01"   "ACPI\ThermalZone\THM0_0"
  // We strip the path prefix to get the short identifier, then build a friendly label.

  std::string id = raw;

  // Strip leading backslash
  if (!id.empty() && id[0] == '\\') id = id.substr(1);

  // If the name contains a dot (ACPI path), take only the last segment.
  // "\_TZ.THM0" → "THM0",  "\_SB.PCI0.LPCB.EC0.TZ00" → "TZ00"
  std::string::size_type dot = id.rfind('.');
  if (dot != std::string::npos) {
    id = id.substr(dot + 1);
  }

  // If the name contains a backslash (WMI instance path like "ACPI\ThermalZone\THM0_0")
  // take the last segment.
  std::string::size_type bs = id.rfind('\\');
  if (bs != std::string::npos) {
    id = id.substr(bs + 1);
  }

  if (id.empty()) return raw;

  return "Thermal Zone (" + id + ")";
}

void thermal_zone::read_acpi(wmi_impl::row r) {
  name = helper::parse_zone_name(r.get_string("InstanceName"));
  active = r.get_int("Active") != 0;
  // CurrentTemperature is in tenths of Kelvin — convert to Celsius.
  long long raw = r.get_int("CurrentTemperature");
  temperature = (static_cast<double>(raw) / 10.0) - 273.15;
  throttle_reasons = r.get_int("ThrottleReasons");
}

void thermal_zone::read_perf(wmi_impl::row r) {
  name = helper::parse_zone_name(r.get_string("Name"));
  active = true;
  // HighPrecisionTemperature is in tenths of Kelvin, Temperature is in Kelvin.
  // Prefer HighPrecisionTemperature when available (non-zero).
  long long hi = r.get_int("HighPrecisionTemperature");
  if (hi > 0) {
    temperature = (static_cast<double>(hi) / 10.0) - 273.15;
  } else {
    long long raw = r.get_int("Temperature");
    temperature = static_cast<double>(raw) - 273.15;
  }
  throttle_reasons = r.get_int("ThrottleReasons");
}

void thermal_zone::build_metrics(PB::Metrics::MetricsBundle *section) const {
  using namespace nscapi::metrics;
  add_metric(section, name + ".temperature", static_cast<long long>(temperature));
  add_metric(section, name + ".active", get_active());
  add_metric(section, name + ".throttle_reasons", throttle_reasons);
}

void temperature_data::query_acpi(zones_type &zones) {
  wmi_impl::query wmi_q(helper::acpi_query, helper::acpi_namespace, "", "");
  wmi_impl::row_enumerator row = wmi_q.execute();
  while (row.has_next()) {
    wmi_impl::row r = row.get_next();
    thermal_zone zone;
    zone.read_acpi(r);
    zones.push_back(zone);
  }
}

void temperature_data::query_perf(zones_type &zones) {
  wmi_impl::query wmi_q(helper::perf_query, helper::perf_namespace, "", "");
  wmi_impl::row_enumerator row = wmi_q.execute();
  while (row.has_next()) {
    wmi_impl::row r = row.get_next();
    thermal_zone zone;
    zone.read_perf(r);
    zones.push_back(zone);
  }
}

void temperature_data::fetch() {
  if (!fetch_temperature_) return;

  zones_type tmp;
  if (!use_fallback_) {
    try {
      query_acpi(tmp);
    } catch (const wmi_impl::wmi_exception &e) {
      if (e.get_code() == WBEM_E_INVALID_QUERY || e.get_code() == WBEM_E_NOT_FOUND || e.get_code() == WBEM_E_ACCESS_DENIED) {
        // ACPI thermal zone not available — switch to performance counter fallback.
        use_fallback_ = true;
      } else {
        throw nsclient::nsclient_exception("Failed to fetch ACPI temperature metrics: " + e.reason());
      }
    }
  }
  if (use_fallback_) {
    try {
      query_perf(tmp);
    } catch (const wmi_impl::wmi_exception &e) {
      if (e.get_code() == WBEM_E_INVALID_QUERY || e.get_code() == WBEM_E_NOT_FOUND) {
        fetch_temperature_ = false;
        throw nsclient::nsclient_exception("Failed to fetch temperature metrics (neither ACPI nor performance counter source available), disabling...");
      }
      throw nsclient::nsclient_exception("Failed to fetch temperature metrics: " + e.reason());
    }
  }
  {
    boost::unique_lock<boost::shared_mutex> write_lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
    if (!write_lock.owns_lock()) throw nsclient::nsclient_exception("Failed to get mutex for writing temperature data");
    zones_ = tmp;
  }
}

zones_type temperature_data::get() {
  boost::shared_lock<boost::shared_mutex> read_lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!read_lock.owns_lock()) throw nsclient::nsclient_exception("Failed to get mutex for reading temperature data");
  return zones_;
}

namespace check {

typedef thermal_zone filter_obj;

typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj>> native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter_type;

filter_obj_handler::filter_obj_handler() {
  registry_.add_string("name", &filter_obj::get_name, "Thermal zone name").add_string("active", &filter_obj::get_active, "True if the thermal zone is active");

  registry_.add_int_x("temperature", &filter_obj::get_temperature_i, "Temperature in degrees Celsius")
      .add_int_perf("C")
      .add_int_x("throttle_reasons", &filter_obj::get_throttle_reasons, "Throttle reasons bitmask");
}

void check_temperature(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response, zones_type data) {
  modern_filter::data_container mdata;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, mdata);

  filter_type filter;
  filter_helper.add_options("temperature > 70", "temperature > 90", "", filter.get_filter_syntax(), "critical");
  filter_helper.add_syntax("${status}: ${list}", "${name}: ${temperature} C", "${name}", "", "%(status): All thermal zones seem ok.");

  if (!filter_helper.parse_options()) return;

  if (!filter_helper.build_filter(filter)) return;
  for (const thermal_zone &z : data) {
    boost::shared_ptr<filter_obj> record(new filter_obj(z));
    filter.match(record);
  }
  filter_helper.post_process(filter);
}

}  // namespace check

}  // namespace temperature_check
