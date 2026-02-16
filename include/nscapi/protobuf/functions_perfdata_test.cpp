/*
 * Copyright (C) 2004-2016 Michael Medin
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

#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/functions_perfdata.hpp>
#include <str/format.hpp>
#include <string>
#include <vector>

std::string do_parse(const std::string& str, const std::size_t max_length = nscapi::protobuf::functions::no_truncation) {
  PB::Commands::QueryResponseMessage::Response::Line r;
  nscapi::protobuf::functions::parse_performance_data(&r, str);
  return nscapi::protobuf::functions::build_performance_data(r, max_length);
}

TEST(PerfDataTest, fractions) {
  EXPECT_EQ("'aaa'=1.23374g;0.12345;4.47538;2.23747;5.94849", do_parse("aaa=1.2337399999999999999g;0.123456;4.4753845;2.2374742;5.9484945"));
}

TEST(PerfDataTest, empty_string) { EXPECT_EQ("", do_parse("")); }

TEST(PerfDataTest, full_string) { EXPECT_EQ("'aaa'=1g;0;4;2;5", do_parse("aaa=1g;0;4;2;5")); }
TEST(PerfDataTest, full_string_with_ticks) { EXPECT_EQ("'aaa'=1g;0;4;2;5", do_parse("aaa=1g;0;4;2;5")); }
TEST(PerfDataTest, full_spaces) { EXPECT_EQ("'aaa'=1g;0;4;2;5", do_parse("     'aaa'=1g;0;4;2;5     ")); }
TEST(PerfDataTest, mltiple_strings) { EXPECT_EQ("'aaa'=1g;0;4;2;5 'bbb'=2g;3;4;2;5", do_parse("aaa=1g;0;4;2;5 bbb=2g;3;4;2;5")); }
TEST(PerfDataTest, only_value) { EXPECT_EQ("'aaa'=1g", do_parse("aaa=1g")); }
TEST(PerfDataTest, multiple_only_values) { EXPECT_EQ("'aaa'=1g 'bbb'=2g 'ccc'=3g", do_parse("aaa=1g bbb=2g 'ccc'=3g")); }
TEST(PerfDataTest, multiple_only_values_no_units) { EXPECT_EQ("'aaa'=1 'bbb'=2 'ccc'=3", do_parse("aaa=1 'bbb'=2 ccc=3")); }
TEST(PerfDataTest, value_with_warncrit) { EXPECT_EQ("'aaa'=1g;0;5", do_parse("aaa=1g;0;5")); }
TEST(PerfDataTest, value_without_warncrit_with_maxmin) { EXPECT_EQ("'aaa'=1g;;;0;5", do_parse("aaa=1g;;;0;5")); }
TEST(PerfDataTest, value_without_warncrit_maxmin) { EXPECT_EQ("'aaa'=1g", do_parse("aaa=1g")); }
TEST(PerfDataTest, leading_space) {
  EXPECT_EQ("'aaa'=1g", do_parse(" aaa=1g"));
  EXPECT_EQ("'aaa'=1g", do_parse("                   aaa=1g"));
}
TEST(PerfDataTest, leading_spaces) {
  EXPECT_EQ("'aaa'=1g", do_parse("     aaa=1g"));
  EXPECT_EQ("'aaa'=1g", do_parse("                   aaa=1g"));
}
TEST(PerfDataTest, negative_vvalues) { EXPECT_EQ("'aaa'=-1g;-0;-4;-2;-5 'bbb'=2g;-3;4;-2;5", do_parse("aaa=-1g;-0;-4;-2;-5 bbb=2g;-3;4;-2;5")); }
TEST(PerfDataTest, value_without_long_uom) { EXPECT_EQ("'aaa'=1ggggg;;;0;5", do_parse("aaa=1ggggg;;;0;5")); }
TEST(PerfDataTest, value_without____uom) { EXPECT_EQ("'aaa'=1gg__gg;;;0;5", do_parse("aaa=1gg__gg;;;0;5")); }

TEST(PerfDataTest, float_value) { EXPECT_EQ("'aaa'=0gig;;;0;5", do_parse("aaa=0.00gig;;;0;5")); }
TEST(PerfDataTest, float_value_rounding_1) { EXPECT_EQ("'aaa'=1.01g;1.02;1.03;1.04;1.05", do_parse("aaa=1.01g;1.02;1.03;1.04;1.05")); }
TEST(PerfDataTest, float_value_rounding_2) {
#if (_MSC_VER == 1700)
  EXPECT_EQ("'aaa'=1.0001g;1.02;1.03;1.04;1.05", do_parse("aaa=1.0001g;1.02;1.03;1.04;1.05"));
#else
  EXPECT_EQ("'aaa'=1.00009g;1.02;1.03;1.04;1.05", do_parse("aaa=1.0001g;1.02;1.03;1.04;1.05"));
#endif
}

TEST(PerfDataTest, problem_701_001) { EXPECT_EQ("'TotalGetRequests__Total'=0requests/s;;;0", do_parse("'TotalGetRequests__Total'=0.00requests/s;;;0;")); }
// 'TotalGetRequests__Total'=0.00requests/s;;;0;
TEST(PerfDataTest, value_various_reparse) {
  std::vector<std::string> strings;
  strings.emplace_back("'aaa'=1g;0;4;2;5");
  strings.emplace_back("'aaa'=6g;1;2;3;4");
  strings.emplace_back("'aaa'=6g;;2;3;4");
  strings.emplace_back("'aaa'=6g;1;;3;4");
  strings.emplace_back("'aaa'=6g;1;2;;4");
  strings.emplace_back("'aaa'=6g;1;2;3");
  strings.emplace_back("'aaa'=6g;;;3;4");
  strings.emplace_back("'aaa'=6g;1;;;4");
  strings.emplace_back("'aaa'=6g;1;2");
  strings.emplace_back("'aaa'=6g");
  for (const std::string& s : strings) {
    EXPECT_EQ(s.c_str(), do_parse(s));
  }
}

TEST(PerfDataTest, truncation) {
  const std::string s = "'abcdefghijklmnopqrstuv'=1g;0;4;2;5 'abcdefghijklmnopqrstuv'=1g;0;4;2;5";
  EXPECT_EQ(s.c_str(), do_parse(s, -1));
  EXPECT_EQ(s.c_str(), do_parse(s, 71));
  EXPECT_EQ("'abcdefghijklmnopqrstuv'=1g;0;4;2;5", do_parse(s, 70));
  EXPECT_EQ("'abcdefghijklmnopqrstuv'=1g;0;4;2;5", do_parse(s, 35));
  EXPECT_EQ("", do_parse(s, 34));
}

TEST(PerfDataTest, unit_conversion_b) {
  const auto d = str::format::convert_to_byte_units(1234567890, "B");
  ASSERT_DOUBLE_EQ(1234567890, d);
}
TEST(PerfDataTest, unit_conversion_k) {
  const auto d = str::format::convert_to_byte_units(1234567890, "K");
  ASSERT_DOUBLE_EQ(1205632.705078125, d);
}
TEST(PerfDataTest, unit_conversion_m) {
  const auto d = str::format::convert_to_byte_units(1234567890, "M");
  ASSERT_DOUBLE_EQ(1177.3756885528564, d);
}
TEST(PerfDataTest, unit_conversion_g) {
  const auto d = str::format::convert_to_byte_units(1234567890, "G");
  ASSERT_DOUBLE_EQ(1.1497809458523989, d);
}

// Unit conversion tests for T (Terabytes)
TEST(PerfDataTest, unit_conversion_t) {
  const auto d = str::format::convert_to_byte_units(1099511627776, "T");
  ASSERT_DOUBLE_EQ(1.0, d);
}

// Performance data extraction tests
TEST(PerfDataExtractionTest, extract_perf_value_as_string_float) {
  PB::Common::PerformanceData perf;
  perf.mutable_float_value()->set_value(42.5);
  perf.mutable_float_value()->set_unit("ms");

  const auto result = nscapi::protobuf::functions::extract_perf_value_as_string(perf);
  EXPECT_EQ("42.5", result);
}

TEST(PerfDataExtractionTest, extract_perf_value_as_string_with_unit) {
  PB::Common::PerformanceData perf;
  perf.mutable_float_value()->set_value(100);
  perf.mutable_float_value()->set_unit("K");

  const auto result = nscapi::protobuf::functions::extract_perf_value_as_string(perf);
  EXPECT_EQ("102400", result);
}

TEST(PerfDataExtractionTest, extract_perf_value_as_int) {
  PB::Common::PerformanceData perf;
  perf.mutable_float_value()->set_value(42.7);

  const auto result = nscapi::protobuf::functions::extract_perf_value_as_int(perf);
  EXPECT_EQ(42, result);
}

TEST(PerfDataExtractionTest, extract_perf_maximum_as_string) {
  PB::Common::PerformanceData perf;
  perf.mutable_float_value()->mutable_maximum()->set_value(100);

  const auto result = nscapi::protobuf::functions::extract_perf_maximum_as_string(perf);
  EXPECT_EQ("100", result);
}

TEST(PerfDataExtractionTest, extract_perf_value_as_string_string_value) {
  PB::Common::PerformanceData perf;
  perf.mutable_string_value()->set_value("test_string");

  const auto result = nscapi::protobuf::functions::extract_perf_value_as_string(perf);
  EXPECT_EQ("test_string", result);
}

TEST(PerfDataExtractionTest, extract_perf_value_as_string_no_value) {
  PB::Common::PerformanceData perf;

  const auto result = nscapi::protobuf::functions::extract_perf_value_as_string(perf);
  EXPECT_EQ("unknown", result);
}

TEST(PerfDataExtractionTest, extract_perf_value_as_int_string_value) {
  PB::Common::PerformanceData perf;
  perf.mutable_string_value()->set_value("test");

  const auto result = nscapi::protobuf::functions::extract_perf_value_as_int(perf);
  EXPECT_EQ(0, result);
}

TEST(PerfDataExtractionTest, extract_perf_value_as_int_with_unit) {
  PB::Common::PerformanceData perf;
  perf.mutable_float_value()->set_value(1);
  perf.mutable_float_value()->set_unit("K");

  const auto result = nscapi::protobuf::functions::extract_perf_value_as_int(perf);
  EXPECT_EQ(1024, result);
}

TEST(PerfDataExtractionTest, extract_perf_maximum_as_string_with_unit) {
  PB::Common::PerformanceData perf;
  perf.mutable_float_value()->mutable_maximum()->set_value(1);
  perf.mutable_float_value()->set_unit("M");

  const auto result = nscapi::protobuf::functions::extract_perf_maximum_as_string(perf);
  EXPECT_EQ("1048576", result);
}

TEST(PerfDataExtractionTest, extract_perf_maximum_as_string_no_float) {
  PB::Common::PerformanceData perf;

  const auto result = nscapi::protobuf::functions::extract_perf_maximum_as_string(perf);
  EXPECT_EQ("unknown", result);
}
