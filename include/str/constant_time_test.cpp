#include "constant_time.hpp"

#include <gtest/gtest.h>

#include <string>

TEST(ConstantTimeEq, EqualStrings) { EXPECT_TRUE(str::constant_time_eq("abc", "abc")); }

TEST(ConstantTimeEq, FirstByteDifferent) { EXPECT_FALSE(str::constant_time_eq("abc", "xbc")); }

TEST(ConstantTimeEq, LastByteDifferent) { EXPECT_FALSE(str::constant_time_eq("abc", "abx")); }

TEST(ConstantTimeEq, LengthMismatchShorter) { EXPECT_FALSE(str::constant_time_eq("abc", "ab")); }

TEST(ConstantTimeEq, LengthMismatchLonger) { EXPECT_FALSE(str::constant_time_eq("ab", "abc")); }

TEST(ConstantTimeEq, EmptyEqualsEmpty) { EXPECT_TRUE(str::constant_time_eq("", "")); }

TEST(ConstantTimeEq, EmptyVsNonEmpty) {
  EXPECT_FALSE(str::constant_time_eq("", "x"));
  EXPECT_FALSE(str::constant_time_eq("x", ""));
}

TEST(ConstantTimeEq, EmbeddedNulHandled) {
  std::string a("a\0b", 3);
  std::string b("a\0b", 3);
  std::string c("a\0c", 3);
  EXPECT_TRUE(str::constant_time_eq(a, b));
  EXPECT_FALSE(str::constant_time_eq(a, c));
}
