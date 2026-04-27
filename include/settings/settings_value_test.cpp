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

#include <settings/settings_value.hpp>
#include <string>

using nscapi::settings::settings_value;

// ============================================================================
// Tests for from_int
// ============================================================================

TEST(settings_value, from_int_zero) { EXPECT_EQ(settings_value::from_int(0), "0"); }

TEST(settings_value, from_int_positive) {
  EXPECT_EQ(settings_value::from_int(1), "1");
  EXPECT_EQ(settings_value::from_int(42), "42");
  EXPECT_EQ(settings_value::from_int(9999), "9999");
}

TEST(settings_value, from_int_negative) {
  EXPECT_EQ(settings_value::from_int(-1), "-1");
  EXPECT_EQ(settings_value::from_int(-100), "-100");
}

TEST(settings_value, from_int_max) { EXPECT_EQ(settings_value::from_int(2147483647), "2147483647"); }

TEST(settings_value, from_int_min) { EXPECT_EQ(settings_value::from_int(-2147483648), "-2147483648"); }

// ============================================================================
// Tests for to_int
// ============================================================================

TEST(settings_value, to_int_zero) { EXPECT_EQ(settings_value::to_int("0"), 0); }

TEST(settings_value, to_int_positive) {
  EXPECT_EQ(settings_value::to_int("1"), 1);
  EXPECT_EQ(settings_value::to_int("42"), 42);
  EXPECT_EQ(settings_value::to_int("9999"), 9999);
}

TEST(settings_value, to_int_negative) {
  EXPECT_EQ(settings_value::to_int("-1"), -1);
  EXPECT_EQ(settings_value::to_int("-100"), -100);
}

TEST(settings_value, to_int_invalid_returns_default) {
  EXPECT_EQ(settings_value::to_int("not_a_number"), -1);  // default is -1
  EXPECT_EQ(settings_value::to_int(""), -1);
  EXPECT_EQ(settings_value::to_int("abc"), -1);
}

TEST(settings_value, to_int_custom_default) {
  EXPECT_EQ(settings_value::to_int("not_a_number", 99), 99);
  EXPECT_EQ(settings_value::to_int("", 0), 0);
}

TEST(settings_value, to_int_roundtrip) {
  for (int v : {0, 1, -1, 42, 1000, -9999}) {
    EXPECT_EQ(settings_value::to_int(settings_value::from_int(v)), v);
  }
}

// ============================================================================
// Tests for from_bool
// ============================================================================

TEST(settings_value, from_bool_true) { EXPECT_EQ(settings_value::from_bool(true), "true"); }

TEST(settings_value, from_bool_false) { EXPECT_EQ(settings_value::from_bool(false), "false"); }

// ============================================================================
// Tests for to_bool — truthy values
// ============================================================================

TEST(settings_value, to_bool_true_lowercase) { EXPECT_TRUE(settings_value::to_bool("true")); }

TEST(settings_value, to_bool_true_uppercase) { EXPECT_TRUE(settings_value::to_bool("TRUE")); }

TEST(settings_value, to_bool_true_mixedcase) { EXPECT_TRUE(settings_value::to_bool("True")); }

TEST(settings_value, to_bool_one) { EXPECT_TRUE(settings_value::to_bool("1")); }

TEST(settings_value, to_bool_yes_lowercase) { EXPECT_TRUE(settings_value::to_bool("yes")); }

TEST(settings_value, to_bool_yes_uppercase) { EXPECT_TRUE(settings_value::to_bool("YES")); }

TEST(settings_value, to_bool_yes_mixedcase) { EXPECT_TRUE(settings_value::to_bool("Yes")); }

// ============================================================================
// Tests for to_bool — falsy values
// ============================================================================

TEST(settings_value, to_bool_false_lowercase) { EXPECT_FALSE(settings_value::to_bool("false")); }

TEST(settings_value, to_bool_false_uppercase) { EXPECT_FALSE(settings_value::to_bool("FALSE")); }

TEST(settings_value, to_bool_zero) { EXPECT_FALSE(settings_value::to_bool("0")); }

TEST(settings_value, to_bool_no) { EXPECT_FALSE(settings_value::to_bool("no")); }

TEST(settings_value, to_bool_empty_string) { EXPECT_FALSE(settings_value::to_bool("")); }

TEST(settings_value, to_bool_arbitrary_string) { EXPECT_FALSE(settings_value::to_bool("maybe")); }

TEST(settings_value, to_bool_two) { EXPECT_FALSE(settings_value::to_bool("2")); }

// ============================================================================
// Round-trip: from_bool then to_bool
// ============================================================================

TEST(settings_value, to_bool_roundtrip_true) { EXPECT_TRUE(settings_value::to_bool(settings_value::from_bool(true))); }

TEST(settings_value, to_bool_roundtrip_false) { EXPECT_FALSE(settings_value::to_bool(settings_value::from_bool(false))); }
