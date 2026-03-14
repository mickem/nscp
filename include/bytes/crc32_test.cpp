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

#include <bytes/crc32.h>

#include <cstring>
#include <string>
#include <vector>

// Well-known CRC-32 value for "123456789" is 0xCBF43926
TEST(crc32, known_check_value) {
  const char *input = "123456789";
  unsigned long crc = calculate_crc32(input, std::strlen(input));
  EXPECT_EQ(crc, 0xCBF43926UL);
}

TEST(crc32, empty_string) {
  const char *input = "";
  unsigned long crc = calculate_crc32(input, 0);
  // CRC-32 of empty data is 0x00000000
  EXPECT_EQ(crc, 0x00000000UL);
}

TEST(crc32, single_byte) {
  const char input = 'A';
  unsigned long crc = calculate_crc32(&input, 1);
  // CRC-32 of "A" is 0xD3D99E8B
  EXPECT_EQ(crc, 0xD3D99E8BUL);
}

TEST(crc32, unsigned_char_overload) {
  const unsigned char input[] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39};  // "123456789"
  unsigned long crc = calculate_crc32(input, sizeof(input));
  EXPECT_EQ(crc, 0xCBF43926UL);
}

TEST(crc32, char_and_unsigned_char_match) {
  const char *str = "Hello, World!";
  size_t len = std::strlen(str);
  unsigned long crc_char = calculate_crc32(str, len);
  unsigned long crc_uchar = calculate_crc32(reinterpret_cast<const unsigned char *>(str), len);
  EXPECT_EQ(crc_char, crc_uchar);
}

TEST(crc32, binary_data_with_zeros) {
  const unsigned char data[] = {0x00, 0x00, 0x00, 0x00};
  unsigned long crc = calculate_crc32(data, sizeof(data));
  // CRC-32 of four zero bytes is 0x2144DF1C
  EXPECT_EQ(crc, 0x2144DF1CUL);
}

TEST(crc32, all_ones) {
  const unsigned char data[] = {0xFF, 0xFF, 0xFF, 0xFF};
  unsigned long crc = calculate_crc32(data, sizeof(data));
  // CRC-32 of four 0xFF bytes is 0xFFFFFFFF
  EXPECT_EQ(crc, 0xFFFFFFFFUL);
}

TEST(crc32, deterministic) {
  const char *input = "test data for crc32";
  unsigned long crc1 = calculate_crc32(input, std::strlen(input));
  unsigned long crc2 = calculate_crc32(input, std::strlen(input));
  EXPECT_EQ(crc1, crc2);
}

TEST(crc32, different_data_different_crc) {
  const char *input1 = "hello";
  const char *input2 = "world";
  unsigned long crc1 = calculate_crc32(input1, std::strlen(input1));
  unsigned long crc2 = calculate_crc32(input2, std::strlen(input2));
  EXPECT_NE(crc1, crc2);
}

TEST(crc32, known_string_hello) {
  const char *input = "hello";
  unsigned long crc = calculate_crc32(input, std::strlen(input));
  // CRC-32 of "hello" is 0x3610A686
  EXPECT_EQ(crc, 0x3610A686UL);
}

TEST(crc32, generate_table_idempotent) {
  // Calling generate_crc32_table() multiple times should not change results
  generate_crc32_table();
  const char *input = "123456789";
  unsigned long crc1 = calculate_crc32(input, std::strlen(input));
  generate_crc32_table();
  unsigned long crc2 = calculate_crc32(input, std::strlen(input));
  EXPECT_EQ(crc1, crc2);
  EXPECT_EQ(crc1, 0xCBF43926UL);
}

