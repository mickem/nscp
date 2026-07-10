// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_disk_health.hpp"

#include <gtest/gtest.h>

#include <nscapi/nscapi_helper_singleton.hpp>

// Provide the NSCAPI singleton so modern_filter.cpp can link.
// nscapi::plugin_singleton is defined once per test binary: in
// check_disk_io_test.cpp for the Windows check_disk_test target and in
// check_disk_unix_test.cpp for check_disk_unix_test.

// ============================================================================
// disk_health struct tests
// ============================================================================

TEST(DiskHealth, DefaultConstruction) {
  disk_health_check::disk_health h;
  EXPECT_EQ(h.name, "");
  EXPECT_FALSE(h.has_space);
  EXPECT_EQ(h.get_has_space(), 0);
  EXPECT_EQ(h.total, 0);
  EXPECT_EQ(h.free, 0);
  EXPECT_EQ(h.user_free, 0);
  EXPECT_EQ(h.read_bytes_per_sec, 0);
  EXPECT_EQ(h.write_bytes_per_sec, 0);
  EXPECT_EQ(h.reads_per_sec, 0);
  EXPECT_EQ(h.writes_per_sec, 0);
  EXPECT_EQ(h.queue_length, 0);
  EXPECT_EQ(h.percent_disk_time, 0);
  EXPECT_EQ(h.percent_idle_time, 0);
  EXPECT_EQ(h.split_io_per_sec, 0);
}

TEST(DiskHealth, SpaceAccessors) {
  disk_health_check::disk_health h;
  h.name = "C:";
  h.total = 100000;
  h.free = 40000;
  h.user_free = 35000;

  EXPECT_EQ(h.get_name(), "C:");
  EXPECT_EQ(h.get_total(), 100000);
  EXPECT_EQ(h.get_free(), 40000);
  EXPECT_EQ(h.get_used(), 60000);
  EXPECT_EQ(h.get_user_free(), 35000);
  EXPECT_EQ(h.get_free_pct(), 40);
  EXPECT_EQ(h.get_used_pct(), 60);
}

TEST(DiskHealth, IoAccessors) {
  disk_health_check::disk_health h;
  h.read_bytes_per_sec = 1000;
  h.write_bytes_per_sec = 2000;
  h.reads_per_sec = 10;
  h.writes_per_sec = 20;
  h.queue_length = 3;
  h.percent_disk_time = 45;
  h.percent_idle_time = 55;
  h.split_io_per_sec = 5;

  EXPECT_EQ(h.get_read_bytes_per_sec(), 1000);
  EXPECT_EQ(h.get_write_bytes_per_sec(), 2000);
  EXPECT_EQ(h.get_total_bytes_per_sec(), 3000);
  EXPECT_EQ(h.get_reads_per_sec(), 10);
  EXPECT_EQ(h.get_writes_per_sec(), 20);
  EXPECT_EQ(h.get_iops(), 30);
  EXPECT_EQ(h.get_queue_length(), 3);
  EXPECT_EQ(h.get_percent_disk_time(), 45);
  EXPECT_EQ(h.get_percent_idle_time(), 55);
  EXPECT_EQ(h.get_split_io_per_sec(), 5);
}

TEST(DiskHealth, PctWhenTotalZero) {
  disk_health_check::disk_health h;
  h.total = 0;
  h.free = 0;
  EXPECT_EQ(h.get_free_pct(), 0);
  EXPECT_EQ(h.get_used_pct(), 0);
}

TEST(DiskHealth, PctRoundsDown) {
  disk_health_check::disk_health h;
  h.total = 3;
  h.free = 1;
  EXPECT_EQ(h.get_free_pct(), 33);
  EXPECT_EQ(h.get_used_pct(), 66);
}

TEST(DiskHealth, ShowReturnsName) {
  disk_health_check::disk_health h;
  h.name = "D:";
  EXPECT_EQ(h.show(), "D:");
}

TEST(DiskHealth, CopyConstruction) {
  disk_health_check::disk_health h;
  h.name = "C:";
  h.total = 500000;
  h.free = 200000;
  h.user_free = 180000;
  h.read_bytes_per_sec = 1000;
  h.writes_per_sec = 50;
  h.queue_length = 2;

  const disk_health_check::disk_health copy(h);
  EXPECT_EQ(copy.name, "C:");
  EXPECT_EQ(copy.total, 500000);
  EXPECT_EQ(copy.free, 200000);
  EXPECT_EQ(copy.user_free, 180000);
  EXPECT_EQ(copy.read_bytes_per_sec, 1000);
  EXPECT_EQ(copy.writes_per_sec, 50);
  EXPECT_EQ(copy.queue_length, 2);
}

TEST(DiskHealth, Assignment) {
  disk_health_check::disk_health h;
  h.name = "E:";
  h.total = 999;
  h.read_bytes_per_sec = 123;

  disk_health_check::disk_health other;
  other = h;
  EXPECT_EQ(other.name, "E:");
  EXPECT_EQ(other.total, 999);
  EXPECT_EQ(other.read_bytes_per_sec, 123);
}

TEST(DiskHealth, LargeValues) {
  disk_health_check::disk_health h;
  h.total = 4000000000000LL;
  h.free = 1000000000000LL;
  h.read_bytes_per_sec = 5000000000LL;
  h.write_bytes_per_sec = 3000000000LL;
  h.reads_per_sec = 500000;
  h.writes_per_sec = 300000;

  EXPECT_EQ(h.get_used(), 3000000000000LL);
  EXPECT_EQ(h.get_free_pct(), 25);
  EXPECT_EQ(h.get_used_pct(), 75);
  EXPECT_EQ(h.get_total_bytes_per_sec(), 8000000000LL);
  EXPECT_EQ(h.get_iops(), 800000);
}

// ============================================================================
// join() tests
// ============================================================================

TEST(DiskHealthJoin, BothEmpty) {
  disk_io_check::disks_type io;
  disk_free_check::drives_type df;
  auto result = disk_health_check::join(io, df);
  EXPECT_TRUE(result.empty());
}

TEST(DiskHealthJoin, IoOnlyDrive) {
  disk_io_check::disks_type io;
  disk_io_check::disk_io d;
  d.name = "C:";
  d.read_bytes_per_sec = 500;
  d.percent_disk_time = 30;
  io.push_back(d);

  disk_free_check::drives_type df;

  auto result = disk_health_check::join(io, df);
  ASSERT_EQ(result.size(), 1u);
  const auto &h = result.front();
  EXPECT_EQ(h.name, "C:");
  EXPECT_EQ(h.read_bytes_per_sec, 500);
  EXPECT_EQ(h.percent_disk_time, 30);
  // Space fields should be zero and flagged as absent
  EXPECT_FALSE(h.has_space);
  EXPECT_EQ(h.total, 0);
  EXPECT_EQ(h.free, 0);
}

TEST(DiskHealthJoin, FreeOnlyDrive) {
  disk_io_check::disks_type io;

  disk_free_check::drives_type df;
  disk_free_check::disk_free f;
  f.name = "D:";
  f.total = 100000;
  f.free = 50000;
  f.user_free = 45000;
  df.push_back(f);

  auto result = disk_health_check::join(io, df);
  ASSERT_EQ(result.size(), 1u);
  const auto &h = result.front();
  EXPECT_EQ(h.name, "D:");
  EXPECT_TRUE(h.has_space);
  EXPECT_EQ(h.total, 100000);
  EXPECT_EQ(h.free, 50000);
  EXPECT_EQ(h.user_free, 45000);
  // I/O fields should be zero
  EXPECT_EQ(h.read_bytes_per_sec, 0);
  EXPECT_EQ(h.queue_length, 0);
}

TEST(DiskHealthJoin, MatchingDrives) {
  disk_io_check::disks_type io;
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
  io.push_back(d);

  disk_free_check::drives_type df;
  disk_free_check::disk_free f;
  f.name = "C:";
  f.total = 100000;
  f.free = 40000;
  f.user_free = 35000;
  df.push_back(f);

  auto result = disk_health_check::join(io, df);
  ASSERT_EQ(result.size(), 1u);
  const auto &h = result.front();
  EXPECT_EQ(h.name, "C:");
  // Space fields
  EXPECT_TRUE(h.has_space);
  EXPECT_EQ(h.total, 100000);
  EXPECT_EQ(h.free, 40000);
  EXPECT_EQ(h.user_free, 35000);
  // I/O fields
  EXPECT_EQ(h.read_bytes_per_sec, 1000);
  EXPECT_EQ(h.write_bytes_per_sec, 2000);
  EXPECT_EQ(h.reads_per_sec, 10);
  EXPECT_EQ(h.writes_per_sec, 20);
  EXPECT_EQ(h.queue_length, 3);
  EXPECT_EQ(h.percent_disk_time, 45);
  EXPECT_EQ(h.percent_idle_time, 55);
  EXPECT_EQ(h.split_io_per_sec, 5);
}

TEST(DiskHealthJoin, MultipleDrives) {
  disk_io_check::disks_type io;
  disk_io_check::disk_io d1;
  d1.name = "C:";
  d1.read_bytes_per_sec = 100;
  io.push_back(d1);
  disk_io_check::disk_io d2;
  d2.name = "D:";
  d2.read_bytes_per_sec = 200;
  io.push_back(d2);

  disk_free_check::drives_type df;
  disk_free_check::disk_free f1;
  f1.name = "C:";
  f1.total = 100000;
  f1.free = 50000;
  df.push_back(f1);
  disk_free_check::disk_free f2;
  f2.name = "D:";
  f2.total = 200000;
  f2.free = 80000;
  df.push_back(f2);

  auto result = disk_health_check::join(io, df);
  ASSERT_EQ(result.size(), 2u);

  // Results are sorted by name since std::map is ordered
  auto it = result.begin();
  EXPECT_EQ(it->name, "C:");
  EXPECT_EQ(it->read_bytes_per_sec, 100);
  EXPECT_EQ(it->total, 100000);
  ++it;
  EXPECT_EQ(it->name, "D:");
  EXPECT_EQ(it->read_bytes_per_sec, 200);
  EXPECT_EQ(it->total, 200000);
}

TEST(DiskHealthJoin, MismatchedDrives) {
  disk_io_check::disks_type io;
  disk_io_check::disk_io d1;
  d1.name = "C:";
  d1.read_bytes_per_sec = 100;
  io.push_back(d1);
  disk_io_check::disk_io d2;
  d2.name = "_Total";
  d2.read_bytes_per_sec = 300;
  io.push_back(d2);

  disk_free_check::drives_type df;
  disk_free_check::disk_free f;
  f.name = "C:";
  f.total = 100000;
  f.free = 50000;
  df.push_back(f);
  disk_free_check::disk_free f2;
  f2.name = "E:";
  f2.total = 500000;
  f2.free = 400000;
  df.push_back(f2);

  auto result = disk_health_check::join(io, df);
  // Should have C:, E:, _Total
  ASSERT_EQ(result.size(), 3u);

  // Find specific entries (map ordering: C:, E:, _Total)
  auto it = result.begin();
  EXPECT_EQ(it->name, "C:");
  EXPECT_EQ(it->read_bytes_per_sec, 100);
  EXPECT_EQ(it->total, 100000);

  ++it;
  EXPECT_EQ(it->name, "E:");
  EXPECT_EQ(it->total, 500000);
  EXPECT_EQ(it->read_bytes_per_sec, 0);  // No I/O data

  ++it;
  EXPECT_EQ(it->name, "_Total");
  EXPECT_EQ(it->read_bytes_per_sec, 300);
  EXPECT_EQ(it->total, 0);          // No free data
  EXPECT_FALSE(it->has_space);      // ... and flagged as such
}

TEST(DiskHealthJoin, EmptyIoData) {
  disk_io_check::disks_type io;

  disk_free_check::drives_type df;
  disk_free_check::disk_free f;
  f.name = "C:";
  f.total = 100000;
  f.free = 50000;
  df.push_back(f);

  auto result = disk_health_check::join(io, df);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result.front().name, "C:");
  EXPECT_EQ(result.front().total, 100000);
}

TEST(DiskHealthJoin, EmptyFreeData) {
  disk_io_check::disks_type io;
  disk_io_check::disk_io d;
  d.name = "C:";
  d.queue_length = 5;
  io.push_back(d);

  disk_free_check::drives_type df;

  auto result = disk_health_check::join(io, df);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result.front().name, "C:");
  EXPECT_EQ(result.front().queue_length, 5);
  EXPECT_EQ(result.front().total, 0);
  EXPECT_FALSE(result.front().has_space);
}

TEST(DiskHealthJoin, FreeEntryJoinsIoByBackingDevice) {
  // Unix: the free entry carries the physical device backing the mountpoint
  // ("sda" for "/" mounted from /dev/sda1) and must join the I/O of that device.
  disk_io_check::disks_type io;
  disk_io_check::disk_io d;
  d.name = "sda";
  d.read_bytes_per_sec = 1234;
  d.percent_disk_time = 40;
  io.push_back(d);

  disk_free_check::drives_type df;
  disk_free_check::disk_free f;
  f.name = "/";
  f.device = "sda";
  f.total = 100000;
  f.free = 60000;
  df.push_back(f);

  auto result = disk_health_check::join(io, df);
  ASSERT_EQ(result.size(), 1u);
  const auto &h = result.front();
  EXPECT_EQ(h.name, "/");
  EXPECT_TRUE(h.has_space);
  EXPECT_EQ(h.total, 100000);
  EXPECT_EQ(h.read_bytes_per_sec, 1234);
  EXPECT_EQ(h.percent_disk_time, 40);
}

TEST(DiskHealthJoin, UnmatchedIoDeviceStaysIoOnly) {
  // Regression: a physical device whose filesystems could not be mapped back
  // to it (e.g. root on LVM before dm resolution) must not gain fabricated
  // space data (total=0 would read as free_pct=0 -> false critical).
  disk_io_check::disks_type io;
  disk_io_check::disk_io d;
  d.name = "sda";
  d.percent_disk_time = 10;
  io.push_back(d);

  disk_free_check::drives_type df;
  disk_free_check::disk_free f;
  f.name = "/";
  f.device = "";  // mount source could not be resolved to a physical disk
  f.total = 100000;
  f.free = 60000;
  df.push_back(f);

  auto result = disk_health_check::join(io, df);
  ASSERT_EQ(result.size(), 2u);
  auto it = result.begin();
  EXPECT_EQ(it->name, "/");
  EXPECT_TRUE(it->has_space);
  EXPECT_EQ(it->percent_disk_time, 0);
  ++it;
  EXPECT_EQ(it->name, "sda");
  EXPECT_FALSE(it->has_space);
  EXPECT_EQ(it->total, 0);
  EXPECT_EQ(it->percent_disk_time, 10);
}

// ============================================================================
// check_disk_health() default threshold tests
// ============================================================================

namespace {
PB::Common::ResultCode run_health_check(const disk_health_check::health_type &data) {
  PB::Commands::QueryRequestMessage::Request request;
  PB::Commands::QueryResponseMessage::Response response;
  request.set_command("check_disk_health");
  disk_health_check::check::check_disk_health(request, &response, data);
  return response.result();
}
}  // namespace

TEST(CheckDiskHealth, IoOnlyRowIsNotEvaluatedAgainstSpaceThresholds) {
  // An idle I/O-only row (no space data) must not go critical on the default
  // free_pct thresholds despite its zeroed total/free fields.
  disk_io_check::disks_type io;
  disk_io_check::disk_io d;
  d.name = "sda";
  d.percent_idle_time = 100;
  io.push_back(d);

  disk_free_check::drives_type df;
  disk_free_check::disk_free f;
  f.name = "/";
  f.device = "";  // does not match "sda" -> "sda" becomes an I/O-only row
  f.total = 100000;
  f.free = 60000;
  df.push_back(f);

  EXPECT_EQ(run_health_check(disk_health_check::join(io, df)), PB::Common::ResultCode::OK);
}

TEST(CheckDiskHealth, IoOnlyRowStillEvaluatedAgainstIoThresholds) {
  disk_io_check::disks_type io;
  disk_io_check::disk_io d;
  d.name = "sda";
  d.percent_disk_time = 99;
  io.push_back(d);

  EXPECT_EQ(run_health_check(disk_health_check::join(io, {})), PB::Common::ResultCode::CRITICAL);
}

TEST(CheckDiskHealth, LowFreeSpaceStillGoesCritical) {
  disk_free_check::drives_type df;
  disk_free_check::disk_free f;
  f.name = "/";
  f.total = 100000;
  f.free = 5000;  // 5% free < 10%
  df.push_back(f);

  EXPECT_EQ(run_health_check(disk_health_check::join({}, df)), PB::Common::ResultCode::CRITICAL);
}

// ============================================================================
// Physical-disk device state (§4.1) — enum mapping, join, thresholds
// ============================================================================

TEST(DiskDevice, MediaTypeMapping) {
  using disk_device_check::disk_device;
  EXPECT_EQ(disk_device::map_media_type(3), "HDD");
  EXPECT_EQ(disk_device::map_media_type(4), "SSD");
  EXPECT_EQ(disk_device::map_media_type(5), "SCM");
  EXPECT_EQ(disk_device::map_media_type(0), "Unspecified");
  EXPECT_EQ(disk_device::map_media_type(99), "Unspecified");
}

TEST(DiskDevice, HealthStatusMapping) {
  using disk_device_check::disk_device;
  EXPECT_EQ(disk_device::map_health_status(0), "Healthy");
  EXPECT_EQ(disk_device::map_health_status(1), "Warning");
  EXPECT_EQ(disk_device::map_health_status(2), "Unhealthy");
  EXPECT_EQ(disk_device::map_health_status(5), "Unknown");
}

TEST(DiskDevice, OperationalStatusDerivation) {
  disk_device_check::disk_device d;
  d.health_status = "Healthy";
  EXPECT_EQ(d.get_operational_status(), "OK");
  d.is_offline = true;
  EXPECT_EQ(d.get_operational_status(), "Offline");  // offline wins
  d.is_offline = false;
  d.health_status = "Unhealthy";
  EXPECT_EQ(d.get_operational_status(), "Unhealthy");
}

TEST(DiskHealthJoin, AppendsDeviceRows) {
  disk_device_check::devices_type devs;
  disk_device_check::disk_device dev;
  dev.number = 0;
  dev.friendly_name = "Samsung SSD 980";
  dev.serial = "S1234";
  dev.media_type = "SSD";
  dev.health_status = "Healthy";
  devs.push_back(dev);

  auto result = disk_health_check::join({}, {}, devs);
  ASSERT_EQ(result.size(), 1u);
  const auto &h = result.front();
  EXPECT_EQ(h.name, "Samsung SSD 980");
  EXPECT_TRUE(h.has_device);
  EXPECT_FALSE(h.has_space);
  EXPECT_EQ(h.get_media_type(), "SSD");
  EXPECT_EQ(h.get_health_status(), "Healthy");
  EXPECT_EQ(h.get_serial(), "S1234");
  EXPECT_EQ(h.get_operational_status(), "OK");
}

TEST(DiskHealthJoin, DeviceRowFallsBackToDiskNumberName) {
  disk_device_check::devices_type devs;
  disk_device_check::disk_device dev;
  dev.number = 2;  // no friendly name
  devs.push_back(dev);
  auto result = disk_health_check::join({}, {}, devs);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result.front().name, "Disk 2");
}

TEST(CheckDiskHealth, UnhealthyDeviceGoesCritical) {
  disk_device_check::devices_type devs;
  disk_device_check::disk_device dev;
  dev.number = 0;
  dev.friendly_name = "BadDisk";
  dev.health_status = "Unhealthy";
  devs.push_back(dev);
  EXPECT_EQ(run_health_check(disk_health_check::join({}, {}, devs)), PB::Common::ResultCode::CRITICAL);
}

TEST(CheckDiskHealth, OfflineDeviceGoesCritical) {
  disk_device_check::devices_type devs;
  disk_device_check::disk_device dev;
  dev.number = 0;
  dev.friendly_name = "OfflineDisk";
  dev.health_status = "Healthy";
  dev.is_offline = true;
  devs.push_back(dev);
  EXPECT_EQ(run_health_check(disk_health_check::join({}, {}, devs)), PB::Common::ResultCode::CRITICAL);
}

TEST(CheckDiskHealth, WarningDeviceGoesWarning) {
  disk_device_check::devices_type devs;
  disk_device_check::disk_device dev;
  dev.number = 0;
  dev.friendly_name = "WarnDisk";
  dev.health_status = "Warning";
  devs.push_back(dev);
  EXPECT_EQ(run_health_check(disk_health_check::join({}, {}, devs)), PB::Common::ResultCode::WARNING);
}

TEST(CheckDiskHealth, HealthyDeviceIsOk) {
  disk_device_check::devices_type devs;
  disk_device_check::disk_device dev;
  dev.number = 0;
  dev.friendly_name = "GoodDisk";
  dev.health_status = "Healthy";
  devs.push_back(dev);
  EXPECT_EQ(run_health_check(disk_health_check::join({}, {}, devs)), PB::Common::ResultCode::OK);
}
