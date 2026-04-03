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

#include <gtest/gtest.h>

#ifdef WIN32
#include <objbase.h>
#endif

#include "check_temperature.hpp"

// ============================================================================
// helper::parse_zone_name tests
// ============================================================================

TEST(TemperatureHelper, ParseZoneNameAcpiPath) {
  // "\_TZ.THM0" -> strip leading backslash, take after last dot
  EXPECT_EQ(temperature_check::helper::parse_zone_name("\\_TZ.THM0"), "Thermal Zone (THM0)");
}

TEST(TemperatureHelper, ParseZoneNameDeepAcpiPath) { EXPECT_EQ(temperature_check::helper::parse_zone_name("\\_SB.PCI0.LPCB.EC0.TZ00"), "Thermal Zone (TZ00)"); }

TEST(TemperatureHelper, ParseZoneNameWmiInstancePath) {
  // "ACPI\ThermalZone\THM0_0" -> take after last backslash
  EXPECT_EQ(temperature_check::helper::parse_zone_name("ACPI\\ThermalZone\\THM0_0"), "Thermal Zone (THM0_0)");
}

TEST(TemperatureHelper, ParseZoneNameSimple) {
  // No dots or backslashes -> wrap as-is
  EXPECT_EQ(temperature_check::helper::parse_zone_name("THM0"), "Thermal Zone (THM0)");
}

TEST(TemperatureHelper, ParseZoneNameEmpty) {
  // Empty string returns the raw input
  EXPECT_EQ(temperature_check::helper::parse_zone_name(""), "");
}

TEST(TemperatureHelper, ParseZoneNameLeadingBackslashOnly) {
  // "\\" -> strip leading backslash, empty id after that -> return raw
  EXPECT_EQ(temperature_check::helper::parse_zone_name("\\"), "\\");
}

// ============================================================================
// thermal_zone struct tests
// ============================================================================

TEST(ThermalZone, DefaultConstruction) {
  temperature_check::thermal_zone z;
  EXPECT_EQ(z.name, "");
  EXPECT_DOUBLE_EQ(z.temperature, 0.0);
  EXPECT_EQ(z.throttle_reasons, 0);
  EXPECT_FALSE(z.active);
}

TEST(ThermalZone, Accessors) {
  temperature_check::thermal_zone z;
  z.name = "Thermal Zone (THM0)";
  z.temperature = 45.5;
  z.throttle_reasons = 3;
  z.active = true;

  EXPECT_EQ(z.get_name(), "Thermal Zone (THM0)");
  EXPECT_DOUBLE_EQ(z.get_temperature(), 45.5);
  EXPECT_EQ(z.get_temperature_i(), 45);
  EXPECT_EQ(z.get_throttle_reasons(), 3);
  EXPECT_EQ(z.get_active(), "true");
}

TEST(ThermalZone, ActiveFalse) {
  temperature_check::thermal_zone z;
  z.active = false;
  EXPECT_EQ(z.get_active(), "false");
}

TEST(ThermalZone, TemperatureIntTruncates) {
  temperature_check::thermal_zone z;
  z.temperature = 99.9;
  EXPECT_EQ(z.get_temperature_i(), 99);
}

TEST(ThermalZone, NegativeTemperature) {
  temperature_check::thermal_zone z;
  z.temperature = -10.5;
  EXPECT_DOUBLE_EQ(z.get_temperature(), -10.5);
  EXPECT_EQ(z.get_temperature_i(), -10);
}

TEST(ThermalZone, ShowFormat) {
  temperature_check::thermal_zone z;
  z.name = "Thermal Zone (THM0)";
  z.temperature = 42.7;
  EXPECT_EQ(z.show(), "Thermal Zone (THM0) (42 C)");
}

TEST(ThermalZone, CopyConstruction) {
  temperature_check::thermal_zone z;
  z.name = "Zone1";
  z.temperature = 55.0;
  z.throttle_reasons = 1;
  z.active = true;

  const temperature_check::thermal_zone copy(z);
  EXPECT_EQ(copy.name, "Zone1");
  EXPECT_DOUBLE_EQ(copy.temperature, 55.0);
  EXPECT_EQ(copy.throttle_reasons, 1);
  EXPECT_TRUE(copy.active);
}

TEST(ThermalZone, Assignment) {
  temperature_check::thermal_zone z;
  z.name = "Zone2";
  z.temperature = 80.0;

  temperature_check::thermal_zone other;
  other = z;
  EXPECT_EQ(other.name, "Zone2");
  EXPECT_DOUBLE_EQ(other.temperature, 80.0);
}

TEST(ThermalZone, BuildMetrics) {
  temperature_check::thermal_zone z;
  z.name = "Zone1";
  z.temperature = 50.0;
  z.throttle_reasons = 0;
  z.active = true;

  PB::Metrics::MetricsBundle section;
  z.build_metrics(&section);

  // Should produce 3 metrics: temperature, active, throttle_reasons
  EXPECT_EQ(section.value_size(), 3);
}

// ============================================================================
// temperature_data thread-safety tests (require COM for WMI)
// ============================================================================

class TemperatureDataTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    com_initialized_ = SUCCEEDED(hr) || hr == S_FALSE;
    CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);
  }

  static void TearDownTestSuite() {
    if (com_initialized_) {
      CoUninitialize();
    }
  }

  static bool com_initialized_;
};

bool TemperatureDataTest::com_initialized_ = false;

TEST(TemperatureData, GetReturnsEmptyBeforeFetch) {
  temperature_check::temperature_data data;
  const auto result = data.get();
  EXPECT_TRUE(result.empty());
}

TEST_F(TemperatureDataTest, FetchDoesNotThrow) {
  temperature_check::temperature_data data;
  // fetch() queries live WMI — should not throw even if no thermal zones exist
  // (it may disable itself silently).
  EXPECT_NO_THROW(data.fetch());
}

TEST_F(TemperatureDataTest, FetchedDataHasNames) {
  temperature_check::temperature_data data;
  try {
    data.fetch();
  } catch (...) {
    // Some machines may not have thermal zone WMI classes — skip gracefully
    GTEST_SKIP() << "Thermal zone WMI data not available on this machine";
  }
  for (const auto &z : data.get()) {
    EXPECT_FALSE(z.name.empty());
  }
}
