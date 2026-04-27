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

#include "check_disk_health.hpp"

#include <gtest/gtest.h>

#include <nscapi/nscapi_helper_singleton.hpp>

// Provide the NSCAPI singleton so modern_filter.cpp can link.
// nscapi::plugin_singleton is defined once in check_disk_io_test.cpp for the
// merged check_disk_test target.

// ============================================================================
// disk_health struct tests
// ============================================================================

TEST(DiskHealth, DefaultConstruction) {
  disk_health_check::disk_health h;
  EXPECT_EQ(h.name, "");
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
  // Space fields should be zero
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
  EXPECT_EQ(it->total, 0);  // No free data
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
}
