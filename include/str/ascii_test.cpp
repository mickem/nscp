// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <str/ascii.hpp>

TEST(StrAscii, Digits) {
  for (char c = '0'; c <= '9'; ++c) EXPECT_TRUE(str::ascii::is_digit(c)) << c;
  EXPECT_FALSE(str::ascii::is_digit('/'));
  EXPECT_FALSE(str::ascii::is_digit(':'));
  EXPECT_FALSE(str::ascii::is_digit('a'));
}

TEST(StrAscii, Alpha) {
  EXPECT_TRUE(str::ascii::is_alpha('a'));
  EXPECT_TRUE(str::ascii::is_alpha('Z'));
  EXPECT_FALSE(str::ascii::is_alpha('@'));
  EXPECT_FALSE(str::ascii::is_alpha('['));
  EXPECT_FALSE(str::ascii::is_alpha('`'));
  EXPECT_FALSE(str::ascii::is_alpha('{'));
  EXPECT_FALSE(str::ascii::is_alpha('5'));
}

TEST(StrAscii, Alnum) {
  EXPECT_TRUE(str::ascii::is_alnum('7'));
  EXPECT_TRUE(str::ascii::is_alnum('q'));
  EXPECT_FALSE(str::ascii::is_alnum('-'));
  EXPECT_FALSE(str::ascii::is_alnum('_'));
}

TEST(StrAscii, Graph) {
  EXPECT_TRUE(str::ascii::is_graph('!'));
  EXPECT_TRUE(str::ascii::is_graph('~'));
  EXPECT_FALSE(str::ascii::is_graph(' '));
  EXPECT_FALSE(str::ascii::is_graph('\t'));
  EXPECT_FALSE(str::ascii::is_graph('\x7f'));
}

TEST(StrAscii, HighBytesAreNeverClassified) {
  // A plain char >= 0x80 is negative on most ABIs; it must not be a digit,
  // letter or printable, and must not be undefined behaviour as with <cctype>.
  for (int i = 0x80; i <= 0xff; ++i) {
    const char c = static_cast<char>(i);
    EXPECT_FALSE(str::ascii::is_digit(c)) << i;
    EXPECT_FALSE(str::ascii::is_alpha(c)) << i;
    EXPECT_FALSE(str::ascii::is_graph(c)) << i;
  }
}
