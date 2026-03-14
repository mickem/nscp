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

#include <str/utf8.hpp>
#include <string>

// ============================================================================
// Tests for cvt nop specializations
// ============================================================================
TEST(utf8, cvt_string_to_string_nop) {
  std::string input = "hello world";
  std::string result = utf8::cvt<std::string>(input);
  EXPECT_EQ(result, "hello world");
}

TEST(utf8, cvt_string_to_string_empty) {
  std::string input = "";
  std::string result = utf8::cvt<std::string>(input);
  EXPECT_EQ(result, "");
}

TEST(utf8, cvt_wstring_to_wstring_nop) {
  std::wstring input = L"hello world";
  std::wstring result = utf8::cvt<std::wstring>(input);
  EXPECT_EQ(result, L"hello world");
}

TEST(utf8, cvt_wstring_to_wstring_empty) {
  std::wstring input = L"";
  std::wstring result = utf8::cvt<std::wstring>(input);
  EXPECT_EQ(result, L"");
}

// ============================================================================
// Tests for wstring_to_string
// ============================================================================
TEST(utf8, wstring_to_string_ascii) {
  std::wstring input = L"hello";
  std::string result = utf8::wstring_to_string(input);
  EXPECT_EQ(result, "hello");
}

TEST(utf8, wstring_to_string_empty) {
  std::wstring input = L"";
  std::string result = utf8::wstring_to_string(input);
  EXPECT_EQ(result, "");
}

TEST(utf8, wstring_to_string_unicode) {
  // U+00E9 (é) in UTF-8 is 0xC3 0xA9
  std::wstring input = L"\u00E9";
  std::string result = utf8::wstring_to_string(input);
  EXPECT_EQ(result, "\xC3\xA9");
}

TEST(utf8, wstring_to_string_multibyte) {
  // U+4E16 (世) in UTF-8 is 0xE4 0xB8 0x96
  std::wstring input = L"\u4E16";
  std::string result = utf8::wstring_to_string(input);
  EXPECT_EQ(result, "\xE4\xB8\x96");
}

TEST(utf8, wstring_to_string_mixed) {
  std::wstring input = L"hello \u00E9 world";
  std::string result = utf8::wstring_to_string(input);
  EXPECT_EQ(result, "hello \xC3\xA9 world");
}

// ============================================================================
// Tests for string_to_wstring
// ============================================================================
TEST(utf8, string_to_wstring_ascii) {
  std::string input = "hello";
  std::wstring result = utf8::string_to_wstring(input);
  EXPECT_EQ(result, L"hello");
}

TEST(utf8, string_to_wstring_empty) {
  std::string input = "";
  std::wstring result = utf8::string_to_wstring(input);
  EXPECT_EQ(result, L"");
}

TEST(utf8, string_to_wstring_unicode) {
  // UTF-8 for U+00E9 (é) is 0xC3 0xA9
  std::string input = "\xC3\xA9";
  std::wstring result = utf8::string_to_wstring(input);
  EXPECT_EQ(result, L"\u00E9");
}

TEST(utf8, string_to_wstring_multibyte) {
  // UTF-8 for U+4E16 (世) is 0xE4 0xB8 0x96
  std::string input = "\xE4\xB8\x96";
  std::wstring result = utf8::string_to_wstring(input);
  EXPECT_EQ(result, L"\u4E16");
}

TEST(utf8, string_to_wstring_mixed) {
  std::string input = "hello \xC3\xA9 world";
  std::wstring result = utf8::string_to_wstring(input);
  EXPECT_EQ(result, L"hello \u00E9 world");
}

// ============================================================================
// Tests for cvt wstring->string and string->wstring (non-nop specializations)
// ============================================================================
TEST(utf8, cvt_wstring_to_string) {
  std::wstring input = L"test \u00E9";
  std::string result = utf8::cvt<std::string>(input);
  EXPECT_EQ(result, "test \xC3\xA9");
}

TEST(utf8, cvt_string_to_wstring) {
  std::string input = "test \xC3\xA9";
  std::wstring result = utf8::cvt<std::wstring>(input);
  EXPECT_EQ(result, L"test \u00E9");
}

// ============================================================================
// Tests for roundtrip conversion
// ============================================================================
TEST(utf8, roundtrip_string_wstring_string) {
  std::string original = "hello \xC3\xA9\xC3\xB1 world";
  std::wstring wide = utf8::string_to_wstring(original);
  std::string back = utf8::wstring_to_string(wide);
  EXPECT_EQ(back, original);
}

TEST(utf8, roundtrip_wstring_string_wstring) {
  std::wstring original = L"hello \u00E9\u00F1 world";
  std::string narrow = utf8::wstring_to_string(original);
  std::wstring back = utf8::string_to_wstring(narrow);
  EXPECT_EQ(back, original);
}

TEST(utf8, roundtrip_cvt) {
  std::string original = "test 123 \xC3\xA9";
  std::wstring wide = utf8::cvt<std::wstring>(original);
  std::string back = utf8::cvt<std::string>(wide);
  EXPECT_EQ(back, original);
}

// ============================================================================
// Tests for to_system
// ============================================================================
TEST(utf8, to_system_ascii) {
  std::wstring input = L"hello world";
  std::string result = utf8::to_system(input);
  EXPECT_EQ(result, "hello world");
}

TEST(utf8, to_system_empty) {
  std::wstring input = L"";
  std::string result = utf8::to_system(input);
  EXPECT_EQ(result, "");
}

TEST(utf8, to_system_unicode) {
  // On Linux, to_system outputs UTF-8
  std::wstring input = L"\u00E9";
  std::string result = utf8::to_system(input);
#ifndef WIN32
  // On Linux, system encoding is UTF-8
  EXPECT_EQ(result, "\xC3\xA9");
#else
  // On Windows, system encoding depends on locale
  EXPECT_FALSE(result.empty());
#endif
}

// ============================================================================
// Tests for to_unicode
// ============================================================================
TEST(utf8, to_unicode_ascii) {
  std::string input = "hello world";
  std::wstring result = utf8::to_unicode(input);
  EXPECT_EQ(result, L"hello world");
}

TEST(utf8, to_unicode_empty) {
  std::string input = "";
  std::wstring result = utf8::to_unicode(input);
  EXPECT_EQ(result, L"");
}

// ============================================================================
// Tests for from_encoding
// ============================================================================
TEST(utf8, from_encoding_utf8) {
  std::string input = "hello \xC3\xA9";
  std::wstring result = utf8::from_encoding(input, "UTF-8");
  EXPECT_EQ(result, L"hello \u00E9");
}

TEST(utf8, from_encoding_ascii) {
  std::string input = "hello world";
  std::wstring result = utf8::from_encoding(input, "ASCII");
  EXPECT_EQ(result, L"hello world");
}

TEST(utf8, from_encoding_empty_string) {
  std::string input = "";
  std::wstring result = utf8::from_encoding(input, "UTF-8");
  EXPECT_EQ(result, L"");
}

// ============================================================================
// Tests for to_encoding (wstring overload)
// ============================================================================
TEST(utf8, to_encoding_wstring_utf8) {
  std::wstring input = L"hello \u00E9";
  std::string result = utf8::to_encoding(input, "UTF-8");
  EXPECT_EQ(result, "hello \xC3\xA9");
}

TEST(utf8, to_encoding_wstring_ascii) {
  std::wstring input = L"hello world";
  std::string result = utf8::to_encoding(input, "ASCII");
  EXPECT_EQ(result, "hello world");
}

TEST(utf8, to_encoding_wstring_empty) {
  std::wstring input = L"";
  std::string result = utf8::to_encoding(input, "UTF-8");
  EXPECT_EQ(result, "");
}

// ============================================================================
// Tests for to_encoding (string overload)
// ============================================================================
TEST(utf8, to_encoding_string_utf8) {
  std::string input = "hello \xC3\xA9";
  std::string result = utf8::to_encoding(input, "UTF-8");
  EXPECT_EQ(result, "hello \xC3\xA9");
}

TEST(utf8, to_encoding_string_ascii) {
  std::string input = "hello world";
  std::string result = utf8::to_encoding(input, "ASCII");
  EXPECT_EQ(result, "hello world");
}

// ============================================================================
// Tests for utf8_from_native
// ============================================================================
TEST(utf8, utf8_from_native_ascii) {
  std::string input = "hello world";
  std::string result = utf8::utf8_from_native(input);
  EXPECT_EQ(result, "hello world");
}

TEST(utf8, utf8_from_native_empty) {
  std::string input = "";
  std::string result = utf8::utf8_from_native(input);
  EXPECT_EQ(result, "");
}

// ============================================================================
// Tests for encoding roundtrips
// ============================================================================
TEST(utf8, roundtrip_from_to_encoding_utf8) {
  std::string original = "test \xC3\xA9\xC3\xB1";
  std::wstring wide = utf8::from_encoding(original, "UTF-8");
  std::string back = utf8::to_encoding(wide, "UTF-8");
  EXPECT_EQ(back, original);
}

TEST(utf8, roundtrip_to_from_encoding_utf8) {
  std::wstring original = L"test \u00E9\u00F1";
  std::string narrow = utf8::to_encoding(original, "UTF-8");
  std::wstring back = utf8::from_encoding(narrow, "UTF-8");
  EXPECT_EQ(back, original);
}

// ============================================================================
// Tests for special characters and longer strings
// ============================================================================
TEST(utf8, wstring_to_string_numbers_and_symbols) {
  std::wstring input = L"123 !@#$%^&*()";
  std::string result = utf8::wstring_to_string(input);
  EXPECT_EQ(result, "123 !@#$%^&*()");
}

TEST(utf8, string_to_wstring_numbers_and_symbols) {
  std::string input = "123 !@#$%^&*()";
  std::wstring result = utf8::string_to_wstring(input);
  EXPECT_EQ(result, L"123 !@#$%^&*()");
}

TEST(utf8, wstring_to_string_long_string) {
  std::wstring input = L"The quick brown fox jumps over the lazy dog. 0123456789";
  std::string result = utf8::wstring_to_string(input);
  EXPECT_EQ(result, "The quick brown fox jumps over the lazy dog. 0123456789");
}

TEST(utf8, string_to_wstring_long_string) {
  std::string input = "The quick brown fox jumps over the lazy dog. 0123456789";
  std::wstring result = utf8::string_to_wstring(input);
  EXPECT_EQ(result, L"The quick brown fox jumps over the lazy dog. 0123456789");
}

TEST(utf8, wstring_to_string_with_spaces) {
  std::wstring input = L"  spaces  ";
  std::string result = utf8::wstring_to_string(input);
  EXPECT_EQ(result, "  spaces  ");
}

TEST(utf8, string_to_wstring_with_newlines) {
  std::string input = "line1\nline2\nline3";
  std::wstring result = utf8::string_to_wstring(input);
  EXPECT_EQ(result, L"line1\nline2\nline3");
}

TEST(utf8, wstring_to_string_with_tabs) {
  std::wstring input = L"col1\tcol2\tcol3";
  std::string result = utf8::wstring_to_string(input);
  EXPECT_EQ(result, "col1\tcol2\tcol3");
}

