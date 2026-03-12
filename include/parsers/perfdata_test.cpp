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

#include <memory>
#include <parsers/perfdata.hpp>
#include <string>
#include <vector>

namespace {

struct perf_entry {
  std::string alias;
  double value = 0.0;
  double warning = -1.0;
  double critical = -1.0;
  double minimum = -1.0;
  double maximum = -1.0;
  std::string unit;
  bool has_warning = false;
  bool has_critical = false;
  bool has_minimum = false;
  bool has_maximum = false;
};

struct string_entry {
  std::string alias;
  std::string value;
};

struct test_builder final : parsers::perfdata::builder {
  std::vector<perf_entry> entries;
  std::vector<string_entry> string_entries;
  perf_entry current_;

  void add(std::string alias) override {
    current_ = perf_entry();
    current_.alias = alias;
  }
  void set_value(double value) override { current_.value = value; }
  void set_warning(double value) override {
    current_.warning = value;
    current_.has_warning = true;
  }
  void set_critical(double value) override {
    current_.critical = value;
    current_.has_critical = true;
  }
  void set_minimum(double value) override {
    current_.minimum = value;
    current_.has_minimum = true;
  }
  void set_maximum(double value) override {
    current_.maximum = value;
    current_.has_maximum = true;
  }
  void set_unit(const std::string& value) override { current_.unit = value; }
  void next() override {
    entries.push_back(current_);
    current_ = perf_entry();
  }
  void add_string(std::string alias, std::string value) override { string_entries.push_back({alias, value}); }
};

}  // namespace

// ==============================================================
// trim_to_double tests
// ==============================================================

TEST(TrimToDouble, IntegerValue) { EXPECT_DOUBLE_EQ(42.0, parsers::perfdata::trim_to_double("42")); }

TEST(TrimToDouble, FloatingPointValue) { EXPECT_DOUBLE_EQ(3.14, parsers::perfdata::trim_to_double("3.14")); }

TEST(TrimToDouble, NegativeValue) { EXPECT_DOUBLE_EQ(-5.0, parsers::perfdata::trim_to_double("-5")); }

TEST(TrimToDouble, ValueWithTrailingUnit) { EXPECT_DOUBLE_EQ(1024.0, parsers::perfdata::trim_to_double("1024B")); }

TEST(TrimToDouble, ValueWithPercentUnit) { EXPECT_DOUBLE_EQ(50.0, parsers::perfdata::trim_to_double("50%")); }

TEST(TrimToDouble, ValueWithCommaDecimalSeparator) { EXPECT_DOUBLE_EQ(1.5, parsers::perfdata::trim_to_double("1,5")); }

TEST(TrimToDouble, EmptyString) { EXPECT_DOUBLE_EQ(0.0, parsers::perfdata::trim_to_double("")); }

TEST(TrimToDouble, OnlyUnit) { EXPECT_DOUBLE_EQ(0.0, parsers::perfdata::trim_to_double("KB")); }

TEST(TrimToDouble, ZeroValue) { EXPECT_DOUBLE_EQ(0.0, parsers::perfdata::trim_to_double("0")); }

TEST(TrimToDouble, VerySmallValue) { EXPECT_DOUBLE_EQ(0.00001, parsers::perfdata::trim_to_double("0.00001")); }

TEST(TrimToDouble, LargeValue) { EXPECT_DOUBLE_EQ(999999999.0, parsers::perfdata::trim_to_double("999999999")); }

TEST(TrimToDouble, ValueWithMultiCharUnit) { EXPECT_DOUBLE_EQ(100.0, parsers::perfdata::trim_to_double("100ms")); }

// ==============================================================
// parse — empty and trivial input
// ==============================================================

class PerfDataParserTest : public ::testing::Test {
 protected:
  std::shared_ptr<test_builder> b = std::make_shared<test_builder>();
};

TEST_F(PerfDataParserTest, EmptyString) {
  parsers::perfdata::parse(b, "");
  EXPECT_TRUE(b->entries.empty());
  EXPECT_TRUE(b->string_entries.empty());
}
// TODO: FIx this test
// TEST_F(PerfDataParserTest, WhitespaceOnly) {
//   parsers::perfdata::parse(b, "     ");
//   EXPECT_TRUE(b->entries.empty());
// }

// ==============================================================
// parse — single metric
// ==============================================================

TEST_F(PerfDataParserTest, SimpleValueOnly) {
  parsers::perfdata::parse(b, "load=42");
  ASSERT_EQ(1, b->entries.size());
  EXPECT_EQ("load", b->entries[0].alias);
  EXPECT_DOUBLE_EQ(42.0, b->entries[0].value);
  EXPECT_EQ("", b->entries[0].unit);
}

TEST_F(PerfDataParserTest, ValueWithUnit) {
  parsers::perfdata::parse(b, "mem=1024B");
  ASSERT_EQ(1, b->entries.size());
  EXPECT_EQ("mem", b->entries[0].alias);
  EXPECT_DOUBLE_EQ(1024.0, b->entries[0].value);
  EXPECT_EQ("B", b->entries[0].unit);
}

TEST_F(PerfDataParserTest, ValueWithPercent) {
  parsers::perfdata::parse(b, "cpu=75%");
  ASSERT_EQ(1, b->entries.size());
  EXPECT_DOUBLE_EQ(75.0, b->entries[0].value);
  EXPECT_EQ("%", b->entries[0].unit);
}

TEST_F(PerfDataParserTest, FullPerfDataAllFields) {
  parsers::perfdata::parse(b, "disk=50g;40;80;0;100");
  ASSERT_EQ(1, b->entries.size());
  const auto& e = b->entries[0];
  EXPECT_EQ("disk", e.alias);
  EXPECT_DOUBLE_EQ(50.0, e.value);
  EXPECT_EQ("g", e.unit);
  EXPECT_TRUE(e.has_warning);
  EXPECT_DOUBLE_EQ(40.0, e.warning);
  EXPECT_TRUE(e.has_critical);
  EXPECT_DOUBLE_EQ(80.0, e.critical);
  EXPECT_TRUE(e.has_minimum);
  EXPECT_DOUBLE_EQ(0.0, e.minimum);
  EXPECT_TRUE(e.has_maximum);
  EXPECT_DOUBLE_EQ(100.0, e.maximum);
}

TEST_F(PerfDataParserTest, ValueWarningCriticalOnly) {
  parsers::perfdata::parse(b, "temp=65;70;90");
  ASSERT_EQ(1, b->entries.size());
  const auto& e = b->entries[0];
  EXPECT_DOUBLE_EQ(65.0, e.value);
  EXPECT_TRUE(e.has_warning);
  EXPECT_DOUBLE_EQ(70.0, e.warning);
  EXPECT_TRUE(e.has_critical);
  EXPECT_DOUBLE_EQ(90.0, e.critical);
  EXPECT_FALSE(e.has_minimum);
  EXPECT_FALSE(e.has_maximum);
}

// ==============================================================
// parse — quoted aliases
// ==============================================================

TEST_F(PerfDataParserTest, QuotedAlias) {
  parsers::perfdata::parse(b, "'disk usage'=50%");
  ASSERT_EQ(1, b->entries.size());
  EXPECT_EQ("disk usage", b->entries[0].alias);
  EXPECT_DOUBLE_EQ(50.0, b->entries[0].value);
  EXPECT_EQ("%", b->entries[0].unit);
}

TEST_F(PerfDataParserTest, QuotedAliasWithSlash) {
  parsers::perfdata::parse(b, "'cpu/total'=80%");
  ASSERT_EQ(1, b->entries.size());
  EXPECT_EQ("cpu/total", b->entries[0].alias);
}

TEST_F(PerfDataParserTest, SimpleAliasNoQuotes) {
  parsers::perfdata::parse(b, "cpu=99");
  ASSERT_EQ(1, b->entries.size());
  EXPECT_EQ("cpu", b->entries[0].alias);
}

// ==============================================================
// parse — multiple metrics
// ==============================================================

TEST_F(PerfDataParserTest, TwoMetrics) {
  parsers::perfdata::parse(b, "cpu=50% mem=1024B");
  ASSERT_EQ(2, b->entries.size());
  EXPECT_EQ("cpu", b->entries[0].alias);
  EXPECT_DOUBLE_EQ(50.0, b->entries[0].value);
  EXPECT_EQ("%", b->entries[0].unit);
  EXPECT_EQ("mem", b->entries[1].alias);
  EXPECT_DOUBLE_EQ(1024.0, b->entries[1].value);
  EXPECT_EQ("B", b->entries[1].unit);
}

TEST_F(PerfDataParserTest, ThreeMetricsWithAllFields) {
  parsers::perfdata::parse(b, "a=1g;0;4;2;5 b=2g;3;4;2;5 c=3g;1;2;0;10");
  ASSERT_EQ(3, b->entries.size());
  EXPECT_EQ("a", b->entries[0].alias);
  EXPECT_EQ("b", b->entries[1].alias);
  EXPECT_EQ("c", b->entries[2].alias);
  EXPECT_DOUBLE_EQ(1.0, b->entries[0].value);
  EXPECT_DOUBLE_EQ(2.0, b->entries[1].value);
  EXPECT_DOUBLE_EQ(3.0, b->entries[2].value);
}

TEST_F(PerfDataParserTest, MultipleMetricsWithQuotedAliases) {
  parsers::perfdata::parse(b, "'disk c'=50% 'disk d'=75%");
  ASSERT_EQ(2, b->entries.size());
  EXPECT_EQ("disk c", b->entries[0].alias);
  EXPECT_EQ("disk d", b->entries[1].alias);
}

// ==============================================================
// parse — leading/trailing whitespace
// ==============================================================

TEST_F(PerfDataParserTest, LeadingSpaces) {
  parsers::perfdata::parse(b, "   load=1");
  ASSERT_EQ(1, b->entries.size());
  EXPECT_DOUBLE_EQ(1.0, b->entries[0].value);
}

TEST_F(PerfDataParserTest, TrailingSpaces) {
  parsers::perfdata::parse(b, "load=1   ");
  ASSERT_EQ(1, b->entries.size());
  EXPECT_DOUBLE_EQ(1.0, b->entries[0].value);
}

TEST_F(PerfDataParserTest, LeadingAndTrailingSpaces) {
  parsers::perfdata::parse(b, "    'aaa'=1g;0;4;2;5     ");
  ASSERT_EQ(1, b->entries.size());
  EXPECT_EQ("aaa", b->entries[0].alias);
  EXPECT_DOUBLE_EQ(1.0, b->entries[0].value);
}

TEST_F(PerfDataParserTest, ExtraSpacesBetweenMetrics) {
  parsers::perfdata::parse(b, "a=1   b=2   c=3");
  ASSERT_EQ(3, b->entries.size());
  EXPECT_DOUBLE_EQ(1.0, b->entries[0].value);
  EXPECT_DOUBLE_EQ(2.0, b->entries[1].value);
  EXPECT_DOUBLE_EQ(3.0, b->entries[2].value);
}

// ==============================================================
// parse — decimal and fractional values
// ==============================================================

TEST_F(PerfDataParserTest, FractionalValue) {
  parsers::perfdata::parse(b, "load=1.23");
  ASSERT_EQ(1, b->entries.size());
  EXPECT_DOUBLE_EQ(1.23, b->entries[0].value);
}

TEST_F(PerfDataParserTest, FractionalWarningCritical) {
  parsers::perfdata::parse(b, "cpu=50.5%;60.1;90.9");
  ASSERT_EQ(1, b->entries.size());
  EXPECT_DOUBLE_EQ(50.5, b->entries[0].value);
  EXPECT_DOUBLE_EQ(60.1, b->entries[0].warning);
  EXPECT_DOUBLE_EQ(90.9, b->entries[0].critical);
}

TEST_F(PerfDataParserTest, NegativeValue) {
  parsers::perfdata::parse(b, "temp=-5");
  ASSERT_EQ(1, b->entries.size());
  EXPECT_DOUBLE_EQ(-5.0, b->entries[0].value);
}

TEST_F(PerfDataParserTest, ZeroValue) {
  parsers::perfdata::parse(b, "count=0");
  ASSERT_EQ(1, b->entries.size());
  EXPECT_DOUBLE_EQ(0.0, b->entries[0].value);
}

TEST_F(PerfDataParserTest, VerySmallValue) {
  parsers::perfdata::parse(b, "tiny=0.00001");
  ASSERT_EQ(1, b->entries.size());
  EXPECT_DOUBLE_EQ(0.00001, b->entries[0].value);
}

// ==============================================================
// parse — units
// ==============================================================

TEST_F(PerfDataParserTest, MillisecondsUnit) {
  parsers::perfdata::parse(b, "time=100ms");
  ASSERT_EQ(1, b->entries.size());
  EXPECT_EQ("ms", b->entries[0].unit);
}

TEST_F(PerfDataParserTest, SecondsUnit) {
  parsers::perfdata::parse(b, "uptime=3600s");
  ASSERT_EQ(1, b->entries.size());
  EXPECT_EQ("s", b->entries[0].unit);
}

TEST_F(PerfDataParserTest, NoUnit) {
  parsers::perfdata::parse(b, "count=42");
  ASSERT_EQ(1, b->entries.size());
  EXPECT_EQ("", b->entries[0].unit);
}

// ==============================================================
// parse — empty optional fields
// ==============================================================

TEST_F(PerfDataParserTest, EmptyWarningField) {
  parsers::perfdata::parse(b, "x=10;;90");
  ASSERT_EQ(1, b->entries.size());
  EXPECT_FALSE(b->entries[0].has_warning);
  EXPECT_TRUE(b->entries[0].has_critical);
  EXPECT_DOUBLE_EQ(90.0, b->entries[0].critical);
}

TEST_F(PerfDataParserTest, EmptyCriticalField) {
  parsers::perfdata::parse(b, "x=10;50;;0;100");
  ASSERT_EQ(1, b->entries.size());
  EXPECT_TRUE(b->entries[0].has_warning);
  EXPECT_FALSE(b->entries[0].has_critical);
  EXPECT_TRUE(b->entries[0].has_minimum);
  EXPECT_TRUE(b->entries[0].has_maximum);
}

TEST_F(PerfDataParserTest, EmptyMinMaxFields) {
  parsers::perfdata::parse(b, "x=10;50;90;;");
  ASSERT_EQ(1, b->entries.size());
  EXPECT_TRUE(b->entries[0].has_warning);
  EXPECT_TRUE(b->entries[0].has_critical);
  EXPECT_FALSE(b->entries[0].has_minimum);
  EXPECT_FALSE(b->entries[0].has_maximum);
}

// ==============================================================
// parse — value with no numeric part
// ==============================================================

TEST_F(PerfDataParserTest, NoNumericValue) {
  parsers::perfdata::parse(b, "status=");
  ASSERT_EQ(1, b->entries.size());
  EXPECT_DOUBLE_EQ(0.0, b->entries[0].value);
}

TEST_F(PerfDataParserTest, ValueIsOnlyUnit) {
  parsers::perfdata::parse(b, "label=KB");
  ASSERT_EQ(1, b->entries.size());
  EXPECT_DOUBLE_EQ(0.0, b->entries[0].value);
}
