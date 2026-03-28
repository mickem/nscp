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

#pragma once

#include <boost/thread/shared_mutex.hpp>
#include <list>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/metrics.hpp>
#include <string>
#include <win/wmi/wmi_query.hpp>

namespace temperature_check {

struct helper {
  // Primary: MSAcpi_ThermalZoneTemperature in root\WMI (requires ACPI driver).
  static std::string acpi_query;
  static std::string acpi_namespace;

  // Fallback: Win32_PerfFormattedData_Counters_ThermalZoneInformation in root\CIMV2 (Windows 10+).
  static std::string perf_query;
  static std::string perf_namespace;

  // Convert raw ACPI / WMI zone names (e.g. "\_TZ.THM") into friendlier labels.
  static std::string parse_zone_name(const std::string &raw);
};

struct thermal_zone {
  std::string name;
  double temperature;
  long long throttle_reasons;
  bool active;

  thermal_zone() : temperature(0.0), throttle_reasons(0), active(false) {}
  thermal_zone(const thermal_zone &other) = default;
  thermal_zone &operator=(const thermal_zone &other) = default;

  // Read from MSAcpi_ThermalZoneTemperature (tenths of Kelvin).
  void read_acpi(wmi_impl::row r);
  // Read from Win32_PerfFormattedData_Counters_ThermalZoneInformation (Kelvin).
  void read_perf(wmi_impl::row r);

  void build_metrics(PB::Metrics::MetricsBundle *section) const;

  std::string get_name() const { return name; }
  double get_temperature() const { return temperature; }
  long long get_temperature_i() const { return static_cast<long long>(temperature); }
  long long get_throttle_reasons() const { return throttle_reasons; }
  std::string get_active() const { return active ? "true" : "false"; }

  std::string show() const { return name + " (" + std::to_string(static_cast<long long>(temperature)) + " C)"; }
};

typedef std::list<thermal_zone> zones_type;

class temperature_data {
  boost::shared_mutex mutex_;
  bool fetch_temperature_;
  bool use_fallback_;
  zones_type zones_;

 public:
  temperature_data() : fetch_temperature_(true), use_fallback_(false) {}

  void fetch();
  zones_type get();

 private:
  void query_acpi(zones_type &zones);
  void query_perf(zones_type &zones);
};

namespace check {
void check_temperature(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response, zones_type data);
}  // namespace check

}  // namespace temperature_check
