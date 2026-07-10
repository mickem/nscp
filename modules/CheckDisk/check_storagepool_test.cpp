// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_storagepool.hpp"

#include <gtest/gtest.h>

// nscapi::plugin_singleton is defined once in check_disk_io_test.cpp for the
// merged check_disk_test target.

using storagepool_check::storagepool;

TEST(StoragePool, HealthStatusMapping) {
  EXPECT_EQ(storagepool::map_health_status(0), "Healthy");
  EXPECT_EQ(storagepool::map_health_status(1), "Warning");
  EXPECT_EQ(storagepool::map_health_status(2), "Unhealthy");
  EXPECT_EQ(storagepool::map_health_status(9), "Unknown");
}

TEST(StoragePool, DerivedCapacityValues) {
  storagepool p;
  p.name = "Pool1";
  p.capacity = 1000;
  p.used = 250;
  EXPECT_EQ(p.get_capacity(), 1000);
  EXPECT_EQ(p.get_used(), 250);
  EXPECT_EQ(p.get_free(), 750);
  EXPECT_EQ(p.get_free_pct(), 75);
  EXPECT_EQ(p.get_used_pct(), 25);
}

TEST(StoragePool, PctGuardsZeroCapacity) {
  storagepool p;
  EXPECT_EQ(p.get_free_pct(), 0);
  EXPECT_EQ(p.get_used_pct(), 0);
}

TEST(StoragePool, ReadonlyFlag) {
  storagepool p;
  EXPECT_EQ(p.get_is_readonly(), 0);
  p.is_readonly = true;
  EXPECT_EQ(p.get_is_readonly(), 1);
}
