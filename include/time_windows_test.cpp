// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <time_windows.hpp>

TEST(TimeWindows, DecodesEachWindowInOrder) {
  time_windows::list out;
  EXPECT_EQ("", time_windows::decode({"5s", "1m", "5m"}, out));
  ASSERT_EQ(3u, out.size());
  EXPECT_EQ("5s", out[0].first);
  EXPECT_EQ(5, out[0].second);
  EXPECT_EQ(60, out[1].second);
  EXPECT_EQ(300, out[2].second);
}

TEST(TimeWindows, RejectsAnUnparsableWindow) {
  time_windows::list out;
  EXPECT_EQ(0u, time_windows::decode({"5x"}, out).find("Invalid time '5x'"));
}

// Both platforms reject a zero window: there is nothing to average over.
TEST(TimeWindows, RejectsAZeroWindow) {
  time_windows::list out;
  EXPECT_EQ("Invalid time '0s': the window must be at least one second", time_windows::decode({"5s", "0s"}, out));
}
