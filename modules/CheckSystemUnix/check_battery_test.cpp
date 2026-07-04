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

#include "check_battery.h"

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <fstream>
#include <string>

namespace {

// RAII scratch directory acting as a fake /sys/class/power_supply root so the
// sysfs parsing can be exercised hermetically.
class FakeSysfs {
 public:
  FakeSysfs() {
    base_ = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("nscp-battery-%%%%-%%%%");
    boost::filesystem::create_directories(base_);
  }
  ~FakeSysfs() {
    boost::system::error_code ec;
    boost::filesystem::remove_all(base_, ec);
  }
  FakeSysfs(const FakeSysfs &) = delete;
  FakeSysfs &operator=(const FakeSysfs &) = delete;

  std::string root() const { return base_.string(); }

  void add_attr(const std::string &supply, const std::string &attr, const std::string &value) const {
    const boost::filesystem::path dir = base_ / supply;
    boost::filesystem::create_directories(dir);
    std::ofstream ofs((dir / attr).string().c_str());
    ofs << value << "\n";
  }

 private:
  boost::filesystem::path base_;
};

battery_check::battery_info read_single(const FakeSysfs &sysfs) {
  battery_check::batteries_type batteries = battery_check::read_battery_from(sysfs.root());
  EXPECT_EQ(batteries.size(), 1u);
  if (batteries.empty()) return battery_check::battery_info();
  return batteries.front();
}

std::string join_lines(const PB::Commands::QueryResponseMessage::Response &r) {
  std::string out;
  for (int i = 0; i < r.lines_size(); ++i) {
    if (!out.empty()) out += "\n";
    out += r.lines(i).message();
  }
  return out;
}

PB::Common::ResultCode run_check(const FakeSysfs &sysfs, PB::Commands::QueryResponseMessage::Response &response) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_battery");
  battery_check::check_battery(request, &response, battery_check::read_battery_from(sysfs.root()));
  return response.result();
}

}  // namespace

// ============================================================================
// battery_info defaults
// ============================================================================

TEST(CheckBattery, battery_info_defaults) {
  battery_check::battery_info info;
  EXPECT_EQ(info.charge_percent, -1);
  EXPECT_EQ(info.power_source, "unknown");
  EXPECT_EQ(info.status, "unknown");
  EXPECT_EQ(info.battery_present, false);
  EXPECT_EQ(info.health_percent, -1);
  EXPECT_EQ(info.time_remaining, -1);
  EXPECT_EQ(info.charge_rate, 0);
  EXPECT_EQ(info.discharge_rate, 0);
  EXPECT_EQ(info.design_capacity, 0);
  EXPECT_EQ(info.full_capacity, 0);
  EXPECT_EQ(info.remaining_capacity, 0);
  EXPECT_EQ(info.get_time_remaining_s(), "unknown");
}

// ============================================================================
// sysfs parsing
// ============================================================================

TEST(CheckBattery, missing_root_yields_no_batteries) {
  battery_check::batteries_type batteries = battery_check::read_battery_from("/nonexistent/nscp-battery-test");
  EXPECT_TRUE(batteries.empty());
}

TEST(CheckBattery, capacity_attribute_is_preferred) {
  FakeSysfs sysfs;
  sysfs.add_attr("BAT0", "type", "Battery");
  sysfs.add_attr("BAT0", "status", "Discharging");
  sysfs.add_attr("BAT0", "capacity", "42");
  // A conflicting pair proves the driver-reported capacity wins.
  sysfs.add_attr("BAT0", "charge_now", "1000000");
  sysfs.add_attr("BAT0", "charge_full", "2000000");

  battery_check::battery_info info = read_single(sysfs);
  EXPECT_EQ(info.name, "BAT0");
  EXPECT_EQ(info.charge_percent, 42);
  EXPECT_EQ(info.status, "discharging");
  EXPECT_TRUE(info.battery_present);
}

TEST(CheckBattery, percent_derived_from_charge_now_when_capacity_absent) {
  // Regression: drivers exposing only charge_now/charge_full used to leave
  // the -1 sentinel in charge_percent, tripping CRITICAL on a full battery.
  FakeSysfs sysfs;
  sysfs.add_attr("BAT0", "type", "Battery");
  sysfs.add_attr("BAT0", "status", "Discharging");
  sysfs.add_attr("BAT0", "present", "1");
  sysfs.add_attr("BAT0", "charge_now", "2500000");
  sysfs.add_attr("BAT0", "charge_full", "5000000");

  battery_check::battery_info info = read_single(sysfs);
  EXPECT_EQ(info.charge_percent, 50);
  EXPECT_TRUE(info.battery_present);
}

TEST(CheckBattery, percent_derived_from_energy_when_charge_absent) {
  FakeSysfs sysfs;
  sysfs.add_attr("BAT0", "type", "Battery");
  sysfs.add_attr("BAT0", "status", "Discharging");
  sysfs.add_attr("BAT0", "energy_now", "37500000");
  sysfs.add_attr("BAT0", "energy_full", "50000000");

  battery_check::battery_info info = read_single(sysfs);
  EXPECT_EQ(info.charge_percent, 75);
  EXPECT_TRUE(info.battery_present);
}

TEST(CheckBattery, derived_percent_is_capped_at_100) {
  FakeSysfs sysfs;
  sysfs.add_attr("BAT0", "type", "Battery");
  sysfs.add_attr("BAT0", "charge_now", "5100000");
  sysfs.add_attr("BAT0", "charge_full", "5000000");

  battery_check::battery_info info = read_single(sysfs);
  EXPECT_EQ(info.charge_percent, 100);
}

TEST(CheckBattery, unknown_percent_reports_battery_not_present) {
  // No capacity, charge_* or energy_* attributes: the charge cannot be
  // determined so the battery must report not-present (mirroring Windows) to
  // stay out of the default charge thresholds.
  FakeSysfs sysfs;
  sysfs.add_attr("BAT0", "type", "Battery");
  sysfs.add_attr("BAT0", "status", "Discharging");
  sysfs.add_attr("BAT0", "present", "1");

  battery_check::battery_info info = read_single(sysfs);
  EXPECT_EQ(info.charge_percent, -1);
  EXPECT_FALSE(info.battery_present);
}

TEST(CheckBattery, present_zero_reports_battery_not_present) {
  FakeSysfs sysfs;
  sysfs.add_attr("BAT0", "type", "Battery");
  sysfs.add_attr("BAT0", "present", "0");
  sysfs.add_attr("BAT0", "capacity", "80");

  battery_check::battery_info info = read_single(sysfs);
  EXPECT_FALSE(info.battery_present);
}

TEST(CheckBattery, mains_online_sets_power_source_ac) {
  FakeSysfs sysfs;
  sysfs.add_attr("BAT0", "type", "Battery");
  sysfs.add_attr("BAT0", "capacity", "90");
  sysfs.add_attr("AC", "type", "Mains");
  sysfs.add_attr("AC", "online", "1");

  battery_check::battery_info info = read_single(sysfs);
  EXPECT_EQ(info.power_source, "ac");
}

// ============================================================================
// capacity / rate / time columns
// ============================================================================

TEST(CheckBattery, energy_attributes_reported_in_mwh) {
  FakeSysfs sysfs;
  sysfs.add_attr("BAT0", "type", "Battery");
  sysfs.add_attr("BAT0", "status", "Discharging");
  sysfs.add_attr("BAT0", "energy_now", "37500000");
  sysfs.add_attr("BAT0", "energy_full", "50000000");
  sysfs.add_attr("BAT0", "energy_full_design", "57000000");
  sysfs.add_attr("BAT0", "power_now", "12000000");

  battery_check::battery_info info = read_single(sysfs);
  EXPECT_EQ(info.remaining_capacity, 37500);
  EXPECT_EQ(info.full_capacity, 50000);
  EXPECT_EQ(info.design_capacity, 57000);
  EXPECT_EQ(info.health_percent, 87);  // 50000000 * 100 / 57000000
  EXPECT_EQ(info.discharge_rate, 12000);
  EXPECT_EQ(info.charge_rate, 0);
  // 37500000 uWh / 12000000 uW = 3.125 h = 11250 s
  EXPECT_EQ(info.time_remaining, 11250);
}

TEST(CheckBattery, charge_attributes_converted_via_design_voltage) {
  FakeSysfs sysfs;
  sysfs.add_attr("BAT0", "type", "Battery");
  sysfs.add_attr("BAT0", "status", "Charging");
  sysfs.add_attr("BAT0", "charge_now", "2500000");
  sysfs.add_attr("BAT0", "charge_full", "5000000");
  sysfs.add_attr("BAT0", "charge_full_design", "5500000");
  sysfs.add_attr("BAT0", "voltage_min_design", "11400000");
  sysfs.add_attr("BAT0", "current_now", "2000000");

  battery_check::battery_info info = read_single(sysfs);
  // 2500000 uAh * 11400000 uV / 1e9 = 28500 mWh
  EXPECT_EQ(info.remaining_capacity, 28500);
  EXPECT_EQ(info.full_capacity, 57000);
  EXPECT_EQ(info.design_capacity, 62700);
  EXPECT_EQ(info.health_percent, 90);  // 5000000 * 100 / 5500000
  // 2000000 uA * 11400000 uV / 1e9 = 22800 mW, charging
  EXPECT_EQ(info.charge_rate, 22800);
  EXPECT_EQ(info.discharge_rate, 0);
  // Charging: no discharge time estimate.
  EXPECT_EQ(info.time_remaining, -1);
}

TEST(CheckBattery, time_remaining_prefers_driver_estimate) {
  FakeSysfs sysfs;
  sysfs.add_attr("BAT0", "type", "Battery");
  sysfs.add_attr("BAT0", "status", "Discharging");
  sysfs.add_attr("BAT0", "capacity", "50");
  sysfs.add_attr("BAT0", "time_to_empty_now", "5400");
  sysfs.add_attr("BAT0", "energy_now", "37500000");
  sysfs.add_attr("BAT0", "power_now", "12000000");

  battery_check::battery_info info = read_single(sysfs);
  EXPECT_EQ(info.time_remaining, 5400);
}

TEST(CheckBattery, time_remaining_derived_from_charge_and_current) {
  FakeSysfs sysfs;
  sysfs.add_attr("BAT0", "type", "Battery");
  sysfs.add_attr("BAT0", "status", "Discharging");
  sysfs.add_attr("BAT0", "charge_now", "2500000");
  sysfs.add_attr("BAT0", "charge_full", "5000000");
  sysfs.add_attr("BAT0", "current_now", "1250000");

  battery_check::battery_info info = read_single(sysfs);
  // 2500000 uAh / 1250000 uA = 2 h = 7200 s
  EXPECT_EQ(info.time_remaining, 7200);
}

// ============================================================================
// check_battery end-to-end with default thresholds
// ============================================================================

TEST(CheckBattery, full_battery_without_capacity_attribute_is_ok) {
  // The reported bug: "BAT0: -1% (discharging)" CRITICAL on a full battery.
  FakeSysfs sysfs;
  sysfs.add_attr("BAT0", "type", "Battery");
  sysfs.add_attr("BAT0", "status", "Discharging");
  sysfs.add_attr("BAT0", "present", "1");
  sysfs.add_attr("BAT0", "charge_now", "5000000");
  sysfs.add_attr("BAT0", "charge_full", "5000000");

  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check(sysfs, response), PB::Common::ResultCode::OK) << join_lines(response);
  EXPECT_NE(join_lines(response).find("100%"), std::string::npos) << join_lines(response);
}

TEST(CheckBattery, unknown_percent_does_not_trip_thresholds) {
  FakeSysfs sysfs;
  sysfs.add_attr("BAT0", "type", "Battery");
  sysfs.add_attr("BAT0", "status", "Discharging");
  sysfs.add_attr("BAT0", "present", "1");

  // The charge thresholds must not fire against the -1 sentinel: the battery
  // is excluded by the default filter and the (Windows-parity) empty-state
  // "warning" applies instead of a false CRITICAL "-1%" detail line.
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check(sysfs, response), PB::Common::ResultCode::WARNING) << join_lines(response);
  EXPECT_EQ(join_lines(response).find("-1%"), std::string::npos) << join_lines(response);

  // The empty-state is user-configurable, so the standard escape hatch works.
  PB::Commands::QueryRequestMessage::Request request;
  PB::Commands::QueryResponseMessage::Response response_ok;
  request.set_command("check_battery");
  request.add_arguments("empty-state=ok");
  battery_check::check_battery(request, &response_ok, battery_check::read_battery_from(sysfs.root()));
  EXPECT_EQ(response_ok.result(), PB::Common::ResultCode::OK) << join_lines(response_ok);
}

TEST(CheckBattery, no_battery_flows_through_empty_state) {
  // Zero batteries must reach the filter's empty-state (default "warning",
  // matching Windows) instead of a hard-coded OK, so fleet configs like
  // `empty-state=critical` detect a removed/dead battery pack on Linux too.
  FakeSysfs sysfs;
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check(sysfs, response), PB::Common::ResultCode::WARNING) << join_lines(response);
  EXPECT_NE(join_lines(response).find("No battery found"), std::string::npos) << join_lines(response);

  for (const auto &state : {std::make_pair(std::string("ok"), PB::Common::ResultCode::OK), std::make_pair(std::string("critical"), PB::Common::ResultCode::CRITICAL)}) {
    PB::Commands::QueryRequestMessage::Request request;
    PB::Commands::QueryResponseMessage::Response configured;
    request.set_command("check_battery");
    request.add_arguments("empty-state=" + state.first);
    battery_check::check_battery(request, &configured, battery_check::read_battery_from(sysfs.root()));
    EXPECT_EQ(configured.result(), state.second) << state.first << ": " << join_lines(configured);
  }
}

TEST(CheckBattery, default_warning_threshold_is_20_percent) {
  FakeSysfs sysfs;
  sysfs.add_attr("BAT0", "type", "Battery");
  sysfs.add_attr("BAT0", "status", "Discharging");
  sysfs.add_attr("BAT0", "capacity", "15");

  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check(sysfs, response), PB::Common::ResultCode::WARNING) << join_lines(response);
}

TEST(CheckBattery, default_critical_threshold_is_10_percent) {
  FakeSysfs sysfs;
  sysfs.add_attr("BAT0", "type", "Battery");
  sysfs.add_attr("BAT0", "status", "Discharging");
  sysfs.add_attr("BAT0", "capacity", "5");

  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check(sysfs, response), PB::Common::ResultCode::CRITICAL) << join_lines(response);
}

TEST(CheckBattery, battery_present_keyword_and_present_alias_both_parse) {
  FakeSysfs sysfs;
  sysfs.add_attr("BAT0", "type", "Battery");
  sysfs.add_attr("BAT0", "status", "Charging");
  sysfs.add_attr("BAT0", "capacity", "80");

  for (const std::string &keyword : {std::string("battery_present"), std::string("present")}) {
    PB::Commands::QueryRequestMessage::Request request;
    PB::Commands::QueryResponseMessage::Response response;
    request.set_command("check_battery");
    request.add_arguments("filter=" + keyword + " = 'true'");
    battery_check::check_battery(request, &response, battery_check::read_battery_from(sysfs.root()));
    EXPECT_EQ(response.result(), PB::Common::ResultCode::OK) << keyword << ": " << join_lines(response);
    EXPECT_NE(join_lines(response).find("80%"), std::string::npos) << keyword << ": " << join_lines(response);
  }
}
