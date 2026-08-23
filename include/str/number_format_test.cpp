// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <str/number_format.hpp>

// The default has to render byte for byte what the checks always rendered, so
// that a check nobody configured keeps its output (#1428).
TEST(number_format, DefaultIsTheHistoricalRendering) {
  const str::number_format fmt;
  EXPECT_TRUE(fmt.is_default());
  EXPECT_EQ(str::render_number(1.0, fmt), "1");
  EXPECT_EQ(str::render_number(25.191, fmt), "25.191");
  EXPECT_EQ(str::render_number(25.1915, fmt), "25.192");
  EXPECT_EQ(str::render_number(0.0, fmt), "0");
  EXPECT_EQ(str::render_number(-70.874, fmt), "-70.874");
}

TEST(number_format, FixedDecimalsKeepTrailingZeros) {
  str::number_format fmt;
  fmt.decimals = 2;
  EXPECT_FALSE(fmt.is_default());
  EXPECT_EQ(str::render_number(1.0, fmt), "1.00");
  EXPECT_EQ(str::render_number(25.191, fmt), "25.19");
  EXPECT_EQ(str::render_number(25.195, fmt), "25.20");
}

TEST(number_format, ZeroDecimalsRoundsToWhole) {
  str::number_format fmt;
  fmt.decimals = 0;
  EXPECT_EQ(str::render_number(25.6, fmt), "26");
  EXPECT_EQ(str::render_number(-25.6, fmt), "-26");
}

TEST(number_format, DecimalSeparator) {
  str::number_format fmt;
  fmt.decimals = 2;
  fmt.decimal_separator = ",";
  EXPECT_EQ(str::render_number(25.191, fmt), "25,19");
  EXPECT_EQ(str::render_number(-25.191, fmt), "-25,19");
}

TEST(number_format, ThousandsSeparator) {
  str::number_format fmt;
  fmt.decimals = 2;
  fmt.decimal_separator = ",";
  fmt.thousands_separator = ".";
  EXPECT_EQ(str::render_number(1006.85, fmt), "1.006,85");
  EXPECT_EQ(str::render_number(1234567.5, fmt), "1.234.567,50");
  EXPECT_EQ(str::render_number(-1234.5, fmt), "-1.234,50");
  EXPECT_EQ(str::render_number(999.5, fmt), "999,50");
}

TEST(number_format, ThousandsSeparatorWithoutDecimals) {
  str::number_format fmt;
  fmt.decimals = 0;
  fmt.thousands_separator = " ";
  EXPECT_EQ(str::render_number(1234567.0, fmt), "1 234 567");
}

TEST(number_format, MultiCharacterSeparators) {
  str::number_format fmt;
  fmt.decimals = 1;
  fmt.thousands_separator = "'";
  fmt.decimal_separator = "::";
  EXPECT_EQ(str::render_number(12345.6, fmt), "12'345::6");
}

TEST(number_format, ApplySeparatorsIsANoOpForTheDefault) {
  const str::number_format fmt;
  EXPECT_EQ(str::apply_separators("1234.5", fmt), "1234.5");
}

TEST(number_format, RenderFixedStripsTrailingZerosOnlyWhenAsked) {
  EXPECT_EQ(str::render_fixed(1.5, -1), "1.5");
  EXPECT_EQ(str::render_fixed(1.5, 3), "1.500");
  EXPECT_EQ(str::render_fixed(1.0, -1), "1");
  EXPECT_EQ(str::render_fixed(1.0, 0), "1");
}

// A huge decimals count used to make setprecision build a multi-megabyte
// string and crash the process; render_fixed clamps to max_decimals as a
// backstop so no caller (a config typo, a hostile REST argument) can trigger
// that. The clamp caps the width, not the value.
TEST(number_format, RenderFixedClampsRunawayDecimals) {
  const std::string clamped = str::render_fixed(1.5, 1000000000);
  EXPECT_EQ(clamped.size(), std::string("1.").size() + str::max_decimals);
  EXPECT_EQ(clamped, "1.500000000000000");
  str::number_format fmt;
  fmt.decimals = 1000000000;
  EXPECT_EQ(str::render_number(1.5, fmt), "1.500000000000000");
}
