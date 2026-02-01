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

TEST(format, format_date) {
  const boost::posix_time::ptime time(boost::gregorian::date(2002, 3, 4), boost::posix_time::time_duration(5, 6, 7));
  EXPECT_EQ(str::format::format_date(time), "2002-03-04 05:06:07");
}

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
  lst.push_back("a");
  EXPECT_EQ(str::format::join(lst, ", "), "a");
  lst.push_back("b");
  lst.push_back("c");
  EXPECT_EQ(str::format::join(lst, ", "), "a, b, c");
  EXPECT_EQ(str::format::join(lst, "-"), "a-b-c");
}

TEST(format, join_vector) {
  std::vector<std::string> vec;
  EXPECT_EQ(str::format::join(vec, ", "), "");
  vec.push_back("x");
  EXPECT_EQ(str::format::join(vec, ", "), "x");
  vec.push_back("y");
  vec.push_back("z");
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
  EXPECT_EQ(str::format::format_byte_units(1000LL* 1000LL * 1024LL, "K"), "1000000");
  EXPECT_EQ(str::format::format_byte_units(1000LL * 1024LL * 1024LL, "M"), "1000");

  EXPECT_EQ(str::format::format_byte_units(5734563876, "G"), "5.341");
  EXPECT_EQ(str::format::format_byte_units(2348796234726384, "T"), "2136.218");
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
