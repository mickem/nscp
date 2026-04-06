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

#include "check_battery.hpp"

#include <Windows.h>

#include <boost/thread/locks.hpp>
#include <nscapi/nscapi_metrics_helper.hpp>
#include <nsclient/nsclient_exception.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <parsers/where/node.hpp>
#include <str/format.hpp>
#include <str/xtos.hpp>
#include <win/wmi/wmi_query.hpp>

namespace battery_check {

std::string battery_info::get_time_remaining_s() const {
  if (time_remaining < 0) return "unknown";
  return str::format::itos_as_time(time_remaining * 1000);
}

std::string battery_info::show() const {
  if (!battery_present) return name + " (no battery)";
  return name + " (" + str::xtos(charge_percent) + "%, " + power_source + ", " + status + ")";
}

void battery_info::build_metrics(PB::Metrics::MetricsBundle *section) const {
  using namespace nscapi::metrics;
  const std::string prefix = name.empty() ? "" : name + ".";
  add_metric(section, prefix + "charge_percent", charge_percent);
  add_metric(section, prefix + "power_source", power_source);
  add_metric(section, prefix + "status", status);
  add_metric(section, prefix + "battery_present", get_battery_present());
  if (time_remaining >= 0) add_metric(section, prefix + "time_remaining", time_remaining);
  if (health_percent >= 0) add_metric(section, prefix + "health_percent", health_percent);
  if (charge_rate > 0) add_metric(section, prefix + "charge_rate", charge_rate);
  if (discharge_rate > 0) add_metric(section, prefix + "discharge_rate", discharge_rate);
  if (design_capacity > 0) add_metric(section, prefix + "design_capacity", design_capacity);
  if (full_capacity > 0) add_metric(section, prefix + "full_capacity", full_capacity);
  if (remaining_capacity > 0) add_metric(section, prefix + "remaining_capacity", remaining_capacity);
}

void battery_data::query_power_status(batteries_type &batteries) {
  SYSTEM_POWER_STATUS sps;
  if (!GetSystemPowerStatus(&sps)) {
    throw nsclient::nsclient_exception("Failed to get system power status: " + str::xtos(GetLastError()));
  }

  battery_info info;
  info.name = "system";

  // ACLineStatus: 0 = offline (battery), 1 = online (AC), 255 = unknown
  switch (sps.ACLineStatus) {
    case 0:
      info.power_source = "battery";
      break;
    case 1:
      info.power_source = "ac";
      break;
    default:
      info.power_source = "unknown";
      break;
  }

  // BatteryFlag bit flags:
  // 1 = High (>66%), 2 = Low (<33%), 4 = Critical (<5%), 8 = Charging, 128 = No battery, 255 = Unknown
  if (sps.BatteryFlag == 255) {
    info.status = "unknown";
    info.battery_present = false;
  } else if (sps.BatteryFlag & 128) {
    info.status = "no_battery";
    info.battery_present = false;
  } else if (sps.BatteryFlag & 8) {
    info.status = "charging";
    info.battery_present = true;
  } else if (sps.BatteryFlag & 4) {
    info.status = "critical";
    info.battery_present = true;
  } else if (sps.BatteryFlag & 2) {
    info.status = "low";
    info.battery_present = true;
  } else if (sps.BatteryFlag & 1) {
    info.status = "high";
    info.battery_present = true;
  } else {
    // No flags set but battery present
    info.status = "discharging";
    info.battery_present = true;
  }

  // BatteryLifePercent: 0-100, or 255 if unknown
  if (sps.BatteryLifePercent != 255) {
    info.charge_percent = sps.BatteryLifePercent;
  }

  // BatteryLifeTime: Seconds remaining, or -1 (0xFFFFFFFF) if unknown/charging/AC
  if (sps.BatteryLifeTime != static_cast<DWORD>(-1)) {
    info.time_remaining = sps.BatteryLifeTime;
  }

  // BatteryFullLifeTime: Seconds of full battery life, or -1 if unknown
  if (sps.BatteryFullLifeTime != static_cast<DWORD>(-1)) {
    info.full_charge_time = sps.BatteryFullLifeTime;
  }

  batteries.push_back(info);
}

void battery_data::query_wmi_battery(batteries_type &batteries) {
  if (batteries.empty()) return;

  // Try to get additional battery info from WMI BatteryStatus and BatteryStaticData
  // These classes are in root\WMI namespace and require the battery driver

  try {
    // BatteryStatus provides dynamic battery information
    wmi_impl::query wmi_status("select Tag, ChargeRate, DischargeRate, RemainingCapacity from BatteryStatus where Active = True", "root\\WMI", "", "");
    wmi_impl::row_enumerator row = wmi_status.execute();
    while (row.has_next()) {
      wmi_impl::row r = row.get_next();
      // Apply to the first battery (we only track one from GetSystemPowerStatus)
      if (!batteries.empty()) {
        battery_info &info = batteries.front();
        info.charge_rate = r.get_int("ChargeRate");
        info.discharge_rate = r.get_int("DischargeRate");
        info.remaining_capacity = r.get_int("RemainingCapacity");
      }
      break;  // Only need first battery
    }
  } catch (const wmi_impl::wmi_exception &) {
    // WMI battery classes may not be available on all systems; ignore errors
  }

  try {
    // BatteryStaticData provides design capacity information for health calculation
    wmi_impl::query wmi_static("select Tag, DesignedCapacity from BatteryStaticData", "root\\WMI", "", "");
    wmi_impl::row_enumerator row = wmi_static.execute();
    while (row.has_next()) {
      wmi_impl::row r = row.get_next();
      if (!batteries.empty()) {
        battery_info &info = batteries.front();
        info.design_capacity = r.get_int("DesignedCapacity");
      }
      break;
    }
  } catch (const wmi_impl::wmi_exception &) {
    // Ignore WMI errors
  }

  try {
    // BatteryFullChargedCapacity provides current full charge capacity
    wmi_impl::query wmi_full("select Tag, FullChargedCapacity from BatteryFullChargedCapacity", "root\\WMI", "", "");
    wmi_impl::row_enumerator row = wmi_full.execute();
    while (row.has_next()) {
      wmi_impl::row r = row.get_next();
      if (!batteries.empty()) {
        battery_info &info = batteries.front();
        info.full_capacity = r.get_int("FullChargedCapacity");

        // Calculate health percentage if we have both design and full capacity
        if (info.design_capacity > 0 && info.full_capacity > 0) {
          info.health_percent = (info.full_capacity * 100) / info.design_capacity;
          // Cap at 100% (some batteries report higher full capacity than design)
          if (info.health_percent > 100) info.health_percent = 100;
        }
      }
      break;
    }
  } catch (const wmi_impl::wmi_exception &) {
    // Ignore WMI errors
  }
}

void battery_data::fetch() {
  if (!fetch_battery_) return;

  batteries_type tmp;
  try {
    query_power_status(tmp);
    query_wmi_battery(tmp);
  } catch (const std::exception &e) {
    fetch_battery_ = false;
    throw nsclient::nsclient_exception("Failed to fetch battery status, disabling: " + std::string(e.what()));
  }

  {
    boost::unique_lock<boost::shared_mutex> write_lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
    if (!write_lock.owns_lock()) throw nsclient::nsclient_exception("Failed to get mutex for writing battery data");
    batteries_ = tmp;
  }
}

batteries_type battery_data::get() {
  boost::shared_lock<boost::shared_mutex> read_lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!read_lock.owns_lock()) throw nsclient::nsclient_exception("Failed to get mutex for reading battery data");
  return batteries_;
}

namespace check {

typedef battery_info filter_obj;

typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj>> native_context;
struct filter_obj_handler final : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter_type;

filter_obj_handler::filter_obj_handler() {
  // clang-format off
  registry_.add_string("name", &filter_obj::get_name, "Battery name/identifier")
      .add_string("power_source", &filter_obj::get_power_source, "Power source: 'ac', 'battery', or 'unknown'")
      .add_string("status", &filter_obj::get_status, "Battery status: 'charging', 'discharging', 'high', 'low', 'critical', 'no_battery', or 'unknown'")
      .add_string("battery_present", &filter_obj::get_battery_present, "Whether a battery is present: 'true' or 'false'");

  registry_.add_int_x("charge", &filter_obj::get_charge_percent, "Battery charge level in percent (0-100)")
      .add_int_perf("%")
      .add_int_x("health", &filter_obj::get_health_percent, "Battery health in percent (full_capacity / design_capacity * 100)")
      .add_int_perf("%")
      .add_int_x("time_remaining", &filter_obj::get_time_remaining, "Estimated time remaining in seconds (-1 if unknown or on AC)")
      .add_int_x("charge_rate", &filter_obj::get_charge_rate, "Current charge rate in mW (when charging)")
      .add_int_x("discharge_rate", &filter_obj::get_discharge_rate, "Current discharge rate in mW (when discharging)")
      .add_int_x("design_capacity", &filter_obj::get_design_capacity, "Design capacity in mWh")
      .add_int_x("full_capacity", &filter_obj::get_full_capacity, "Current full charge capacity in mWh")
      .add_int_x("remaining_capacity", &filter_obj::get_remaining_capacity, "Current remaining capacity in mWh");
  // clang-format on

  registry_.add_human_string("time_remaining", &filter_obj::get_time_remaining_s, "Estimated time remaining (human readable)");
}

void check_battery(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response, batteries_type data) {
  modern_filter::data_container mdata;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, mdata);

  filter_type filter;
  filter_helper.add_options("charge < 20", "charge < 10", "battery_present = 'true'", filter.get_filter_syntax(), "warning");
  filter_helper.add_syntax("${status}: ${list}", "${name}: ${charge}% (${power_source}, ${status})", "${name}", "",
                           "%(status): No battery found or all batteries ok.");

  if (!filter_helper.parse_options()) return;

  if (!filter_helper.build_filter(filter)) return;

  for (const battery_info &b : data) {
    boost::shared_ptr<filter_obj> record(new filter_obj(b));
    filter.match(record);
  }

  filter_helper.post_process(filter);
}

}  // namespace check

}  // namespace battery_check
