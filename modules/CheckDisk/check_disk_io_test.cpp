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

#include <nscapi/nscapi_helper_singleton.hpp>

#ifdef WIN32
#include <objbase.h>
#endif

#include "check_disk_io.hpp"

// Provide the NSCAPI singleton so modern_filter.cpp can link.
// The core_wrapper is constructed with null function pointers, which means
// all log calls are harmless no-ops.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

// ============================================================================
// disk_io struct tests
// ============================================================================

TEST(DiskIo, DefaultConstruction) {
  disk_io_check::disk_io d;
  EXPECT_EQ(d.name, "");
  EXPECT_EQ(d.read_bytes_per_sec, 0);
  EXPECT_EQ(d.write_bytes_per_sec, 0);
  EXPECT_EQ(d.reads_per_sec, 0);
  EXPECT_EQ(d.writes_per_sec, 0);
  EXPECT_EQ(d.queue_length, 0);
  EXPECT_EQ(d.percent_disk_time, 0);
  EXPECT_EQ(d.percent_idle_time, 0);
  EXPECT_EQ(d.split_io_per_sec, 0);
}

TEST(DiskIo, Accessors) {
  disk_io_check::disk_io d;
  d.name = "C:";
  d.read_bytes_per_sec = 1000;
  d.write_bytes_per_sec = 2000;
  d.reads_per_sec = 10;
  d.writes_per_sec = 20;
  d.queue_length = 3;
  d.percent_disk_time = 45;
  d.percent_idle_time = 55;
  d.split_io_per_sec = 5;

  EXPECT_EQ(d.get_name(), "C:");
  EXPECT_EQ(d.get_read_bytes_per_sec(), 1000);
  EXPECT_EQ(d.get_write_bytes_per_sec(), 2000);
  EXPECT_EQ(d.get_reads_per_sec(), 10);
  EXPECT_EQ(d.get_writes_per_sec(), 20);
  EXPECT_EQ(d.get_queue_length(), 3);
  EXPECT_EQ(d.get_percent_disk_time(), 45);
  EXPECT_EQ(d.get_percent_idle_time(), 55);
  EXPECT_EQ(d.get_split_io_per_sec(), 5);
}

TEST(DiskIo, ComputedTotalBytesPerSec) {
  disk_io_check::disk_io d;
  d.read_bytes_per_sec = 1000;
  d.write_bytes_per_sec = 2000;
  EXPECT_EQ(d.get_total_bytes_per_sec(), 3000);
}

TEST(DiskIo, ComputedTotalBytesPerSecZero) {
  disk_io_check::disk_io d;
  EXPECT_EQ(d.get_total_bytes_per_sec(), 0);
}

TEST(DiskIo, ComputedIops) {
  disk_io_check::disk_io d;
  d.reads_per_sec = 100;
  d.writes_per_sec = 200;
  EXPECT_EQ(d.get_iops(), 300);
}

TEST(DiskIo, ComputedIopsZero) {
  const disk_io_check::disk_io d;
  EXPECT_EQ(d.get_iops(), 0);
}

TEST(DiskIo, ShowReturnsName) {
  disk_io_check::disk_io d;
  d.name = "D:";
  EXPECT_EQ(d.show(), "D:");
}

TEST(DiskIo, CopyConstruction) {
  disk_io_check::disk_io d;
  d.name = "C:";
  d.read_bytes_per_sec = 500;
  d.write_bytes_per_sec = 600;
  d.reads_per_sec = 10;
  d.writes_per_sec = 20;
  d.queue_length = 2;
  d.percent_disk_time = 50;
  d.percent_idle_time = 50;
  d.split_io_per_sec = 3;

  const disk_io_check::disk_io copy(d);
  EXPECT_EQ(copy.name, "C:");
  EXPECT_EQ(copy.read_bytes_per_sec, 500);
  EXPECT_EQ(copy.write_bytes_per_sec, 600);
  EXPECT_EQ(copy.reads_per_sec, 10);
  EXPECT_EQ(copy.writes_per_sec, 20);
  EXPECT_EQ(copy.queue_length, 2);
  EXPECT_EQ(copy.percent_disk_time, 50);
  EXPECT_EQ(copy.percent_idle_time, 50);
  EXPECT_EQ(copy.split_io_per_sec, 3);
}

TEST(DiskIo, Assignment) {
  disk_io_check::disk_io d;
  d.name = "E:";
  d.read_bytes_per_sec = 999;

  disk_io_check::disk_io other;
  other = d;
  EXPECT_EQ(other.name, "E:");
  EXPECT_EQ(other.read_bytes_per_sec, 999);
}

TEST(DiskIo, BuildMetrics) {
  disk_io_check::disk_io d;
  d.name = "C:";
  d.read_bytes_per_sec = 1000;
  d.write_bytes_per_sec = 2000;
  d.reads_per_sec = 10;
  d.writes_per_sec = 20;
  d.queue_length = 3;
  d.percent_disk_time = 45;
  d.percent_idle_time = 55;
  d.split_io_per_sec = 5;

  PB::Metrics::MetricsBundle section;
  d.build_metrics(&section);

  EXPECT_EQ(section.value_size(), 8);
}

// ============================================================================
// disk_free struct tests
// ============================================================================

TEST(DiskFree, DefaultConstruction) {
  const disk_free_check::disk_free d;
  EXPECT_EQ(d.name, "");
  EXPECT_EQ(d.total, 0);
  EXPECT_EQ(d.free, 0);
  EXPECT_EQ(d.user_free, 0);
}

TEST(DiskFree, Accessors) {
  disk_free_check::disk_free d;
  d.name = "C:";
  d.total = 100000;
  d.free = 40000;
  d.user_free = 35000;

  EXPECT_EQ(d.get_name(), "C:");
  EXPECT_EQ(d.get_total(), 100000);
  EXPECT_EQ(d.get_free(), 40000);
  EXPECT_EQ(d.get_user_free(), 35000);
}

TEST(DiskFree, ComputedUsed) {
  disk_free_check::disk_free d;
  d.total = 100000;
  d.free = 40000;
  EXPECT_EQ(d.get_used(), 60000);
}

TEST(DiskFree, ComputedUsedWhenEmpty) {
  disk_free_check::disk_free d;
  EXPECT_EQ(d.get_used(), 0);
}

TEST(DiskFree, ComputedFreePct) {
  disk_free_check::disk_free d;
  d.total = 100;
  d.free = 40;
  EXPECT_EQ(d.get_free_pct(), 40);
}

TEST(DiskFree, ComputedUsedPct) {
  disk_free_check::disk_free d;
  d.total = 100;
  d.free = 40;
  EXPECT_EQ(d.get_used_pct(), 60);
}

TEST(DiskFree, PctWhenTotalZero) {
  disk_free_check::disk_free d;
  d.total = 0;
  d.free = 0;
  EXPECT_EQ(d.get_free_pct(), 0);
  EXPECT_EQ(d.get_used_pct(), 0);
}

TEST(DiskFree, PctRoundsDown) {
  disk_free_check::disk_free d;
  d.total = 3;
  d.free = 1;
  // 1 * 100 / 3 = 33 (integer division)
  EXPECT_EQ(d.get_free_pct(), 33);
  // (3 - 1) * 100 / 3 = 66
  EXPECT_EQ(d.get_used_pct(), 66);
}

TEST(DiskFree, ShowReturnsName) {
  disk_free_check::disk_free d;
  d.name = "D:";
  EXPECT_EQ(d.show(), "D:");
}

TEST(DiskFree, CopyConstruction) {
  disk_free_check::disk_free d;
  d.name = "C:";
  d.total = 500000;
  d.free = 200000;
  d.user_free = 180000;

  disk_free_check::disk_free copy(d);
  EXPECT_EQ(copy.name, "C:");
  EXPECT_EQ(copy.total, 500000);
  EXPECT_EQ(copy.free, 200000);
  EXPECT_EQ(copy.user_free, 180000);
}

TEST(DiskFree, Assignment) {
  disk_free_check::disk_free d;
  d.name = "F:";
  d.total = 999;
  d.free = 111;
  d.user_free = 100;

  disk_free_check::disk_free other;
  other = d;
  EXPECT_EQ(other.name, "F:");
  EXPECT_EQ(other.total, 999);
  EXPECT_EQ(other.free, 111);
  EXPECT_EQ(other.user_free, 100);
}

TEST(DiskFree, BuildMetrics) {
  disk_free_check::disk_free d;
  d.name = "C:";
  d.total = 100000;
  d.free = 40000;
  d.user_free = 35000;

  PB::Metrics::MetricsBundle section;
  d.build_metrics(&section);

  // Should produce 6 metrics: total, free, used, user_free, free_pct, used_pct
  EXPECT_EQ(section.value_size(), 6);
}

TEST(DiskFree, FullDisk) {
  disk_free_check::disk_free d;
  d.name = "Z:";
  d.total = 1000000;
  d.free = 0;
  d.user_free = 0;

  EXPECT_EQ(d.get_used(), 1000000);
  EXPECT_EQ(d.get_free_pct(), 0);
  EXPECT_EQ(d.get_used_pct(), 100);
}

TEST(DiskFree, EmptyDisk) {
  disk_free_check::disk_free d;
  d.name = "Z:";
  d.total = 1000000;
  d.free = 1000000;
  d.user_free = 1000000;

  EXPECT_EQ(d.get_used(), 0);
  EXPECT_EQ(d.get_free_pct(), 100);
  EXPECT_EQ(d.get_used_pct(), 0);
}

// ============================================================================
// disk_io_data thread-safety tests (require COM for WMI)
// ============================================================================

class DiskIoDataTest : public ::testing::Test {
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

bool DiskIoDataTest::com_initialized_ = false;

TEST(DiskIoData, GetReturnsEmptyBeforeFetch) {
  disk_io_check::disk_io_data data;
  const auto result = data.get();
  EXPECT_TRUE(result.empty());
}

TEST_F(DiskIoDataTest, FetchPopulatesData) {
  disk_io_check::disk_io_data data;
  // fetch() queries live WMI - should not throw on a normal Windows system
  EXPECT_NO_THROW(data.fetch());
  const auto result = data.get();
  // Every Windows system has at least one logical disk
  EXPECT_FALSE(result.empty());
}

TEST_F(DiskIoDataTest, FetchedDataHasNames) {
  disk_io_check::disk_io_data data;
  data.fetch();
  for (const auto &d : data.get()) {
    EXPECT_FALSE(d.name.empty());
  }
}

// ============================================================================
// disk_free_data thread-safety tests
// ============================================================================

TEST(DiskFreeData, GetReturnsEmptyBeforeFetch) {
  disk_free_check::disk_free_data data;
  const auto result = data.get();
  EXPECT_TRUE(result.empty());
}

TEST(DiskFreeData, FetchPopulatesData) {
  disk_free_check::disk_free_data data;
  EXPECT_NO_THROW(data.fetch());
  auto result = data.get();
  EXPECT_FALSE(result.empty());
}

TEST(DiskFreeData, FetchedDrivesHavePositiveTotal) {
  disk_free_check::disk_free_data data;
  data.fetch();
  for (const auto &d : data.get()) {
    EXPECT_FALSE(d.name.empty());
    EXPECT_GT(d.total, 0);
    EXPECT_GE(d.free, 0);
    EXPECT_LE(d.free, d.total);
    EXPECT_GE(d.user_free, 0);
  }
}

TEST(DiskFreeData, FetchedDriveNameHasNoTrailingBackslash) {
  disk_free_check::disk_free_data data;
  data.fetch();
  for (const auto &d : data.get()) {
    EXPECT_NE(d.name.back(), '\\');
  }
}

// ============================================================================
// Large value edge cases
// ============================================================================

TEST(DiskIo, LargeValues) {
  disk_io_check::disk_io d;
  d.read_bytes_per_sec = 5000000000LL;   // 5 GB/s
  d.write_bytes_per_sec = 3000000000LL;  // 3 GB/s
  d.reads_per_sec = 500000;
  d.writes_per_sec = 300000;

  EXPECT_EQ(d.get_total_bytes_per_sec(), 8000000000LL);
  EXPECT_EQ(d.get_iops(), 800000);
}

TEST(DiskFree, LargeValues) {
  disk_free_check::disk_free d;
  d.total = 4000000000000LL;  // 4 TB
  d.free = 1000000000000LL;   // 1 TB
  d.user_free = 900000000000LL;

  EXPECT_EQ(d.get_used(), 3000000000000LL);
  EXPECT_EQ(d.get_free_pct(), 25);
  EXPECT_EQ(d.get_used_pct(), 75);
}
