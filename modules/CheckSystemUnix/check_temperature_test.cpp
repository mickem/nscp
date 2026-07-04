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

#include "check_temperature.h"

#include <boost/filesystem.hpp>
#include <fstream>
#include <gtest/gtest.h>
#include <map>
#include <string>

namespace fs = boost::filesystem;

namespace {

// Builds a fake sysfs tree (thermal/ and hwmon/ roots) in a temp directory so
// read_temperature_from() can be exercised without real hardware.
class CheckTemperatureTest : public ::testing::Test {
 protected:
  fs::path root;
  fs::path thermal;
  fs::path hwmon;

  void SetUp() override {
    root = fs::temp_directory_path() / fs::unique_path("nscp_temp_test_%%%%-%%%%");
    thermal = root / "thermal";
    hwmon = root / "hwmon";
    fs::create_directories(thermal);
    fs::create_directories(hwmon);
  }

  void TearDown() override {
    boost::system::error_code ec;
    fs::remove_all(root, ec);
  }

  static void write_file(const fs::path &p, const std::string &content) {
    fs::create_directories(p.parent_path());
    std::ofstream ofs(p.string().c_str());
    ofs << content << "\n";
  }

  // Adds hwmon/<dev>/ with a chip `name` and one temp1_input (+ optional label).
  void add_hwmon_sensor(const std::string &dev, const std::string &chip_name, long long millidegrees, const std::string &label = "") {
    write_file(hwmon / dev / "name", chip_name);
    write_file(hwmon / dev / "temp1_input", std::to_string(millidegrees));
    if (!label.empty()) write_file(hwmon / dev / "temp1_label", label);
  }

  temperature_check::zones_type read() { return temperature_check::read_temperature_from(thermal.string(), hwmon.string()); }

  static std::map<std::string, double> as_map(const temperature_check::zones_type &zones) {
    std::map<std::string, double> result;
    for (const temperature_check::thermal_zone &z : zones) result[z.name] = z.temperature;
    return result;
  }
};

}  // namespace

TEST_F(CheckTemperatureTest, empty_tree_yields_no_zones) { EXPECT_TRUE(read().empty()); }

TEST_F(CheckTemperatureTest, single_chip_names_unchanged) {
  add_hwmon_sensor("hwmon0", "coretemp", 45000, "Core 0");
  write_file(hwmon / "hwmon0" / "temp2_input", "47000");
  write_file(hwmon / "hwmon0" / "temp2_label", "Core 1");

  std::map<std::string, double> zones = as_map(read());
  ASSERT_EQ(zones.size(), 2u);
  EXPECT_DOUBLE_EQ(zones["coretemp_Core_0"], 45.0);
  EXPECT_DOUBLE_EQ(zones["coretemp_Core_1"], 47.0);
}

TEST_F(CheckTemperatureTest, unlabeled_sensor_uses_temp_number) {
  add_hwmon_sensor("hwmon0", "nvme", 38000);

  std::map<std::string, double> zones = as_map(read());
  ASSERT_EQ(zones.size(), 1u);
  EXPECT_DOUBLE_EQ(zones["nvme_temp1"], 38.0);
}

TEST_F(CheckTemperatureTest, duplicate_chip_names_keep_both_sensors) {
  // Dual-socket server: two hwmon devices both named "coretemp". The second
  // chip's sensors must not be dropped; they get a "-1" chip suffix while the
  // first chip keeps its plain name.
  add_hwmon_sensor("hwmon0", "coretemp", 45000, "Core 0");
  add_hwmon_sensor("hwmon1", "coretemp", 95000, "Core 0");

  std::map<std::string, double> zones = as_map(read());
  ASSERT_EQ(zones.size(), 2u);
  EXPECT_DOUBLE_EQ(zones["coretemp_Core_0"], 45.0);
  EXPECT_DOUBLE_EQ(zones["coretemp-1_Core_0"], 95.0);
}

TEST_F(CheckTemperatureTest, three_identical_chips_all_kept) {
  add_hwmon_sensor("hwmon0", "nvme", 30000, "Composite");
  add_hwmon_sensor("hwmon1", "nvme", 31000, "Composite");
  add_hwmon_sensor("hwmon2", "nvme", 32000, "Composite");

  std::map<std::string, double> zones = as_map(read());
  ASSERT_EQ(zones.size(), 3u);
  EXPECT_DOUBLE_EQ(zones["nvme_Composite"], 30.0);
  EXPECT_DOUBLE_EQ(zones["nvme-1_Composite"], 31.0);
  EXPECT_DOUBLE_EQ(zones["nvme-2_Composite"], 32.0);
}

TEST_F(CheckTemperatureTest, duplicate_thermal_zone_types_keep_both_zones) {
  // Two zones of the same type (dual "acpitz" on laptops/ARM SoCs) must not
  // produce two identical names: one row/metric would mask the other.
  write_file(thermal / "thermal_zone0" / "temp", "42000");
  write_file(thermal / "thermal_zone0" / "type", "acpitz");
  write_file(thermal / "thermal_zone1" / "temp", "95000");
  write_file(thermal / "thermal_zone1" / "type", "acpitz");

  std::map<std::string, double> zones = as_map(read());
  ASSERT_EQ(zones.size(), 2u);
  EXPECT_DOUBLE_EQ(zones["acpitz"], 42.0);
  EXPECT_DOUBLE_EQ(zones["acpitz-1"], 95.0);
}

TEST_F(CheckTemperatureTest, thermal_zone_and_hwmon_are_both_read) {
  write_file(thermal / "thermal_zone0" / "temp", "52000");
  write_file(thermal / "thermal_zone0" / "type", "x86_pkg_temp");
  add_hwmon_sensor("hwmon0", "acpitz", 51000);

  std::map<std::string, double> zones = as_map(read());
  ASSERT_EQ(zones.size(), 2u);
  EXPECT_DOUBLE_EQ(zones["x86_pkg_temp"], 52.0);
  EXPECT_DOUBLE_EQ(zones["acpitz_temp1"], 51.0);
}

TEST_F(CheckTemperatureTest, hwmon_name_clash_with_thermal_zone_is_suffixed_not_dropped) {
  // If a hwmon-derived name happens to collide with a thermal zone name the
  // sensor is kept under a suffixed name rather than silently discarded.
  write_file(thermal / "thermal_zone0" / "temp", "40000");
  write_file(thermal / "thermal_zone0" / "type", "acpitz_temp1");
  add_hwmon_sensor("hwmon0", "acpitz", 41000);

  std::map<std::string, double> zones = as_map(read());
  ASSERT_EQ(zones.size(), 2u);
  EXPECT_DOUBLE_EQ(zones["acpitz_temp1"], 40.0);
  EXPECT_DOUBLE_EQ(zones["acpitz_temp1-1"], 41.0);
}
