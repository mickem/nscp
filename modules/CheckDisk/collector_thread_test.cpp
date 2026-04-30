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

#include "collector_thread.hpp"

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

