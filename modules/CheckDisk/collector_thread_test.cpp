// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "collector_thread.hpp"

#include <gtest/gtest.h>

// ============================================================================
// Static / constant API
// ============================================================================

TEST(CollectorThread, ToStringConstant) { EXPECT_EQ(collector_thread::to_string(), "disk_collector"); }

// ============================================================================
// Default-constructed state
// ============================================================================

TEST(CollectorThread, DefaultConstructionDefaultsToTenSecondInterval) {
  collector_thread c(nullptr, /*plugin_id=*/0);
  EXPECT_EQ(c.collection_interval, 10);
  EXPECT_TRUE(c.disable_.empty());
}

TEST(CollectorThread, GetDiskIoEmptyBeforeStart) {
  collector_thread c(nullptr, /*plugin_id=*/0);
  EXPECT_TRUE(c.get_disk_io().empty());
}

TEST(CollectorThread, GetDiskFreeEmptyBeforeStart) {
  collector_thread c(nullptr, /*plugin_id=*/0);
  EXPECT_TRUE(c.get_disk_free().empty());
}

// ============================================================================
// Mutable public configuration
// ============================================================================

TEST(CollectorThread, MutateConfig) {
  collector_thread c(nullptr, 42);
  c.collection_interval = 5;
  c.disable_ = "disk_io,disk_free";
  EXPECT_EQ(c.collection_interval, 5);
  EXPECT_EQ(c.disable_, "disk_io,disk_free");
}

// ============================================================================
// stop() without a prior start() must be safe (idempotent / null-event path)
// ============================================================================

TEST(CollectorThread, StopWithoutStartIsSafe) {
  collector_thread c(nullptr, 0);
  EXPECT_TRUE(c.stop());
  // Calling it again must remain safe.
  EXPECT_TRUE(c.stop());
}
