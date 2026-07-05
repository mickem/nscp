// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <list>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/metrics.hpp>
#include <string>

namespace battery_check {

/**
 * Represents battery / power status information obtained from the Linux sysfs
 * power_supply class (/sys/class/power_supply/).
 *
 * Each "Battery" type entry yields one battery_info. AC adapter ("Mains")
 * entries are folded into the power_source field of every battery.
 *
 * Units mirror the Windows implementation (mW / mWh / seconds). Drivers that
 * only expose charge_* (uAh) and current_now (uA) are converted to energy
 * units using the design voltage (falling back to the current voltage).
 */
struct battery_info {
  std::string name;              // e.g. "BAT0"
  long long charge_percent;      // 0-100, -1 if unknown
  std::string power_source;      // "ac", "battery", "unknown"
  std::string status;            // "charging", "discharging", "full", "not_charging", "unknown"
  bool battery_present;          // True if a battery is detected
  long long health_percent;      // full_capacity / design_capacity * 100, -1 if unknown
  long long time_remaining;      // Seconds remaining, -1 if unknown or on AC
  long long charge_rate;         // mW charge rate (when charging), 0 if unknown
  long long discharge_rate;      // mW discharge rate (when discharging), 0 if unknown
  long long design_capacity;     // Design capacity in mWh, 0 if unknown
  long long full_capacity;       // Full charge capacity in mWh, 0 if unknown
  long long remaining_capacity;  // Current remaining capacity in mWh, 0 if unknown

  battery_info()
      : charge_percent(-1),
        power_source("unknown"),
        status("unknown"),
        battery_present(false),
        health_percent(-1),
        time_remaining(-1),
        charge_rate(0),
        discharge_rate(0),
        design_capacity(0),
        full_capacity(0),
        remaining_capacity(0) {}

  battery_info(const battery_info &other) = default;
  battery_info &operator=(const battery_info &other) = default;

  void build_metrics(PB::Metrics::MetricsBundle *section) const;

  std::string get_name() const { return name; }
  long long get_charge_percent() const { return charge_percent; }
  std::string get_power_source() const { return power_source; }
  std::string get_status() const { return status; }
  std::string get_battery_present() const { return battery_present ? "true" : "false"; }
  long long get_health_percent() const { return health_percent; }
  long long get_time_remaining() const { return time_remaining; }
  long long get_charge_rate() const { return charge_rate; }
  long long get_discharge_rate() const { return discharge_rate; }
  long long get_design_capacity() const { return design_capacity; }
  long long get_full_capacity() const { return full_capacity; }
  long long get_remaining_capacity() const { return remaining_capacity; }
  std::string get_time_remaining_s() const;
  std::string show() const;
};

typedef std::list<battery_info> batteries_type;

batteries_type read_battery();
// Read batteries from an alternative power_supply class root (for tests).
batteries_type read_battery_from(const std::string &power_supply_path);

void check_battery(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
// Filter the given batteries instead of reading sysfs (for tests).
void check_battery(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                   const batteries_type &batteries);

void build_battery_metrics(PB::Metrics::MetricsBundle *parent);

}  // namespace battery_check
