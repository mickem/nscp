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
  EXPECT_EQ(c.max_collection_errors, 10);
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

TEST(CollectorThread, MutateFailureLimit) {
  collector_thread c(nullptr, 0);
  c.max_collection_errors = 3;
  EXPECT_EQ(c.max_collection_errors, 3);
}

// ============================================================================
// Consecutive-failure tracking (#1392)
//
// A single failed fetch must not disable a collection for the lifetime of the
// process: the source is typically just busy (WMI under load), and the loop
// recovers on the next tick.
// ============================================================================

TEST(CollectorFailureTracker, StartsWithNothingGivenUp) {
  collector_failure_tracker t(3);
  EXPECT_FALSE(t.given_up());
  EXPECT_EQ(t.consecutive(), 0);
  EXPECT_EQ(t.limit(), 3);
}

TEST(CollectorFailureTracker, KeepsCollectingBelowTheLimit) {
  collector_failure_tracker t(3);
  EXPECT_FALSE(t.failed());
  EXPECT_FALSE(t.failed());
  EXPECT_FALSE(t.given_up());
  EXPECT_EQ(t.consecutive(), 2);
}

TEST(CollectorFailureTracker, GivesUpOnceTheLimitIsReached) {
  collector_failure_tracker t(2);
  EXPECT_FALSE(t.failed());
  // The failure that crosses the limit reports the transition, so the caller
  // can log it exactly once.
  EXPECT_TRUE(t.failed());
  EXPECT_TRUE(t.given_up());
  EXPECT_EQ(t.consecutive(), 2);
}

TEST(CollectorFailureTracker, ReportsTheGiveUpTransitionOnlyOnce) {
  collector_failure_tracker t(1);
  EXPECT_TRUE(t.failed());
  EXPECT_FALSE(t.failed());
  EXPECT_TRUE(t.given_up());
}

TEST(CollectorFailureTracker, SuccessResetsTheCount) {
  collector_failure_tracker t(3);
  t.failed();
  t.failed();
  t.succeeded();
  EXPECT_EQ(t.consecutive(), 0);
  // Intermittent failures must never accumulate into a give-up.
  EXPECT_FALSE(t.failed());
  EXPECT_FALSE(t.failed());
  EXPECT_FALSE(t.given_up());
}

TEST(CollectorFailureTracker, ZeroLimitNeverGivesUp) {
  collector_failure_tracker t(0);
  for (int i = 0; i < 1000; ++i) {
    EXPECT_FALSE(t.failed());
  }
  EXPECT_FALSE(t.given_up());
  EXPECT_EQ(t.consecutive(), 1000);
}

TEST(CollectorFailureTracker, DefaultConstructedNeverGivesUp) {
  collector_failure_tracker t;
  EXPECT_EQ(t.limit(), 0);
  EXPECT_FALSE(t.failed());
  EXPECT_FALSE(t.given_up());
}
