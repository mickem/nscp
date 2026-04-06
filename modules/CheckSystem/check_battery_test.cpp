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

#include <gtest/gtest.h>

// ============================================================================
// battery_info tests
// ============================================================================

TEST(CheckBattery, battery_info_defaults) {
  battery_check::battery_info info;
  EXPECT_EQ(info.charge_percent, -1);
  EXPECT_EQ(info.power_source, "unknown");
  EXPECT_EQ(info.status, "unknown");
  EXPECT_EQ(info.time_remaining, -1);
  EXPECT_EQ(info.battery_present, false);
  EXPECT_EQ(info.health_percent, -1);
}

TEST(CheckBattery, battery_info_show_no_battery) {
  battery_check::battery_info info;
  info.name = "test";
  info.battery_present = false;
  EXPECT_EQ(info.show(), "test (no battery)");
}

TEST(CheckBattery, battery_info_show_with_battery) {
  battery_check::battery_info info;
  info.name = "test";
  info.battery_present = true;
  info.charge_percent = 75;
  info.power_source = "ac";
  info.status = "charging";
  EXPECT_EQ(info.show(), "test (75%, ac, charging)");
}

TEST(CheckBattery, battery_info_time_remaining_unknown) {
  battery_check::battery_info info;
  info.time_remaining = -1;
  EXPECT_EQ(info.get_time_remaining_s(), "unknown");
}

TEST(CheckBattery, battery_info_getters) {
  battery_check::battery_info info;
  info.name = "system";
  info.charge_percent = 80;
  info.power_source = "battery";
  info.status = "discharging";
  info.time_remaining = 3600;
  info.battery_present = true;
  info.health_percent = 95;
  info.charge_rate = 1000;
  info.discharge_rate = 500;
  info.design_capacity = 50000;
  info.full_capacity = 47500;
  info.remaining_capacity = 38000;

  EXPECT_EQ(info.get_name(), "system");
  EXPECT_EQ(info.get_charge_percent(), 80);
  EXPECT_EQ(info.get_power_source(), "battery");
  EXPECT_EQ(info.get_status(), "discharging");
  EXPECT_EQ(info.get_time_remaining(), 3600);
  EXPECT_EQ(info.get_battery_present(), "true");
  EXPECT_EQ(info.get_health_percent(), 95);
  EXPECT_EQ(info.get_charge_rate(), 1000);
  EXPECT_EQ(info.get_discharge_rate(), 500);
  EXPECT_EQ(info.get_design_capacity(), 50000);
  EXPECT_EQ(info.get_full_capacity(), 47500);
  EXPECT_EQ(info.get_remaining_capacity(), 38000);
}

// ============================================================================
// battery_data tests
// ============================================================================

TEST(CheckBattery, battery_data_fetch_and_get) {
  battery_check::battery_data data;

  // fetch() should not throw - it uses GetSystemPowerStatus which is always available
  EXPECT_NO_THROW(data.fetch());

  // get() should return a list (may be empty on desktop without battery)
  battery_check::batteries_type batteries;
  EXPECT_NO_THROW(batteries = data.get());

  // If we got any batteries, verify basic constraints
  for (const auto &b : batteries) {
    // charge_percent should be -1 (unknown) or 0-100
    EXPECT_TRUE(b.charge_percent == -1 || (b.charge_percent >= 0 && b.charge_percent <= 100));
    // power_source should be one of the known values
    EXPECT_TRUE(b.power_source == "ac" || b.power_source == "battery" || b.power_source == "unknown");
  }
}
