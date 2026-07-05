// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <list>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/metrics.hpp>
#include <string>

namespace temperature_check {

struct thermal_zone {
  std::string name;    // friendly label, e.g. "x86_pkg_temp", "coretemp_Core_0", or thermal zone type
  double temperature;  // degrees Celsius
  bool active;

  thermal_zone() : temperature(0.0), active(false) {}
  thermal_zone(const thermal_zone &other) = default;
  thermal_zone &operator=(const thermal_zone &other) = default;

  std::string get_name() const { return name; }
  double get_temperature() const { return temperature; }
  long long get_temperature_i() const { return static_cast<long long>(temperature); }
  std::string get_active() const { return active ? "true" : "false"; }

  std::string show() const { return name + " (" + std::to_string(static_cast<long long>(temperature)) + " C)"; }
};

typedef std::list<thermal_zone> zones_type;

// Reads temperatures from /sys/class/thermal and /sys/class/hwmon. May return an empty list.
zones_type read_temperature();

// Testable variant of read_temperature() reading from explicit sysfs-style roots.
zones_type read_temperature_from(const std::string &thermal_base, const std::string &hwmon_base);

void check_temperature(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

void build_temperature_metrics(PB::Metrics::MetricsBundle *parent);

}  // namespace temperature_check
