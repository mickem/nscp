// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <str/rfc3339.hpp>

TEST(Rfc3339, ParsesTheFormsTheApisEmit) {
  const auto plain = str::parse_rfc3339("2026-08-12T07:44:00Z");
  ASSERT_TRUE(plain);
  EXPECT_EQ(boost::posix_time::to_iso_extended_string(plain.value()), "2026-08-12T07:44:00");
  const auto fractional = str::parse_rfc3339("2026-08-12T07:44:00.123456789Z");
  ASSERT_TRUE(fractional);
  EXPECT_EQ(plain.value(), fractional.value()) << "fractional seconds are dropped";
  const auto no_zone = str::parse_rfc3339("2026-08-12T07:44:00");
  ASSERT_TRUE(no_zone);
  EXPECT_EQ(plain.value(), no_zone.value());
}

TEST(Rfc3339, RejectsWhatIsNotATimestamp) {
  EXPECT_FALSE(str::parse_rfc3339(""));
  EXPECT_FALSE(str::parse_rfc3339("not a date"));
  EXPECT_FALSE(str::parse_rfc3339("2026-13-45T99:00:00Z"));
  EXPECT_EQ(str::seconds_since_rfc3339(""), -1);
  EXPECT_EQ(str::seconds_since_rfc3339("not a date"), -1);
}

TEST(Rfc3339, SecondsSinceCountsFromNow) {
  EXPECT_GT(str::seconds_since_rfc3339("2020-01-01T00:00:00Z"), 24 * 3600LL * 365 * 5);
  EXPECT_GT(str::seconds_since_rfc3339("2020-01-01T00:00:00.5Z"), 24 * 3600LL * 365 * 5);
  // The docker zero value ("never started") lies outside the date range
  // boost accepts, so it reads as no timestamp at all.
  EXPECT_EQ(str::seconds_since_rfc3339("0001-01-01T00:00:00Z"), -1);
}
