// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <str/saturate.hpp>

// to_int64_saturating stands in for static_cast<long long> everywhere the
// filter engine converts a float, so it has to truncate toward zero exactly
// like the cast did. Rounding here silently changes documented check results
// (`9.7` reads as `9`, not `10`).
TEST(Saturate, TruncatesTowardZero) {
  EXPECT_EQ(9, str::to_int64_saturating(9.7));
  EXPECT_EQ(3, str::to_int64_saturating(3.99));
  EXPECT_EQ(2, str::to_int64_saturating(2.5));
  EXPECT_EQ(-2, str::to_int64_saturating(-2.5));
  EXPECT_EQ(-3, str::to_int64_saturating(-3.99));
  EXPECT_EQ(0, str::to_int64_saturating(-0.9));
  EXPECT_EQ(0, str::to_int64_saturating(0.0));
}

TEST(Saturate, ClampsOutOfRangeValues) {
  EXPECT_EQ((std::numeric_limits<long long>::max)(), str::to_int64_saturating(1.0e30));
  EXPECT_EQ((std::numeric_limits<long long>::min)(), str::to_int64_saturating(-1.0e30));
  EXPECT_EQ((std::numeric_limits<long long>::max)(), str::to_int64_saturating(std::numeric_limits<double>::infinity()));
  EXPECT_EQ((std::numeric_limits<long long>::min)(), str::to_int64_saturating(-std::numeric_limits<double>::infinity()));
}

TEST(Saturate, NanBecomesZero) { EXPECT_EQ(0, str::to_int64_saturating(std::numeric_limits<double>::quiet_NaN())); }

TEST(Saturate, FitsInt64) {
  EXPECT_TRUE(str::fits_int64(9.7));
  EXPECT_TRUE(str::fits_int64(0.0));
  EXPECT_TRUE(str::fits_int64(-9223372036854775808.0));
  EXPECT_FALSE(str::fits_int64(1.0e30));
  EXPECT_FALSE(str::fits_int64(-1.0e30));
  // The bound is 2^63 exactly: numeric_limits<long long>::max() rounds up to
  // it as a double, so the value itself must not be admitted.
  EXPECT_FALSE(str::fits_int64(9223372036854775808.0));
  EXPECT_FALSE(str::fits_int64(std::numeric_limits<double>::quiet_NaN()));
}
