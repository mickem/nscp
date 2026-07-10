// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#ifdef WIN32
#include <objbase.h>
#endif

#include "check_cpu_frequency.hpp"

// ============================================================================
// cpu_frequency struct tests
// ============================================================================

TEST(CpuFrequency, DefaultConstruction) {
  cpu_frequency_check::cpu_frequency c;
  EXPECT_EQ(c.name, "");
  EXPECT_EQ(c.current_mhz, 0);
  EXPECT_EQ(c.max_mhz, 0);
  EXPECT_EQ(c.number_of_cores, 0);
  EXPECT_EQ(c.number_of_logical_processors, 0);
}

TEST(CpuFrequency, Accessors) {
  cpu_frequency_check::cpu_frequency c;
  c.name = "Intel(R) Core(TM) i7-10700 CPU @ 2.90GHz";
  c.current_mhz = 2900;
  c.max_mhz = 4800;
  c.number_of_cores = 8;
  c.number_of_logical_processors = 16;

  EXPECT_EQ(c.get_name(), "Intel(R) Core(TM) i7-10700 CPU @ 2.90GHz");
  EXPECT_EQ(c.get_current_mhz(), 2900);
  EXPECT_EQ(c.get_max_mhz(), 4800);
  EXPECT_EQ(c.get_number_of_cores(), 8);
  EXPECT_EQ(c.get_number_of_logical_processors(), 16);
}

TEST(CpuFrequency, FrequencyPct) {
  cpu_frequency_check::cpu_frequency c;
  c.current_mhz = 2400;
  c.max_mhz = 4800;
  EXPECT_EQ(c.get_frequency_pct(), 50);
}

TEST(CpuFrequency, FrequencyPctFull) {
  cpu_frequency_check::cpu_frequency c;
  c.current_mhz = 3600;
  c.max_mhz = 3600;
  EXPECT_EQ(c.get_frequency_pct(), 100);
}

TEST(CpuFrequency, FrequencyPctZeroMax) {
  cpu_frequency_check::cpu_frequency c;
  c.current_mhz = 100;
  c.max_mhz = 0;
  EXPECT_EQ(c.get_frequency_pct(), 0);
}

TEST(CpuFrequency, FrequencyPctRoundsDown) {
  cpu_frequency_check::cpu_frequency c;
  c.current_mhz = 1000;
  c.max_mhz = 3000;
  // 1000 * 100 / 3000 = 33 (integer division)
  EXPECT_EQ(c.get_frequency_pct(), 33);
}

TEST(CpuFrequency, ShowReturnsName) {
  cpu_frequency_check::cpu_frequency c;
  c.name = "AMD Ryzen 9 5950X";
  EXPECT_EQ(c.show(), "AMD Ryzen 9 5950X");
}

TEST(CpuFrequency, CopyConstruction) {
  cpu_frequency_check::cpu_frequency c;
  c.name = "TestCPU";
  c.current_mhz = 3000;
  c.max_mhz = 4000;
  c.number_of_cores = 4;
  c.number_of_logical_processors = 8;

  const cpu_frequency_check::cpu_frequency copy(c);
  EXPECT_EQ(copy.name, "TestCPU");
  EXPECT_EQ(copy.current_mhz, 3000);
  EXPECT_EQ(copy.max_mhz, 4000);
  EXPECT_EQ(copy.number_of_cores, 4);
  EXPECT_EQ(copy.number_of_logical_processors, 8);
}

TEST(CpuFrequency, Assignment) {
  cpu_frequency_check::cpu_frequency c;
  c.name = "CPU0";
  c.current_mhz = 2500;
  c.max_mhz = 5000;

  cpu_frequency_check::cpu_frequency other;
  other = c;
  EXPECT_EQ(other.name, "CPU0");
  EXPECT_EQ(other.current_mhz, 2500);
  EXPECT_EQ(other.max_mhz, 5000);
}

TEST(CpuFrequency, BuildMetrics) {
  cpu_frequency_check::cpu_frequency c;
  c.name = "CPU0";
  c.current_mhz = 3000;
  c.max_mhz = 4500;
  c.number_of_cores = 8;
  c.number_of_logical_processors = 16;

  PB::Metrics::MetricsBundle section;
  c.build_metrics(&section);

  // Should produce 6 metrics: current_mhz, max_mhz, frequency_pct, cores, logical_processors, load_pct
  EXPECT_EQ(section.value_size(), 6);
}

TEST(CpuFrequency, SocketAndLoadAccessors) {
  cpu_frequency_check::cpu_frequency c;
  c.socket_id = "CPU0";
  c.socket = "CPU 1";
  c.load_pct = 42;
  EXPECT_EQ(c.get_socket_id(), "CPU0");
  EXPECT_EQ(c.get_socket(), "CPU 1");
  EXPECT_EQ(c.get_load_pct(), 42);
}

TEST(CpuFrequency, SocketDefaultsEmpty) {
  cpu_frequency_check::cpu_frequency c;
  EXPECT_EQ(c.socket_id, "");
  EXPECT_EQ(c.socket, "");
  EXPECT_EQ(c.load_pct, 0);
}

// ============================================================================
// cpu_frequency_data thread-safety tests (require COM for WMI)
// ============================================================================

class CpuFrequencyDataTest : public ::testing::Test {
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

bool CpuFrequencyDataTest::com_initialized_ = false;

TEST(CpuFrequencyData, GetReturnsEmptyBeforeFetch) {
  cpu_frequency_check::cpu_frequency_data data;
  const auto result = data.get();
  EXPECT_TRUE(result.empty());
}

TEST_F(CpuFrequencyDataTest, FetchPopulatesData) {
  cpu_frequency_check::cpu_frequency_data data;
  EXPECT_NO_THROW(data.fetch());
  const auto result = data.get();
  // Every Windows system has at least one processor
  EXPECT_FALSE(result.empty());
}

TEST_F(CpuFrequencyDataTest, FetchedDataHasNames) {
  cpu_frequency_check::cpu_frequency_data data;
  data.fetch();
  for (const auto &c : data.get()) {
    EXPECT_FALSE(c.name.empty());
  }
}

TEST_F(CpuFrequencyDataTest, FetchedDataHasPositiveFrequencies) {
  cpu_frequency_check::cpu_frequency_data data;
  data.fetch();
  for (const auto &c : data.get()) {
    EXPECT_GT(c.current_mhz, 0);
    EXPECT_GT(c.max_mhz, 0);
    EXPECT_LE(c.current_mhz, c.max_mhz * 2);  // Turbo boost can exceed base, but not by 2x
    EXPECT_GT(c.number_of_cores, 0);
    EXPECT_GT(c.number_of_logical_processors, 0);
    EXPECT_GE(c.number_of_logical_processors, c.number_of_cores);
  }
}

TEST_F(CpuFrequencyDataTest, FetchedFrequencyPctInRange) {
  cpu_frequency_check::cpu_frequency_data data;
  data.fetch();
  for (const auto &c : data.get()) {
    long long pct = c.get_frequency_pct();
    EXPECT_GE(pct, 1);
    EXPECT_LE(pct, 200);  // Turbo boost can push current above max temporarily
  }
}

// ============================================================================
// Large value edge cases
// ============================================================================

TEST(CpuFrequency, LargeValues) {
  cpu_frequency_check::cpu_frequency c;
  c.current_mhz = 6000;
  c.max_mhz = 6000;
  c.number_of_cores = 128;
  c.number_of_logical_processors = 256;

  EXPECT_EQ(c.get_frequency_pct(), 100);
  EXPECT_EQ(c.get_number_of_cores(), 128);
  EXPECT_EQ(c.get_number_of_logical_processors(), 256);
}
