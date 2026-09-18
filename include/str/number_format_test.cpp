// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <limits>
#include <sstream>
#include <str/number_format.hpp>
#include <string>

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

// `render_shortest` is the machine-readable counterpart of everything above:
// no locale, no configured decimals, just the shortest string that reads back
// as the same double. It exists because `str::xtos` is a bare stringstream -
// six significant digits - which is fine for a check message and wrong for an
// exposition a scraper parses (a 16 GB memory reading left as "1.6554e+10").
TEST(number_format, RenderShortestKeepsEveryDigitOfAnIntegralValue) {
  EXPECT_EQ(str::render_shortest(16554000000.0), "16554000000");
  EXPECT_EQ(str::render_shortest(12592123904.0), "12592123904");
  EXPECT_EQ(str::render_shortest(0.0), "0");
  EXPECT_EQ(str::render_shortest(-42.0), "-42");
}

TEST(number_format, RenderShortestNeverGrowsAFractionIntoNoise) {
  // The naive "print 17 digits" answer turns 0.1 into 0.10000000000000001.
  EXPECT_EQ(str::render_shortest(0.1), "0.1");
  EXPECT_EQ(str::render_shortest(1.0 / 3.0), "0.3333333333333333");
  EXPECT_EQ(str::render_shortest(97.8293), "97.8293");
}

TEST(number_format, RenderShortestRoundTripsExactly) {
  // The contract: whatever comes out parses back as the identical double, for
  // awkward values as much as round ones.
  const double values[] = {0.1, 1.0 / 3.0, 1e-300, 1.7976931348623157e308, 2.2250738585072014e-308, 1234567.891011, -0.000123456789};
  for (const double value : values) {
    std::istringstream back(str::render_shortest(value));
    back.imbue(std::locale::classic());
    double parsed = 0;
    back >> parsed;
    EXPECT_EQ(parsed, value) << "did not round trip: " << str::render_shortest(value);
  }
}

TEST(number_format, RenderShortestHandlesValuesTooLargeForAnInt64) {
  // The integral fast path casts to long long, so anything outside its range
  // has to take the general path rather than trip undefined behaviour.
  EXPECT_EQ(str::render_shortest(1e19), "1e+19");
  EXPECT_EQ(str::render_shortest(-1e19), "-1e+19");
}

TEST(number_format, RenderShortestNeverReturnsAnEmptyString) {
  // Non-finite values belong to the caller (each exposition format spells them
  // differently), but they must still produce something.
  EXPECT_FALSE(str::render_shortest(std::numeric_limits<double>::quiet_NaN()).empty());
  EXPECT_FALSE(str::render_shortest(std::numeric_limits<double>::infinity()).empty());
}
