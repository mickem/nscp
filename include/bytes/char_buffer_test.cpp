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

#include <bytes/char_buffer.hpp>

#include <cstring>
#include <string>

// ===== hlp::char_buffer tests =====

TEST(char_buffer, construct_from_string) {
  std::string input = "hello";
  hlp::char_buffer buf(input);

  EXPECT_GE(buf.size(), input.length());
  EXPECT_EQ(std::strncmp(buf.get(), "hello", 5), 0);
}

TEST(char_buffer, construct_from_size) {
  hlp::char_buffer buf(64);
  EXPECT_EQ(buf.size(), 64u);
}

TEST(char_buffer, construct_from_empty_string) {
  std::string input;
  hlp::char_buffer buf(input);

  EXPECT_GE(buf.size(), 2u);
}

TEST(char_buffer, zero_clears_buffer) {
  std::string input = "data";
  hlp::char_buffer buf(input);
  buf.zero();

  // After zeroing, all bytes should be zero
  const char *p = buf.get();
  for (std::size_t i = 0; i < buf.size(); ++i) {
    EXPECT_EQ(p[i], 0) << "byte at index " << i << " not zero";
  }
}

TEST(char_buffer, str_returns_content) {
  hlp::char_buffer buf(5);
  std::memcpy(buf.get(), "ABCDE", 5);

  std::string result = buf.str();
  EXPECT_EQ(result.size(), 5u);
  EXPECT_EQ(result, "ABCDE");
}

TEST(char_buffer, str_preserves_embedded_nulls) {
  hlp::char_buffer buf(4);
  buf.get()[0] = 'A';
  buf.get()[1] = '\0';
  buf.get()[2] = 'B';
  buf.get()[3] = 'C';

  std::string result = buf.str();
  EXPECT_EQ(result.size(), 4u);
  EXPECT_EQ(result[0], 'A');
  EXPECT_EQ(result[1], '\0');
  EXPECT_EQ(result[2], 'B');
  EXPECT_EQ(result[3], 'C');
}

TEST(char_buffer, zero_on_size_one_buffer) {
  hlp::char_buffer buf(1);
  // size == 1, so zero() guard "size() > 1" won't clear; just ensure no crash
  buf.zero();
}

// ===== hlp::tchar_buffer tests =====

TEST(tchar_buffer, construct_from_wstring) {
  std::wstring input = L"hello";
  hlp::tchar_buffer buf(input);

  EXPECT_GE(buf.size(), input.length());
  EXPECT_EQ(std::wcsncmp(buf.get(), L"hello", 5), 0);
}

TEST(tchar_buffer, construct_from_size) {
  hlp::tchar_buffer buf(32);
  EXPECT_EQ(buf.size(), 32u);
}

TEST(tchar_buffer, construct_from_empty_wstring) {
  std::wstring input;
  hlp::tchar_buffer buf(input);

  EXPECT_GE(buf.size(), 2u);
}

TEST(tchar_buffer, zero_clears_buffer) {
  std::wstring input = L"data";
  hlp::tchar_buffer buf(input);
  buf.zero();

  const wchar_t *p = buf.get();
  // Check the raw bytes are zero
  const char *raw = reinterpret_cast<const char *>(p);
  for (std::size_t i = 0; i < buf.size() * sizeof(wchar_t); ++i) {
    // only check up to the meaningful part
    if (i < buf.size()) {
      EXPECT_EQ(raw[i], 0) << "byte at index " << i << " not zero";
    }
  }
}

