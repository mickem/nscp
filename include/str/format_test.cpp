// Test epoch conversion
// Test with a known time value
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

#include <list>
#include <str/format.hpp>
#include <string>
#include <vector>

TEST(format, format_byte_units_units) {
  EXPECT_EQ(str::format::format_byte_units(0LL), "0B");
  EXPECT_EQ(str::format::format_byte_units(1LL), "1B");
  EXPECT_EQ(str::format::format_byte_units(1024LL), "1KB");
  EXPECT_EQ(str::format::format_byte_units(1024 * 1024LL), "1MB");
  EXPECT_EQ(str::format::format_byte_units(1024 * 1024 * 1024LL), "1GB");
  EXPECT_EQ(str::format::format_byte_units(1024 * 1024 * 1024 * 1024LL), "1TB");
  EXPECT_EQ(str::format::format_byte_units(-76100000000LL), "-70.874GB");
  EXPECT_EQ(str::format::format_byte_units(9223372036854775807LL), "8EB");
  EXPECT_EQ(str::format::format_byte_units(-1LL), "-1B");
  EXPECT_EQ(str::format::format_byte_units(-1024LL), "-1KB");
  EXPECT_EQ(str::format::format_byte_units(-1024 * 1024LL), "-1MB");
  EXPECT_EQ(str::format::format_byte_units(-1024 * 1024 * 1024LL), "-1GB");
  EXPECT_EQ(str::format::format_byte_units(-1024LL * 1024LL * 1024LL * 1024LL), "-1TB");

  EXPECT_EQ(str::format::format_byte_units(0ULL), "0B");
  EXPECT_EQ(str::format::format_byte_units(1ULL), "1B");
  EXPECT_EQ(str::format::format_byte_units(1024ULL), "1KB");
  EXPECT_EQ(str::format::format_byte_units(1024 * 1024ULL), "1MB");
  EXPECT_EQ(str::format::format_byte_units(1024 * 1024 * 1024ULL), "1GB");
  EXPECT_EQ(str::format::format_byte_units(1024 * 1024 * 1024 * 1024ULL), "1TB");
  EXPECT_EQ(str::format::format_byte_units(9223372036854775807ULL), "8EB");
  EXPECT_EQ(str::format::format_byte_units(1024ULL * 1024ULL * 1024ULL * 1024ULL), "1TB");

  EXPECT_EQ(str::format::format_byte_units(13347635876348ULL), "12.14TB");
}

TEST(format, format_byte_units_common) {
  EXPECT_EQ(str::format::format_byte_units(512LL), "512B");
  EXPECT_EQ(str::format::format_byte_units(999LL), "999B");
}

TEST(format, format_byte_units_rounding) {
  EXPECT_EQ(str::format::format_byte_units(0LL), "0B");
  EXPECT_EQ(str::format::format_byte_units(1000LL), "0.977KB");
  EXPECT_EQ(str::format::format_byte_units(1023LL), "0.999KB");
  EXPECT_EQ(str::format::format_byte_units(1024LL), "1KB");
  EXPECT_EQ(str::format::format_byte_units(1126LL), "1.1KB");
  EXPECT_EQ(str::format::format_byte_units(1136LL), "1.109KB");
}

TEST(format, itos_as_time) {
  EXPECT_EQ(str::format::itos_as_time(12345), "12s");
  EXPECT_EQ(str::format::itos_as_time(1234512), "0:20");
  EXPECT_EQ(str::format::itos_as_time(123451234), "1d 10:17");
  EXPECT_EQ(str::format::itos_as_time(1234512345), "2w 0d 06:55");
  EXPECT_EQ(str::format::itos_as_time(12345123456), "20w 2d 21:12");
}

TEST(format, itos_as_time_zero) { EXPECT_EQ(str::format::itos_as_time(0), "0"); }

TEST(format, itos_as_time_sub_second) {
  EXPECT_EQ(str::format::itos_as_time(1), "1");
  EXPECT_EQ(str::format::itos_as_time(500), "500");
  EXPECT_EQ(str::format::itos_as_time(999), "999");
}

TEST(format, itos_as_time_exact_second) {
  // SEC = 1000, time == 1000 is NOT > SEC, falls to else
  EXPECT_EQ(str::format::itos_as_time(1000), "1000");
  EXPECT_EQ(str::format::itos_as_time(1001), "1s");
}

TEST(format, itos_as_time_exact_minute) {
  // MINUTE = 60000, exactly 1 minute
  EXPECT_EQ(str::format::itos_as_time(60001), "0:01");
}

TEST(format, itos_as_time_exact_hour) {
  // HOUR = 3600000, exactly 1 hour
  EXPECT_EQ(str::format::itos_as_time(3600001), "01:00");
}

TEST(format, itos_as_time_exact_day) {
  // DAY = 86400000, exactly 1 day
  EXPECT_EQ(str::format::itos_as_time(86400001), "1d 00:00");
}

TEST(format, itos_as_time_exact_week) {
  // WEEK = 604800000, exactly 1 week
  EXPECT_EQ(str::format::itos_as_time(604800001), "1w 0d 00:00");
}

TEST(format, format_date) {
  const boost::posix_time::ptime time(boost::gregorian::date(2002, 3, 4), boost::posix_time::time_duration(5, 6, 7));
  EXPECT_EQ(str::format::format_date(time), "2002-03-04 05:06:07");
}

TEST(format, format_date_with_custom_format) {
  const boost::posix_time::ptime time(boost::gregorian::date(2020, 12, 25), boost::posix_time::time_duration(14, 30, 0));
  EXPECT_EQ(str::format::format_date(time, "%Y/%m/%d"), "2020/12/25");
}

TEST(format, format_date_from_time_t) {
  // time_t 0 = 1970-01-01 00:00:00 UTC
  EXPECT_EQ(str::format::format_date(static_cast<std::time_t>(0)), "1970-01-01 00:00:00");
}

TEST(format, format_date_from_time_t_custom_format) { EXPECT_EQ(str::format::format_date(static_cast<std::time_t>(0), "%Y"), "1970"); }

// Padding tests
TEST(format, rpad) {
  EXPECT_EQ(str::format::rpad("abc", 5), "  abc");
  EXPECT_EQ(str::format::rpad("abc", 3), "abc");
  EXPECT_EQ(str::format::rpad("abcdef", 3), "def");
  EXPECT_EQ(str::format::rpad("", 3), "   ");
  EXPECT_EQ(str::format::rpad("x", 1), "x");
}

TEST(format, lpad) {
  EXPECT_EQ(str::format::lpad("abc", 5), "abc  ");
  EXPECT_EQ(str::format::lpad("abc", 3), "abc");
  EXPECT_EQ(str::format::lpad("abcdef", 3), "abc");
  EXPECT_EQ(str::format::lpad("", 3), "   ");
  EXPECT_EQ(str::format::lpad("x", 1), "x");
}

// Cleaning tests
TEST(format, strip_ctrl_chars) {
  EXPECT_EQ(str::format::strip_ctrl_chars("hello"), "hello");
  EXPECT_EQ(str::format::strip_ctrl_chars("hello\nworld"), "hello?world");
  EXPECT_EQ(str::format::strip_ctrl_chars("hello\rworld"), "hello?world");
  EXPECT_EQ(str::format::strip_ctrl_chars("hello\tworld"), "hello\tworld");  // tab is not stripped
  std::string with_null = std::string("hel") + '\0' + "lo";
  EXPECT_EQ(str::format::strip_ctrl_chars(with_null), "hel?lo");
  std::string with_bell = std::string("hel") + '\7' + "lo";
  EXPECT_EQ(str::format::strip_ctrl_chars(with_bell), "hel?lo");
  std::string with_del = std::string("hel") + '\x7f' + "lo";
  EXPECT_EQ(str::format::strip_ctrl_chars(with_del), "hel?lo");
  // Vertical tab (11) and form feed (12) are also stripped
  std::string with_vt = std::string("hel") + '\x0b' + "lo";
  EXPECT_EQ(str::format::strip_ctrl_chars(with_vt), "hel?lo");
  std::string with_ff = std::string("hel") + '\x0c' + "lo";
  EXPECT_EQ(str::format::strip_ctrl_chars(with_ff), "hel?lo");
}

TEST(format, format_buffer) {
  EXPECT_EQ(str::format::format_buffer("", 0), "");
  EXPECT_EQ(str::format::format_buffer("abc"), "00000000: 61, 62, 63, abc");
  EXPECT_EQ(str::format::format_buffer("lfhsjdlkfhadslkjfhkldsjfhlkdsajhfauishuxvhc"),
            "00000000: 6c, 66, 68, 73, 6a, 64, 6c, 6b, 66, 68, 61, 64, 73, 6c, 6b, 6a, 66, 68, 6b, 6c, 64, 73, 6a, 66, 68, 6c, 6b, 64, 73, 61, 6a, 68, "
            "lfhsjdlkfhadslkjfhkldsjfhlkdsajh\n"
            "00000020: 66, 61, 75, 69, 73, 68, 75, 78, 76, 68, 63, fauishuxvhc");
}

// Appending tests
TEST(format, append_list) {
  std::string lst;
  str::format::append_list(lst, "first");
  EXPECT_EQ(lst, "first");
  str::format::append_list(lst, "second");
  EXPECT_EQ(lst, "first, second");
  str::format::append_list(lst, "third", " | ");
  EXPECT_EQ(lst, "first, second | third");
  str::format::append_list(lst, "");
  EXPECT_EQ(lst, "first, second | third");
}

TEST(format, join_list) {
  std::list<std::string> lst;
  EXPECT_EQ(str::format::join(lst, ", "), "");
  lst.emplace_back("a");
  EXPECT_EQ(str::format::join(lst, ", "), "a");
  lst.emplace_back("b");
  lst.emplace_back("c");
  EXPECT_EQ(str::format::join(lst, ", "), "a, b, c");
  EXPECT_EQ(str::format::join(lst, "-"), "a-b-c");
}

TEST(format, join_vector) {
  std::vector<std::string> vec;
  EXPECT_EQ(str::format::join(vec, ", "), "");
  vec.emplace_back("x");
  EXPECT_EQ(str::format::join(vec, ", "), "x");
  vec.emplace_back("y");
  vec.emplace_back("z");
  EXPECT_EQ(str::format::join(vec, ", "), "x, y, z");
  EXPECT_EQ(str::format::join(vec, "::"), "x::y::z");
}

// Time decoding tests
TEST(format, decode_time) {
  EXPECT_EQ(str::format::decode_time<int>("10"), 10);
  EXPECT_EQ(str::format::decode_time<int>("10s"), 10);
  EXPECT_EQ(str::format::decode_time<int>("10S"), 10);
  EXPECT_EQ(str::format::decode_time<int>("10m"), 600);
  EXPECT_EQ(str::format::decode_time<int>("10M"), 600);
  EXPECT_EQ(str::format::decode_time<int>("2h"), 7200);
  EXPECT_EQ(str::format::decode_time<int>("2H"), 7200);
  EXPECT_EQ(str::format::decode_time<int>("1d"), 86400);
  EXPECT_EQ(str::format::decode_time<int>("1D"), 86400);
  EXPECT_EQ(str::format::decode_time<int>("1w"), 604800);
  EXPECT_EQ(str::format::decode_time<int>("1W"), 604800);
  EXPECT_EQ(str::format::decode_time<int>("5", 2), 10);
  EXPECT_EQ(str::format::decode_time<int>("5s", 2), 10);
}

TEST(format, stox_as_time_sec) {
  EXPECT_EQ(str::format::stox_as_time_sec<int>("10", "s"), 10);
  EXPECT_EQ(str::format::stox_as_time_sec<int>("10", "m"), 600);
  EXPECT_EQ(str::format::stox_as_time_sec<int>("10s", "m"), 10);
  EXPECT_EQ(str::format::stox_as_time_sec<int>("10m", "s"), 600);
  EXPECT_EQ(str::format::stox_as_time_sec<int>("2h", "s"), 7200);
  EXPECT_EQ(str::format::stox_as_time_sec<int>("1d", "s"), 86400);
  EXPECT_EQ(str::format::stox_as_time_sec<int>("1w", "s"), 604800);
  EXPECT_EQ(str::format::stox_as_time_sec<int>("5", ""), 5);
}

// Issue #589: malformed time specs should be rejected rather than silently
// accepted with the trailing garbage discarded.
TEST(format, decode_time_rejects_garbage) {
  EXPECT_THROW(str::format::decode_time<int>("3000foobar"), std::invalid_argument);
  EXPECT_THROW(str::format::decode_time<int>("3000mfoobar"), std::invalid_argument);
  EXPECT_THROW(str::format::decode_time<int>("foo"), std::invalid_argument);
  EXPECT_THROW(str::format::decode_time<int>(""), std::invalid_argument);
  EXPECT_THROW(str::format::decode_time<int>("10x"), std::invalid_argument);
  EXPECT_THROW(str::format::decode_time<int>("10ss"), std::invalid_argument);
}
TEST(format, stox_as_time_sec_rejects_garbage) {
  EXPECT_THROW(str::format::stox_as_time_sec<int>("3000foobar", "s"), std::invalid_argument);
  EXPECT_THROW(str::format::stox_as_time_sec<int>("3000mfoobar", "s"), std::invalid_argument);
  EXPECT_THROW(str::format::stox_as_time_sec<int>("foo", "s"), std::invalid_argument);
  EXPECT_THROW(str::format::stox_as_time_sec<int>("", "s"), std::invalid_argument);
}

// Pin down whitespace and sign handling. Embedded whitespace, signs, and
// all-whitespace inputs are rejected by validate_time_spec (std::invalid_argument).
// Surrounding whitespace currently passes the validator but then breaks the
// downstream lexical_cast because the parser does not trim - that case is
// asserted as "throws something" so the test still catches a future fix that
// either tightens the validator or trims before parsing.
TEST(format, decode_time_rejects_embedded_whitespace_and_signs) {
  EXPECT_THROW(str::format::decode_time<int>("1 0s"), std::invalid_argument);
  EXPECT_THROW(str::format::decode_time<int>("10 s"), std::invalid_argument);
  EXPECT_THROW(str::format::decode_time<int>("+10s"), std::invalid_argument);
  EXPECT_THROW(str::format::decode_time<int>("-10s"), std::invalid_argument);
  EXPECT_THROW(str::format::decode_time<int>("   "), std::invalid_argument);
  EXPECT_THROW(str::format::decode_time<int>("\t"), std::invalid_argument);
}
TEST(format, decode_time_surrounding_whitespace_throws) {
  EXPECT_THROW(str::format::decode_time<int>(" 10s "), std::exception);
  EXPECT_THROW(str::format::decode_time<int>("\t10\t"), std::exception);
  EXPECT_THROW(str::format::decode_time<int>(" 10"), std::exception);
}
TEST(format, stox_as_time_sec_rejects_embedded_whitespace_and_signs) {
  EXPECT_THROW(str::format::stox_as_time_sec<int>("1 0s", "s"), std::invalid_argument);
  EXPECT_THROW(str::format::stox_as_time_sec<int>("10 s", "s"), std::invalid_argument);
  EXPECT_THROW(str::format::stox_as_time_sec<int>("+10s", "s"), std::invalid_argument);
  EXPECT_THROW(str::format::stox_as_time_sec<int>("-10s", "s"), std::invalid_argument);
  EXPECT_THROW(str::format::stox_as_time_sec<int>("   ", "s"), std::invalid_argument);
  EXPECT_THROW(str::format::stox_as_time_sec<int>("\t", "s"), std::invalid_argument);
}
TEST(format, stox_as_time_sec_surrounding_whitespace_throws) {
  EXPECT_THROW(str::format::stox_as_time_sec<int>(" 10s ", "s"), std::exception);
  EXPECT_THROW(str::format::stox_as_time_sec<int>("\t10m\t", "s"), std::exception);
}

// Byte units decoding tests
TEST(format, decode_byte_units_with_unit) {
  EXPECT_EQ(str::format::decode_byte_units(1LL, "B"), 1LL);
  EXPECT_EQ(str::format::decode_byte_units(1LL, "K"), 1024LL);
  EXPECT_EQ(str::format::decode_byte_units(1LL, "M"), 1024LL * 1024LL);
  EXPECT_EQ(str::format::decode_byte_units(1LL, "G"), 1024LL * 1024LL * 1024LL);
  EXPECT_EQ(str::format::decode_byte_units(1LL, "T"), 1024LL * 1024LL * 1024LL * 1024LL);
  EXPECT_EQ(str::format::decode_byte_units(2LL, "KB"), 2048LL);
  EXPECT_EQ(str::format::decode_byte_units(1LL, ""), 1LL);
}

TEST(format, decode_byte_units_string) {
  EXPECT_EQ(str::format::decode_byte_units("100"), 100LL);
  EXPECT_EQ(str::format::decode_byte_units("100B"), 100LL);
  EXPECT_EQ(str::format::decode_byte_units("1K"), 1024LL);
  EXPECT_EQ(str::format::decode_byte_units("1KB"), 1024LL);
  EXPECT_EQ(str::format::decode_byte_units("1M"), 1024LL * 1024LL);
  EXPECT_EQ(str::format::decode_byte_units("2G"), 2LL * 1024LL * 1024LL * 1024LL);
}

TEST(format, convert_to_byte_units) {
  EXPECT_DOUBLE_EQ(str::format::convert_to_byte_units(1024LL, "B"), 1024.0);
  EXPECT_DOUBLE_EQ(str::format::convert_to_byte_units(1024LL, "K"), 1.0);
  EXPECT_DOUBLE_EQ(str::format::convert_to_byte_units(1024LL * 1024LL, "M"), 1.0);
  EXPECT_DOUBLE_EQ(str::format::convert_to_byte_units(1024LL * 1024LL * 1024LL, "G"), 1.0);
  EXPECT_DOUBLE_EQ(str::format::convert_to_byte_units(2048LL, "KB"), 2.0);
  EXPECT_DOUBLE_EQ(str::format::convert_to_byte_units(1024LL, ""), 1024.0);
}

TEST(format, format_byte_units_with_unit) {
  EXPECT_EQ(str::format::format_byte_units(1024LL, "B"), "1024");
  EXPECT_EQ(str::format::format_byte_units(1000LL, "B"), "1000");
  EXPECT_EQ(str::format::format_byte_units(1024LL, "K"), "1");
  EXPECT_EQ(str::format::format_byte_units(2048LL, "K"), "2");
  EXPECT_EQ(str::format::format_byte_units(1024LL * 1024LL, "M"), "1");
  EXPECT_FALSE(str::format::format_byte_units(1024LL, "").empty());
  EXPECT_EQ(str::format::format_byte_units(1000LL * 1000LL * 1024LL, "K"), "1000000");
  EXPECT_EQ(str::format::format_byte_units(1000LL * 1024LL * 1024LL, "M"), "1000");

  EXPECT_EQ(str::format::format_byte_units(5734563876, "G"), "5.341");
  EXPECT_EQ(str::format::format_byte_units(2348796234726384, "T"), "2136.218");
}

TEST(format, decode_byte_units_lowercase) {
  EXPECT_EQ(str::format::decode_byte_units(1LL, "b"), 1LL);
  EXPECT_EQ(str::format::decode_byte_units(1LL, "k"), 1024LL);
  EXPECT_EQ(str::format::decode_byte_units(1LL, "m"), 1024LL * 1024LL);
  EXPECT_EQ(str::format::decode_byte_units(1LL, "g"), 1024LL * 1024LL * 1024LL);
  EXPECT_EQ(str::format::decode_byte_units(1LL, "t"), 1024LL * 1024LL * 1024LL * 1024LL);
}

TEST(format, format_byte_units_with_empty_unit) {
  // Empty unit: raw double output
  std::string result = str::format::format_byte_units(1024LL, "");
  EXPECT_EQ(result, "1024");
}

TEST(format, format_byte_units_with_unknown_unit) {
  // Unknown unit not in BKMG_RANGE: falls through the loop dividing by 1024 each time
  std::string result = str::format::format_byte_units(0LL, "Z");
  EXPECT_FALSE(result.empty());
}

TEST(format, format_byte_units_zero_with_unit) {
  EXPECT_EQ(str::format::format_byte_units(0LL, "B"), "0");
  EXPECT_EQ(str::format::format_byte_units(0LL, "K"), "0");
  EXPECT_EQ(str::format::format_byte_units(0LL, "M"), "0");
}

TEST(format, convert_to_byte_units_unknown_unit) {
  // Unknown unit: divides through all BKMG_SIZE iterations
  double result = str::format::convert_to_byte_units(1024LL, "Z");
  // After 7 divisions by 1024, result is 1024 / 1024^7
  EXPECT_GT(result, 0.0);
}

TEST(format, decode_time_with_long_type) {
  EXPECT_EQ(str::format::decode_time<long>("60s"), 60L);
  EXPECT_EQ(str::format::decode_time<long>("2m"), 120L);
  EXPECT_EQ(str::format::decode_time<long long>("1h"), 3600LL);
}

TEST(format, stox_as_time_sec_uppercase) {
  EXPECT_EQ(str::format::stox_as_time_sec<int>("10S", "m"), 10);
  EXPECT_EQ(str::format::stox_as_time_sec<int>("10M", "s"), 600);
  EXPECT_EQ(str::format::stox_as_time_sec<int>("2H", "s"), 7200);
  EXPECT_EQ(str::format::stox_as_time_sec<int>("1D", "s"), 86400);
  EXPECT_EQ(str::format::stox_as_time_sec<int>("1W", "s"), 604800);
}

TEST(format, find_proper_unit_BKMG) {
  EXPECT_EQ(str::format::find_proper_unit_BKMG(0ULL), "B");
  EXPECT_EQ(str::format::find_proper_unit_BKMG(100ULL), "B");
  EXPECT_EQ(str::format::find_proper_unit_BKMG(999ULL), "B");
  EXPECT_EQ(str::format::find_proper_unit_BKMG(1024ULL), "KB");
  EXPECT_EQ(str::format::find_proper_unit_BKMG(1024ULL * 1024ULL), "MB");
  EXPECT_EQ(str::format::find_proper_unit_BKMG(1024ULL * 1024ULL * 1024ULL), "GB");
  EXPECT_EQ(str::format::find_proper_unit_BKMG(1024ULL * 1024ULL * 1024ULL * 1024ULL), "TB");
}

#ifdef WIN32
TEST(format, format_time_delta) {
  // Test with a known time value
  time_t test_time = 90061;  // 1 day, 1 hour, 1 minute, 1 second
  std::string result = str::format::format_time_delta(test_time, "%d days %H hours %M minutes %S seconds");
  EXPECT_EQ(result, "1 days 1 hours 1 minutes 1 seconds");

  test_time = 125293;
  result = str::format::format_time_delta(test_time, "%d days %H hours %M minutes %S seconds");
  EXPECT_EQ(result, "1 days 10 hours 48 minutes 13 seconds");
}

TEST(format, filetime_to_time) {
  // Test epoch conversion
  const unsigned long long filetime = str::format::SECS_BETWEEN_EPOCHS * str::format::SECS_TO_100NS;
  EXPECT_EQ(str::format::filetime_to_time(filetime), 0ULL);

  // Test 1 second after epoch
  const unsigned long long filetime_1sec = filetime + str::format::SECS_TO_100NS;
  EXPECT_EQ(str::format::filetime_to_time(filetime_1sec), 1ULL);
}

TEST(format, filetime_to_time_large_value) {
  // 2001-09-09 01:46:40 UTC = 1000000000 seconds since epoch
  const unsigned long long filetime = (str::format::SECS_BETWEEN_EPOCHS + 1000000000ULL) * str::format::SECS_TO_100NS;
  EXPECT_EQ(str::format::filetime_to_time(filetime), 1000000000ULL);
}

TEST(format, format_filetime) {
  EXPECT_EQ(str::format::format_filetime(0), "ZERO");
  const unsigned long long filetime = (str::format::SECS_BETWEEN_EPOCHS + 1000000000ULL) * str::format::SECS_TO_100NS;
  const std::string result = str::format::format_filetime(filetime);
  EXPECT_EQ(result, "2001-09-09 01:46:40");
}

TEST(format, format_filetime_delta) {
  EXPECT_EQ(str::format::format_filetime_delta(0), "ZERO");

  unsigned long long filetime = 10000000ULL * 3600ULL;
  std::string result = str::format::format_filetime_delta(filetime);
  EXPECT_EQ(result, "0-0-0 1:0:0");

  filetime = 234789234792 * 3600ULL;
  result = str::format::format_filetime_delta(filetime);
  EXPECT_EQ(result, "72-8-4 6:55:24");
}
#endif

// --- Additional edge case tests for full coverage ---

// rpad/lpad with zero length
TEST(format, rpad_zero_length) {
  EXPECT_EQ(str::format::rpad("abc", 0), "");
  EXPECT_EQ(str::format::rpad("", 0), "");
}

TEST(format, lpad_zero_length) {
  EXPECT_EQ(str::format::lpad("abc", 0), "");
  EXPECT_EQ(str::format::lpad("", 0), "");
}

TEST(format, stox_as_time_sec_uppercase_default) {
  EXPECT_EQ(str::format::stox_as_time_sec<int>("10", "S"), 10);
  EXPECT_EQ(str::format::stox_as_time_sec<int>("10", "M"), 600);
  EXPECT_EQ(str::format::stox_as_time_sec<int>("10", "H"), 36000);
  EXPECT_EQ(str::format::stox_as_time_sec<int>("10", "D"), 864000);
  EXPECT_EQ(str::format::stox_as_time_sec<int>("10", "W"), 6048000);
}

// stox_as_time_sec with lowercase default units
TEST(format, stox_as_time_sec_lowercase_default) {
  EXPECT_EQ(str::format::stox_as_time_sec<int>("10", "h"), 36000);
  EXPECT_EQ(str::format::stox_as_time_sec<int>("10", "d"), 864000);
  EXPECT_EQ(str::format::stox_as_time_sec<int>("10", "w"), 6048000);
}

// stox_as_time_sec with unknown default unit (falls through to return value)
TEST(format, stox_as_time_sec_unknown_default_unit) {
  EXPECT_EQ(str::format::stox_as_time_sec<int>("10", "x"), 10);
  EXPECT_EQ(str::format::stox_as_time_sec<int>("10", "Z"), 10);
}

// stox_as_time_sec with different template types
TEST(format, stox_as_time_sec_long) {
  EXPECT_EQ(str::format::stox_as_time_sec<long>("60s", "m"), 60L);
  EXPECT_EQ(str::format::stox_as_time_sec<long long>("2h", "s"), 7200LL);
}

// decode_byte_units with unknown unit (fallthrough)
TEST(format, decode_byte_units_unknown_unit) {
  EXPECT_EQ(str::format::decode_byte_units(42LL, "Z"), 42LL);
  EXPECT_EQ(str::format::decode_byte_units(42LL, "x"), 42LL);
}

// decode_byte_units with different template type (int)
TEST(format, decode_byte_units_int_type) {
  EXPECT_EQ(str::format::decode_byte_units(1, "K"), 1024);
  EXPECT_EQ(str::format::decode_byte_units(2, "M"), 2 * 1024 * 1024);
}

// decode_byte_units(string) exception paths
TEST(format, decode_byte_units_string_throws_on_empty) { EXPECT_THROW(str::format::decode_byte_units(""), boost::bad_lexical_cast); }

TEST(format, decode_byte_units_string_throws_on_no_digits) { EXPECT_THROW(str::format::decode_byte_units("KB"), boost::bad_lexical_cast); }

// format_byte_units(long long) with small negative values
TEST(format, format_byte_units_negative_small) {
  EXPECT_EQ(str::format::format_byte_units(-512LL), "-512B");
  EXPECT_EQ(str::format::format_byte_units(-999LL), "-999B");
  EXPECT_EQ(str::format::format_byte_units(-1000LL), "-0.977KB");
}

// convert_to_byte_units with lowercase unit (tests to_upper_copy path)
TEST(format, convert_to_byte_units_lowercase) {
  EXPECT_DOUBLE_EQ(str::format::convert_to_byte_units(1024LL, "k"), 1.0);
  EXPECT_DOUBLE_EQ(str::format::convert_to_byte_units(1024LL * 1024LL, "m"), 1.0);
  EXPECT_DOUBLE_EQ(str::format::convert_to_byte_units(1024LL * 1024LL * 1024LL, "g"), 1.0);
}

// convert_to_byte_units with T (terabyte)
TEST(format, convert_to_byte_units_terabyte) {
  EXPECT_DOUBLE_EQ(str::format::convert_to_byte_units(1024LL * 1024LL * 1024LL * 1024LL, "T"), 1.0);
  EXPECT_DOUBLE_EQ(str::format::convert_to_byte_units(1024LL * 1024LL * 1024LL * 1024LL, "t"), 1.0);
}

// convert_to_byte_units with int template type
TEST(format, convert_to_byte_units_int_type) {
  EXPECT_DOUBLE_EQ(str::format::convert_to_byte_units(1024, "K"), 1.0);
  EXPECT_DOUBLE_EQ(str::format::convert_to_byte_units(2048, "K"), 2.0);
}

// format_byte_units<T>(value, unit) with non-zero unknown unit (falls through loop)
TEST(format, format_byte_units_with_unit_unknown_nonzero) {
  std::string result = str::format::format_byte_units(1024LL, "Z");
  EXPECT_FALSE(result.empty());
  // After dividing by 1024 seven times: 1024 / 1024^7 → very small number
  double val = std::stod(result);
  EXPECT_GT(val, 0.0);
}

// format_byte_units<T>(value, unit) with T (terabyte) unit
TEST(format, format_byte_units_with_unit_terabyte) { EXPECT_EQ(str::format::format_byte_units(1024LL * 1024LL * 1024LL * 1024LL, "T"), "1"); }

// format_byte_units<T>(value, unit) with P (petabyte) unit
TEST(format, format_byte_units_with_unit_petabyte) { EXPECT_EQ(str::format::format_byte_units(1024LL * 1024LL * 1024LL * 1024LL * 1024LL, "P"), "1"); }

// format_byte_units<T>(value, unit) with int template
TEST(format, format_byte_units_with_unit_int_type) {
  EXPECT_EQ(str::format::format_byte_units(1024, "K"), "1");
  EXPECT_EQ(str::format::format_byte_units(2048, "K"), "2");
}

// find_proper_unit_BKMG with PB and EB
TEST(format, find_proper_unit_BKMG_large) {
  EXPECT_EQ(str::format::find_proper_unit_BKMG(1024ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL), "PB");
  EXPECT_EQ(str::format::find_proper_unit_BKMG(1024ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL), "EB");
}

// find_proper_unit_BKMG with max unsigned long long
TEST(format, find_proper_unit_BKMG_max) {
  std::string result = str::format::find_proper_unit_BKMG(ULLONG_MAX);
  // ULLONG_MAX is ~18.4 EB, should return EB
  EXPECT_EQ(result, "EB");
}

// itos_as_time: verify seconds are discarded in longer formats
TEST(format, itos_as_time_residual_seconds_discarded) {
  // 1 week + 30 seconds (30000ms) — seconds should not appear
  std::string result = str::format::itos_as_time(604800001ULL + 30000ULL);
  EXPECT_EQ(result.find("s"), std::string::npos);  // no "s" suffix
  EXPECT_NE(result.find("w"), std::string::npos);  // has week indicator
}

// itos_as_time: hour boundary with minutes
TEST(format, itos_as_time_hour_with_minutes) {
  // 2 hours 30 minutes = (2*3600 + 30*60) * 1000 + 1
  unsigned long long t = (2ULL * 3600 + 30 * 60) * 1000 + 1;
  EXPECT_EQ(str::format::itos_as_time(t), "02:30");
}

// itos_as_time: day with hours and minutes
TEST(format, itos_as_time_day_with_hours_minutes) {
  // 2 days, 5 hours, 15 minutes
  unsigned long long t = (2ULL * 86400 + 5 * 3600 + 15 * 60) * 1000 + 1;
  EXPECT_EQ(str::format::itos_as_time(t), "2d 05:15");
}

// itos_as_time: week with days, hours and minutes
TEST(format, itos_as_time_week_with_days_hours_minutes) {
  // 3 weeks, 2 days, 10 hours, 45 minutes
  unsigned long long t = (3ULL * 604800 + 2 * 86400 + 10 * 3600 + 45 * 60) * 1000 + 1;
  EXPECT_EQ(str::format::itos_as_time(t), "3w 2d 10:45");
}

// append_list with empty initial list and custom separator
TEST(format, append_list_empty_start_custom_sep) {
  std::string lst;
  str::format::append_list(lst, "only", " | ");
  EXPECT_EQ(lst, "only");
}

// format_byte_units(unsigned long long) fractional values
TEST(format, format_byte_units_ull_fractional) {
  // 1536 bytes = 1.5KB
  EXPECT_EQ(str::format::format_byte_units(1536ULL), "1.5KB");
}

// format_byte_units(long long) fractional values
TEST(format, format_byte_units_ll_fractional) {
  EXPECT_EQ(str::format::format_byte_units(1536LL), "1.5KB");
  EXPECT_EQ(str::format::format_byte_units(-1536LL), "-1.5KB");
}

#ifdef WIN32
// Constants accessibility
TEST(format, constants) {
  EXPECT_EQ(str::format::SECS_BETWEEN_EPOCHS, 11644473600ULL);
  EXPECT_EQ(str::format::SECS_TO_100NS, 10000000ULL);
}
#endif
// calc_pct_round / format_pct (issue #419, #595)
TEST(format, calc_pct_round_basic) {
  EXPECT_EQ(str::format::calc_pct_round(0ULL, 100ULL), 0);
  EXPECT_EQ(str::format::calc_pct_round(50ULL, 100ULL), 50);
  EXPECT_EQ(str::format::calc_pct_round(100ULL, 100ULL), 100);
}

TEST(format, calc_pct_round_handles_zero_total) {
  EXPECT_EQ(str::format::calc_pct_round(0ULL, 0ULL), 0);
  EXPECT_EQ(str::format::calc_pct_round(123ULL, 0ULL), 0);
}

TEST(format, calc_pct_round_rounds_to_nearest) {
  // 58032128 / 7784628224 ~= 0.7454% -> rounds to 1, not truncates to 0 (issue #419)
  EXPECT_EQ(str::format::calc_pct_round(58032128ULL, 7784628224ULL), 1);
  // 4 / 7 = 57.14% -> rounds to 57
  EXPECT_EQ(str::format::calc_pct_round(4LL, 7LL), 57);
  // 5 / 9 = 55.55% -> rounds to 56
  EXPECT_EQ(str::format::calc_pct_round(5LL, 9LL), 56);
}

TEST(format, calc_pct_round_handles_signed_inputs) {
  EXPECT_EQ(str::format::calc_pct_round(-1LL, 100LL), -1);
  EXPECT_EQ(str::format::calc_pct_round(100LL, -1LL), 0);
}

TEST(format, format_pct_default_decimals) {
  EXPECT_EQ(str::format::format_pct(97.337), "97.34");
  EXPECT_EQ(str::format::format_pct(0.0), "0.00");
  EXPECT_EQ(str::format::format_pct(100.0), "100.00");
}

TEST(format, format_pct_custom_decimals) {
  EXPECT_EQ(str::format::format_pct(97.337, 1), "97.3");
  EXPECT_EQ(str::format::format_pct(97.36, 1), "97.4");
  EXPECT_EQ(str::format::format_pct(97.337, 0), "97");
}

TEST(format, format_pct_value_total_overload) {
  EXPECT_EQ(str::format::format_pct(58032128ULL, 7784628224ULL), "0.75");
  EXPECT_EQ(str::format::format_pct(0ULL, 0ULL), "0.00");
}

// Regression: the typical disk filter passes long long pairs (drive_size and
// user_free are long long in CheckDisk::filter_obj). These overloads must be
// callable without ambiguity.
TEST(format, calc_pct_round_signed_overload_disambiguates) {
  long long value = 58032128;
  long long total = 7784628224LL;
  EXPECT_EQ(str::format::calc_pct_round(value, total), 1);
  // Mixed widths (int literal + long long variable) — must still resolve to
  // the signed overload without an ambiguous-call diagnostic.
  EXPECT_EQ(str::format::calc_pct_round(50, total), 0);
}

// A value > total can occur when the metric and the denominator are sampled
// at slightly different moments (e.g. swap reservations briefly exceed
// configured size). The helper should not clamp — we want callers to see
// the real number rather than a silently-wrong 100.
TEST(format, calc_pct_round_does_not_clamp_above_100) {
  EXPECT_EQ(str::format::calc_pct_round(150LL, 100LL), 150);
  EXPECT_EQ(str::format::format_pct(150LL, 100LL), "150.00");
}

// Half-up rounding behaviour. std::llround rounds half away from zero so
// 0.5% must yield 1, not 0.
TEST(format, calc_pct_round_half_up) {
  EXPECT_EQ(str::format::calc_pct_round(1ULL, 200ULL), 1);     // exactly 0.5%
  EXPECT_EQ(str::format::calc_pct_round(3ULL, 200ULL), 2);     // exactly 1.5%
  EXPECT_EQ(str::format::calc_pct_round(995ULL, 1000ULL), 100);
}

// Petabyte-scale inputs still produce the right percentage. Both fit
// comfortably within double's 53-bit mantissa.
TEST(format, calc_pct_round_petabyte_scale) {
  const unsigned long long pb = 1024ULL * 1024 * 1024 * 1024 * 1024;  // 2^50
  EXPECT_EQ(str::format::calc_pct_round(pb / 4, pb), 25);
  EXPECT_EQ(str::format::calc_pct_round(pb, pb), 100);
}

// `nscp settings --sort` will roundtrip percentage values through both
// helpers; format_pct on a long long pair must agree with the human render
// of the same value calculated as a double.
TEST(format, format_pct_signed_overload_matches_double_path) {
  const long long used = 58032128LL;
  const long long total = 7784628224LL;
  EXPECT_EQ(str::format::format_pct(used, total),
            str::format::format_pct(str::format::calc_pct_double(used, total)));
}

// std::fixed always prints the decimal separator; verify no surprise locale
// behaviour leaks in (some locales would render "0,75").
TEST(format, format_pct_uses_dot_decimal_separator) {
  const std::string s = str::format::format_pct(0.755);
  EXPECT_NE(s.find('.'), std::string::npos);
  EXPECT_EQ(s.find(','), std::string::npos);
}

// Negative decimals should not crash; behaviour falls through to whatever
// std::setprecision does (which is well-defined: precision 0).
TEST(format, format_pct_negative_decimals_does_not_crash) {
  EXPECT_NO_THROW((void)str::format::format_pct(97.34, -1));
}
