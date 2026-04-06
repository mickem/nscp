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

namespace battery_check {

/**
 * Represents battery status information obtained from Windows APIs.
 *
 * Primary source: GetSystemPowerStatus (always available on Windows)
 * - ACLineStatus: 0 = offline (battery), 1 = online (AC), 255 = unknown
 * - BatteryFlag: Bit flags for battery status
 * - BatteryLifePercent: 0-100, or 255 if unknown
 * - BatteryLifeTime: Seconds remaining, or -1 if unknown/AC
 * - BatteryFullLifeTime: Seconds for full charge, or -1 if unknown
 *
 * Secondary source: WMI BatteryStatus class (root\WMI)
 * - Provides additional health and charge/discharge rate information
 */
struct battery_info {
  std::string name;
  long long charge_percent;      // 0-100, -1 if unknown
  std::string power_source;      // "ac", "battery", "unknown"
  std::string status;            // "charging", "discharging", "full", "no_battery", "unknown"
  long long time_remaining;      // Seconds remaining, -1 if unknown or on AC
  long long full_charge_time;    // Seconds for full battery life, -1 if unknown
  bool battery_present;          // True if a battery is detected
  long long charge_rate;         // mW charge rate (positive = charging), 0 if unknown
  long long discharge_rate;      // mW discharge rate (positive = discharging), 0 if unknown
  long long design_capacity;     // Design capacity in mWh, 0 if unknown
  long long full_capacity;       // Full charge capacity in mWh, 0 if unknown
  long long remaining_capacity;  // Current remaining capacity in mWh, 0 if unknown
  long long health_percent;      // Battery health (full_capacity / design_capacity * 100), -1 if unknown

  battery_info()
      : charge_percent(-1),
        power_source("unknown"),
        status("unknown"),
        time_remaining(-1),
        full_charge_time(-1),
        battery_present(false),
        charge_rate(0),
        discharge_rate(0),
        design_capacity(0),
        full_capacity(0),
        remaining_capacity(0),
        health_percent(-1) {}

  battery_info(const battery_info &other) = default;
  battery_info &operator=(const battery_info &other) = default;

  void build_metrics(PB::Metrics::MetricsBundle *section) const;

  std::string get_name() const { return name; }
  long long get_charge_percent() const { return charge_percent; }
  std::string get_power_source() const { return power_source; }
  std::string get_status() const { return status; }
  long long get_time_remaining() const { return time_remaining; }
  long long get_full_charge_time() const { return full_charge_time; }
  std::string get_battery_present() const { return battery_present ? "true" : "false"; }
  long long get_charge_rate() const { return charge_rate; }
  long long get_discharge_rate() const { return discharge_rate; }
  long long get_design_capacity() const { return design_capacity; }
  long long get_full_capacity() const { return full_capacity; }
  long long get_remaining_capacity() const { return remaining_capacity; }
  long long get_health_percent() const { return health_percent; }

  std::string get_time_remaining_s() const;
  std::string show() const;
};

typedef std::list<battery_info> batteries_type;

class battery_data final {
  boost::shared_mutex mutex_;
  bool fetch_battery_;
  batteries_type batteries_;

 public:
  battery_data() : fetch_battery_(true) {}

  void fetch();
  batteries_type get();

 private:
  static void query_power_status(batteries_type &batteries);
  static void query_wmi_battery(batteries_type &batteries);
};

namespace check {
void check_battery(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response, batteries_type data);
}  // namespace check

}  // namespace battery_check
