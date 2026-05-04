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

// Issue #669: explicitly-undefined values (per Nagios plugin guidelines) should be
// preserved as the literal "U" rather than silently coerced to 0.
TEST(PerfDataTest, undefined_value_U) {
  EXPECT_EQ("'label'=U", do_parse("label=U;;;;"));
  EXPECT_EQ("'label'=U", do_parse("label=U%;;;;"));
  EXPECT_EQ("'label'=U 'label2'=100%", do_parse("label=U%;;;; label2=100%;;;;"));
}

// Issue #669: only the spec-defined "U" marker should be preserved; longer
// non-numeric tokens like "Unknown" should not be mistaken for it.
TEST(PerfDataTest, undefined_value_U_does_not_match_words_starting_with_U) {
  EXPECT_EQ("'label'=0", do_parse("label=Unknown;;;;"));
  EXPECT_EQ("'label'=0", do_parse("label=Unicorn"));
}

TEST(PerfDataExtractionTest, extract_perf_value_as_string_undefined_U) {
  PB::Commands::QueryResponseMessage::Response::Line r;
  nscapi::protobuf::functions::parse_performance_data(&r, "label=U%;;;;");
  ASSERT_EQ(1, r.perf_size());
  EXPECT_EQ("U", nscapi::protobuf::functions::extract_perf_value_as_string(r.perf(0)));
}

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

// Additional extraction tests for edge cases
TEST(PerfDataExtractionTest, extract_perf_value_as_string_with_G_unit) {
  PB::Common::PerformanceData perf;
  perf.mutable_float_value()->set_value(1);
  perf.mutable_float_value()->set_unit("G");

  const auto result = nscapi::protobuf::functions::extract_perf_value_as_string(perf);
  EXPECT_EQ("1073741824", result);  // 1 * 1024^3
}

TEST(PerfDataExtractionTest, extract_perf_value_as_string_with_T_unit) {
  PB::Common::PerformanceData perf;
  perf.mutable_float_value()->set_value(1);
  perf.mutable_float_value()->set_unit("T");

  const auto result = nscapi::protobuf::functions::extract_perf_value_as_string(perf);
  EXPECT_EQ("1099511627776", result);  // 1 * 1024^4
}

TEST(PerfDataExtractionTest, extract_perf_value_as_int_with_G_unit) {
  PB::Common::PerformanceData perf;
  perf.mutable_float_value()->set_value(2);
  perf.mutable_float_value()->set_unit("G");

  const auto result = nscapi::protobuf::functions::extract_perf_value_as_int(perf);
  EXPECT_EQ(2147483648LL, result);  // 2 * 1024^3
}

TEST(PerfDataExtractionTest, extract_perf_value_as_int_with_T_unit) {
  PB::Common::PerformanceData perf;
  perf.mutable_float_value()->set_value(1);
  perf.mutable_float_value()->set_unit("T");

  const auto result = nscapi::protobuf::functions::extract_perf_value_as_int(perf);
  EXPECT_EQ(1099511627776LL, result);  // 1 * 1024^4
}

TEST(PerfDataExtractionTest, extract_perf_value_as_int_no_value) {
  const PB::Common::PerformanceData perf;

  const auto result = nscapi::protobuf::functions::extract_perf_value_as_int(perf);
  EXPECT_EQ(0, result);
}

TEST(PerfDataExtractionTest, extract_perf_maximum_as_string_string_value) {
  PB::Common::PerformanceData perf;
  perf.mutable_string_value()->set_value("test");

  const auto result = nscapi::protobuf::functions::extract_perf_maximum_as_string(perf);
  EXPECT_EQ("unknown", result);
}

TEST(PerfDataExtractionTest, extract_perf_value_as_string_empty_unit) {
  PB::Common::PerformanceData perf;
  perf.mutable_float_value()->set_value(50);
  perf.mutable_float_value()->set_unit("");

  const auto result = nscapi::protobuf::functions::extract_perf_value_as_string(perf);
  EXPECT_EQ("50", result);
}

TEST(PerfDataExtractionTest, extract_perf_value_as_int_empty_unit) {
  PB::Common::PerformanceData perf;
  perf.mutable_float_value()->set_value(123.9);
  perf.mutable_float_value()->set_unit("");

  const auto result = nscapi::protobuf::functions::extract_perf_value_as_int(perf);
  EXPECT_EQ(123, result);
}

TEST(PerfDataExtractionTest, extract_perf_maximum_as_string_with_G_unit) {
  PB::Common::PerformanceData perf;
  perf.mutable_float_value()->mutable_maximum()->set_value(2);
  perf.mutable_float_value()->set_unit("G");

  const auto result = nscapi::protobuf::functions::extract_perf_maximum_as_string(perf);
  EXPECT_EQ("2147483648", result);  // 2 * 1024^3
}

TEST(PerfDataExtractionTest, extract_perf_maximum_as_string_with_T_unit) {
  PB::Common::PerformanceData perf;
  perf.mutable_float_value()->mutable_maximum()->set_value(1);
  perf.mutable_float_value()->set_unit("T");

  const auto result = nscapi::protobuf::functions::extract_perf_maximum_as_string(perf);
  EXPECT_EQ("1099511627776", result);  // 1 * 1024^4
}

// Performance data parsing edge cases
TEST(PerfDataTest, parse_only_alias_no_value) {
  // Invalid perf data format - should handle gracefully
  EXPECT_EQ("'aaa'=0", do_parse("aaa"));
}

TEST(PerfDataTest, parse_string_value) {
  // String values in perf data
  EXPECT_EQ("'aaa'=0", do_parse("'aaa'=\"string_value\""));
}

TEST(PerfDataTest, decimal_only_value) { EXPECT_EQ("'aaa'=0.5", do_parse("aaa=0.5")); }

TEST(PerfDataTest, very_small_value) { EXPECT_EQ("'aaa'=0.00001", do_parse("aaa=0.00001")); }

TEST(PerfDataTest, zero_value) { EXPECT_EQ("'aaa'=0", do_parse("aaa=0")); }

TEST(PerfDataTest, percent_unit) { EXPECT_EQ("'aaa'=50%", do_parse("aaa=50%")); }

TEST(PerfDataTest, bytes_unit) { EXPECT_EQ("'aaa'=1024B", do_parse("aaa=1024B")); }

TEST(PerfDataTest, milliseconds_unit) { EXPECT_EQ("'aaa'=100ms", do_parse("aaa=100ms")); }

TEST(PerfDataTest, seconds_unit) { EXPECT_EQ("'aaa'=5s", do_parse("aaa=5s")); }

TEST(PerfDataTest, alias_with_spaces) { EXPECT_EQ("'disk usage'=50%", do_parse("'disk usage'=50%")); }

TEST(PerfDataTest, alias_with_special_chars) { EXPECT_EQ("'cpu/total'=75%", do_parse("'cpu/total'=75%")); }

// Build performance data tests
TEST(PerfDataBuildTest, build_empty_line) {
  PB::Commands::QueryResponseMessage::Response::Line line;

  const auto result = nscapi::protobuf::functions::build_performance_data(line, nscapi::protobuf::functions::no_truncation);
  EXPECT_EQ("", result);
}

TEST(PerfDataBuildTest, build_single_perf) {
  PB::Commands::QueryResponseMessage::Response::Line line;
  auto* perf = line.add_perf();
  perf->set_alias("test");
  perf->mutable_float_value()->set_value(42);

  const auto result = nscapi::protobuf::functions::build_performance_data(line, nscapi::protobuf::functions::no_truncation);
  EXPECT_EQ("'test'=42", result);
}

TEST(PerfDataBuildTest, build_perf_with_all_thresholds) {
  PB::Commands::QueryResponseMessage::Response::Line line;
  auto* perf = line.add_perf();
  perf->set_alias("metric");
  perf->mutable_float_value()->set_value(50);
  perf->mutable_float_value()->set_unit("%");
  perf->mutable_float_value()->mutable_warning()->set_value(70);
  perf->mutable_float_value()->mutable_critical()->set_value(90);
  perf->mutable_float_value()->mutable_minimum()->set_value(0);
  perf->mutable_float_value()->mutable_maximum()->set_value(100);

  const auto result = nscapi::protobuf::functions::build_performance_data(line, nscapi::protobuf::functions::no_truncation);
  EXPECT_EQ("'metric'=50%;70;90;0;100", result);
}

TEST(PerfDataBuildTest, build_multiple_perfs) {
  PB::Commands::QueryResponseMessage::Response::Line line;
  auto* perf1 = line.add_perf();
  perf1->set_alias("cpu");
  perf1->mutable_float_value()->set_value(50);
  auto* perf2 = line.add_perf();
  perf2->set_alias("mem");
  perf2->mutable_float_value()->set_value(75);

  const auto result = nscapi::protobuf::functions::build_performance_data(line, nscapi::protobuf::functions::no_truncation);
  EXPECT_EQ("'cpu'=50 'mem'=75", result);
}

TEST(PerfDataBuildTest, build_with_truncation_exact) {
  PB::Commands::QueryResponseMessage::Response::Line line;
  auto* perf = line.add_perf();
  perf->set_alias("test");
  perf->mutable_float_value()->set_value(1);

  const auto result = nscapi::protobuf::functions::build_performance_data(line, 10);  // "'test'=1" is 8 chars
  EXPECT_EQ("'test'=1", result);
}

TEST(PerfDataBuildTest, build_with_no_string_value) {
  // When perf has alias but no value type set
  PB::Commands::QueryResponseMessage::Response::Line line;
  auto* perf = line.add_perf();
  perf->set_alias("test");
  // No value set

  const auto result = nscapi::protobuf::functions::build_performance_data(line, nscapi::protobuf::functions::no_truncation);
  EXPECT_EQ("'test'=", result);
}
