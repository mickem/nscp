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
#include <str/wstring.hpp>
#include <string>

// ============================================================================
// Tests for xtos (wide string conversion)
// ============================================================================
TEST(wstring, xtos_int) {
  EXPECT_EQ(strEx::xtos(42), L"42");
  EXPECT_EQ(strEx::xtos(-123), L"-123");
  EXPECT_EQ(strEx::xtos(0), L"0");
}

TEST(wstring, xtos_double) {
  std::wstring result = strEx::xtos(3.14);
  EXPECT_FALSE(result.empty());
  EXPECT_TRUE(result.find(L"3.14") != std::wstring::npos || result.find(L"3,14") != std::wstring::npos);
}

TEST(wstring, xtos_long) {
  EXPECT_EQ(strEx::xtos(1234567890L), L"1234567890");
  EXPECT_EQ(strEx::xtos(-1234567890L), L"-1234567890");
}

TEST(wstring, xtos_unsigned) {
  EXPECT_EQ(strEx::xtos(100u), L"100");
  EXPECT_EQ(strEx::xtos(0u), L"0");
}

// ============================================================================
// Tests for splitEx (wide string split)
// ============================================================================
TEST(wstring, splitEx_basic) {
  std::list<std::wstring> result = strEx::splitEx(L"a,b,c", L",");
  ASSERT_EQ(result.size(), 3u);
  auto it = result.begin();
  EXPECT_EQ(*it++, L"a");
  EXPECT_EQ(*it++, L"b");
  EXPECT_EQ(*it++, L"c");
}

TEST(wstring, splitEx_no_delimiter) {
  std::list<std::wstring> result = strEx::splitEx(L"abc", L",");
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result.front(), L"abc");
}

TEST(wstring, splitEx_empty_string) {
  std::list<std::wstring> result = strEx::splitEx(L"", L",");
  EXPECT_TRUE(result.empty());
}

TEST(wstring, splitEx_consecutive_delimiters) {
  std::list<std::wstring> result = strEx::splitEx(L"a,,c", L",");
  ASSERT_EQ(result.size(), 3u);
  auto it = result.begin();
  EXPECT_EQ(*it++, L"a");
  EXPECT_EQ(*it++, L"");
  EXPECT_EQ(*it++, L"c");
}

TEST(wstring, splitEx_delimiter_at_start) {
  std::list<std::wstring> result = strEx::splitEx(L",a,b", L",");
  ASSERT_EQ(result.size(), 3u);
  auto it = result.begin();
  EXPECT_EQ(*it++, L"");
  EXPECT_EQ(*it++, L"a");
  EXPECT_EQ(*it++, L"b");
}

TEST(wstring, splitEx_delimiter_at_end) {
  std::list<std::wstring> result = strEx::splitEx(L"a,b,", L",");
  ASSERT_EQ(result.size(), 2u);
  auto it = result.begin();
  EXPECT_EQ(*it++, L"a");
  EXPECT_EQ(*it++, L"b");
}

TEST(wstring, splitEx_multi_char_delimiter) {
  std::list<std::wstring> result = strEx::splitEx(L"a::b::c", L"::");
  ASSERT_EQ(result.size(), 3u);
  auto it = result.begin();
  EXPECT_EQ(*it++, L"a");
  EXPECT_EQ(*it++, L":b");  // Note: single char increment in implementation
  EXPECT_EQ(*it++, L":c");
}

TEST(wstring, splitEx_unicode) {
  std::list<std::wstring> result = strEx::splitEx(L"hello,世界,test", L",");
  ASSERT_EQ(result.size(), 3u);
  auto it = result.begin();
  EXPECT_EQ(*it++, L"hello");
  EXPECT_EQ(*it++, L"世界");
  EXPECT_EQ(*it++, L"test");
}

// ============================================================================
// Tests for stox (wide string to type conversion)
// ============================================================================
TEST(wstring, stox_int) {
  EXPECT_EQ(strEx::stox<int>(L"42"), 42);
  EXPECT_EQ(strEx::stox<int>(L"-123"), -123);
  EXPECT_EQ(strEx::stox<int>(L"0"), 0);
}

TEST(wstring, stox_long) {
  EXPECT_EQ(strEx::stox<long>(L"1234567890"), 1234567890L);
  EXPECT_EQ(strEx::stox<long>(L"-1234567890"), -1234567890L);
}

TEST(wstring, stox_double) {
  EXPECT_DOUBLE_EQ(strEx::stox<double>(L"3.14"), 3.14);
  EXPECT_DOUBLE_EQ(strEx::stox<double>(L"-2.5"), -2.5);
}

TEST(wstring, stox_unsigned) {
  EXPECT_EQ(strEx::stox<unsigned int>(L"100"), 100u);
  EXPECT_EQ(strEx::stox<unsigned int>(L"0"), 0u);
}

// ============================================================================
// Tests for replace (wide string replace)
// ============================================================================
TEST(wstring, replace_basic) {
  std::wstring str = L"hello world";
  strEx::replace(str, L"world", L"universe");
  EXPECT_EQ(str, L"hello universe");
}

TEST(wstring, replace_multiple_occurrences) {
  std::wstring str = L"aaa";
  strEx::replace(str, L"a", L"b");
  EXPECT_EQ(str, L"bbb");
}

TEST(wstring, replace_not_found) {
  std::wstring str = L"hello world";
  strEx::replace(str, L"xyz", L"abc");
  EXPECT_EQ(str, L"hello world");
}

TEST(wstring, replace_empty_string) {
  std::wstring str = L"";
  strEx::replace(str, L"a", L"b");
  EXPECT_EQ(str, L"");
}

TEST(wstring, replace_with_empty) {
  std::wstring str = L"hello world";
  strEx::replace(str, L"world", L"");
  EXPECT_EQ(str, L"hello ");
}

TEST(wstring, replace_with_longer) {
  std::wstring str = L"ab";
  strEx::replace(str, L"a", L"aaa");
  EXPECT_EQ(str, L"aaab");
}

TEST(wstring, replace_overlapping) {
  std::wstring str = L"aaa";
  strEx::replace(str, L"aa", L"b");
  EXPECT_EQ(str, L"ba");
}

TEST(wstring, replace_unicode) {
  std::wstring str = L"hello 世界";
  strEx::replace(str, L"世界", L"world");
  EXPECT_EQ(str, L"hello world");
}

TEST(wstring, replace_at_start) {
  std::wstring str = L"abc def";
  strEx::replace(str, L"abc", L"xyz");
  EXPECT_EQ(str, L"xyz def");
}

TEST(wstring, replace_at_end) {
  std::wstring str = L"abc def";
  strEx::replace(str, L"def", L"xyz");
  EXPECT_EQ(str, L"abc xyz");
}
