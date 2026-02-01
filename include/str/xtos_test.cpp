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

#include <str/format.hpp>
#include <str/xtos.hpp>
#include <string>

// ============================================================================
// Tests for stox<T>(string s) - string to type conversion
// ============================================================================
TEST(xtos, stox_int) {
  EXPECT_EQ(str::stox<int>("42"), 42);
  EXPECT_EQ(str::stox<int>("-123"), -123);
  EXPECT_EQ(str::stox<int>("0"), 0);
}

TEST(xtos, stox_long) {
  EXPECT_EQ(str::stox<long>("1234567890"), 1234567890L);
  EXPECT_EQ(str::stox<long>("-1234567890"), -1234567890L);
}

TEST(xtos, stox_long_long) {
  EXPECT_EQ(str::stox<long long>("9223372036854775807"), 9223372036854775807LL);
  EXPECT_EQ(str::stox<long long>("-9223372036854775807"), -9223372036854775807LL);
}

TEST(xtos, stox_unsigned) {
  EXPECT_EQ(str::stox<unsigned int>("100"), 100u);
  EXPECT_EQ(str::stox<unsigned int>("0"), 0u);
  EXPECT_EQ(str::stox<unsigned int>("4294967295"), 4294967295u);
}

TEST(xtos, stox_double) {
  EXPECT_DOUBLE_EQ(str::stox<double>("3.14"), 3.14);
  EXPECT_DOUBLE_EQ(str::stox<double>("-2.5"), -2.5);
  EXPECT_DOUBLE_EQ(str::stox<double>("0.0"), 0.0);
}

TEST(xtos, stox_float) {
  EXPECT_FLOAT_EQ(str::stox<float>("3.14"), 3.14f);
  EXPECT_FLOAT_EQ(str::stox<float>("-2.5"), -2.5f);
}

// ============================================================================
// Tests for stox<T>(string s, T def) - string to type with default
// ============================================================================
TEST(xtos, stox_with_default_valid) {
  EXPECT_EQ(str::stox<int>("42", 0), 42);
  EXPECT_EQ(str::stox<int>("-123", 0), -123);
  EXPECT_EQ(str::stox<double>("3.14", 0.0), 3.14);
}

TEST(xtos, stox_with_default_invalid) {
  EXPECT_EQ(str::stox<int>("not_a_number", 99), 99);
  EXPECT_EQ(str::stox<int>("", 42), 42);
  EXPECT_EQ(str::stox<int>("abc123", -1), -1);
}

TEST(xtos, stox_with_default_empty_string) {
  EXPECT_EQ(str::stox<int>("", 100), 100);
  EXPECT_EQ(str::stox<double>("", 3.14), 3.14);
  EXPECT_EQ(str::stox<long>("", 999L), 999L);
}

TEST(xtos, stox_with_default_partial_number) {
  // These should fail and return default since they're not valid numbers
  EXPECT_EQ(str::stox<int>("123abc", 0), 0);
  EXPECT_EQ(str::stox<int>("12.34", 0), 0);  // int can't parse decimals
}

// ============================================================================
// Tests for xtos<T>(T i) - type to string conversion
// ============================================================================
TEST(xtos, xtos_int) {
  EXPECT_EQ(str::xtos(42), "42");
  EXPECT_EQ(str::xtos(-123), "-123");
  EXPECT_EQ(str::xtos(0), "0");
}

TEST(xtos, xtos_long) {
  EXPECT_EQ(str::xtos(1234567890L), "1234567890");
  EXPECT_EQ(str::xtos(-1234567890L), "-1234567890");
}

TEST(xtos, xtos_unsigned) {
  EXPECT_EQ(str::xtos(100u), "100");
  EXPECT_EQ(str::xtos(0u), "0");
}

// ============================================================================
// Tests for ihextos(unsigned int i) - integer to hex string
// ============================================================================
TEST(xtos, ihextos_basic) {
  EXPECT_EQ(str::ihextos(0), "0");
  EXPECT_EQ(str::ihextos(1), "1");
  EXPECT_EQ(str::ihextos(10), "a");
  EXPECT_EQ(str::ihextos(15), "f");
  EXPECT_EQ(str::ihextos(16), "10");
}

TEST(xtos, ihextos_larger_values) {
  EXPECT_EQ(str::ihextos(255), "ff");
  EXPECT_EQ(str::ihextos(256), "100");
  EXPECT_EQ(str::ihextos(4096), "1000");
  EXPECT_EQ(str::ihextos(65535), "ffff");
}

TEST(xtos, ihextos_max_values) {
  EXPECT_EQ(str::ihextos(0xDEADBEEF), "deadbeef");
  EXPECT_EQ(str::ihextos(0xFFFFFFFF), "ffffffff");
  EXPECT_EQ(str::ihextos(0x12345678), "12345678");
}

// ============================================================================
// Tests for xtos_non_sci<T>(T i) - type to string without scientific notation
// ============================================================================

TEST(format, strex_s__xtos_non_sci_int) {
  EXPECT_EQ(str::xtos_non_sci(0LL), "0");
  EXPECT_EQ(str::xtos_non_sci(1000LL), "1000");
  EXPECT_EQ(str::xtos_non_sci(10230000LL), "10230000");
  EXPECT_EQ(str::xtos_non_sci(1024000000000LL), "1024000000000");
  EXPECT_EQ(str::xtos_non_sci(1024000000000000000ULL), "1024000000000000000");
  EXPECT_EQ(str::xtos_non_sci(9223ULL), "9223");
  EXPECT_EQ(str::xtos_non_sci(92233720ULL), "92233720");
  EXPECT_EQ(str::xtos_non_sci(922337203685ULL), "922337203685");
  EXPECT_EQ(str::xtos_non_sci(9223372036854775807ULL), "9223372036854775807");
}
TEST(format, strex_s__xtos_float) {
  EXPECT_EQ(str::xtos(0.0), "0");
  EXPECT_EQ(str::xtos(1000.0), "1000");
#if (_MSC_VER == 1700)
#define ESUF "e+0"
#else
#define ESUF "e+"
#endif
  EXPECT_EQ(str::xtos(10230000.0), "1.023" ESUF "07");
  EXPECT_EQ(str::xtos(1024000000000.0), "1.024" ESUF "12");
  EXPECT_EQ(str::xtos(1024000000000000000.0), "1.024" ESUF "18");
  EXPECT_EQ(str::xtos(9223.0), "9223");
  EXPECT_EQ(str::xtos(92233720.0), "9.22337" ESUF "07");
  EXPECT_EQ(str::xtos(922337203685.0), "9.22337" ESUF "11");
  EXPECT_EQ(str::xtos(9223372036854775807.0), "9.22337" ESUF "18");
}

TEST(format, strex_s__xtos_no_sci_int) {
  EXPECT_EQ(str::xtos_non_sci(0LL), "0");
  EXPECT_EQ(str::xtos_non_sci(1000LL), "1000");
  EXPECT_EQ(str::xtos_non_sci(10230000LL), "10230000");
  EXPECT_EQ(str::xtos_non_sci(1024000000000LL), "1024000000000");
  EXPECT_EQ(str::xtos_non_sci(1024000000000000000ULL), "1024000000000000000");
  EXPECT_EQ(str::xtos_non_sci(9223ULL), "9223");
  EXPECT_EQ(str::xtos_non_sci(92233720ULL), "92233720");
  EXPECT_EQ(str::xtos_non_sci(922337203685ULL), "922337203685");
  EXPECT_EQ(str::xtos_non_sci(9223372036854775807ULL), "9223372036854775807");
}
TEST(format, strex_s__xtos_no_sci_float_0) {
  EXPECT_EQ(str::xtos_non_sci(0.0), "0");
  EXPECT_EQ(str::xtos_non_sci(1000.0), "1000");
  EXPECT_EQ(str::xtos_non_sci(10230000.0), "10230000");
  EXPECT_EQ(str::xtos_non_sci(1024000000000.0), "1024000000000");
  EXPECT_EQ(str::xtos_non_sci(1024000000000000000.0), "1024000000000000000");
  EXPECT_EQ(str::xtos_non_sci(9223.0), "9223");
  EXPECT_EQ(str::xtos_non_sci(92233720.0), "92233720");
  EXPECT_EQ(str::xtos_non_sci(922337203685.0), "922337203685");
#if (_MSC_VER == 1700)
  EXPECT_EQ(str::xtos_non_sci(9223372036854775807.0), "9223372036854775800");
#else
  EXPECT_EQ(str::xtos_non_sci(9223372036854775807.0), "9223372036854775808");
#endif
}
TEST(format, strex_s__xtos_no_sci_float_1) {
  EXPECT_EQ(str::xtos_non_sci(0.339), "0.339");
  EXPECT_EQ(str::xtos_non_sci(1000.344585858585858585858585585), "1000.34458");
  EXPECT_EQ(str::xtos_non_sci(10230000.3333333333333333333333), "10230000.33333");
#if (_MSC_VER == 1700)
  EXPECT_EQ(str::xtos_non_sci(1024000000000.13123123123123), "1024000000000.1312");
#else
  EXPECT_EQ(str::xtos_non_sci(1024000000000.13123123123123), "1024000000000.13122");
#endif
  EXPECT_EQ(str::xtos_non_sci(1024000000000000000.13123123123123), "1024000000000000000");
  EXPECT_EQ(str::xtos_non_sci(9223.13123432423423), "9223.13123");
  EXPECT_EQ(str::xtos_non_sci(92233720.234324234234234), "92233720.23432");
  EXPECT_EQ(str::xtos_non_sci(922337203685.2423423423423), "922337203685.24231");
#if (_MSC_VER == 1700)
  EXPECT_EQ(str::xtos_non_sci(9223372036854775807.98798789879887), "9223372036854775800");
#else
  EXPECT_EQ(str::xtos_non_sci(9223372036854775807.98798789879887), "9223372036854775808");
#endif
}
