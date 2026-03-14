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

#include <bytes/buffer.hpp>
#include <cstring>

// --- Construction ---

TEST(buffer, construct_with_size) {
  hlp::buffer<char> buf(10);
  EXPECT_EQ(buf.size(), 10u);
  EXPECT_NE(buf.get(), nullptr);
}

TEST(buffer, construct_with_size_zero) {
  hlp::buffer<char> buf(0);
  EXPECT_EQ(buf.size(), 0u);
}

TEST(buffer, construct_with_data) {
  const char src[] = "hello";
  hlp::buffer<char> buf(5, src);
  EXPECT_EQ(buf.size(), 5u);
  EXPECT_EQ(std::memcmp(buf.get(), src, 5), 0);
}

// --- Copy constructor ---

TEST(buffer, copy_constructor) {
  const char src[] = "abcdef";
  hlp::buffer<char> original(6, src);
  hlp::buffer<char> copy(original);

  EXPECT_EQ(copy.size(), 6u);
  EXPECT_EQ(std::memcmp(copy.get(), src, 6), 0);
  // Ensure deep copy (different pointers)
  EXPECT_NE(copy.get(), original.get());
}

// --- Element access ---

TEST(buffer, subscript_operator) {
  hlp::buffer<char> buf(4);
  buf[0] = 'A';
  buf[1] = 'B';
  buf[2] = 'C';
  buf[3] = '\0';

  EXPECT_EQ(buf[0], 'A');
  EXPECT_EQ(buf[1], 'B');
  EXPECT_EQ(buf[2], 'C');
}

// --- Implicit conversion ---

TEST(buffer, implicit_conversion_to_pointer) {
  hlp::buffer<char> buf(4);
  buf[0] = 'X';
  const char *ptr = buf.get();
  EXPECT_EQ(ptr[0], 'X');
}

// --- size_in_bytes ---

TEST(buffer, size_in_bytes_char) {
  hlp::buffer<char> buf(10);
  EXPECT_EQ(buf.size_in_bytes(), 10u);
}

TEST(buffer, size_in_bytes_int) {
  hlp::buffer<int> buf(5);
  EXPECT_EQ(buf.size_in_bytes(), 5u * sizeof(int));
}

// --- get with offset ---

TEST(buffer, get_with_offset) {
  const char src[] = "ABCDEF";
  hlp::buffer<char> buf(6, src);

  char *p0 = buf.get(0);
  EXPECT_EQ(*p0, 'A');

  char *p3 = buf.get(3);
  EXPECT_EQ(*p3, 'D');
}

// --- get_t ---

TEST(buffer, get_t_reinterpret) {
  hlp::buffer<char> buf(8);
  std::memset(buf.get(), 0, 8);
  buf[0] = 1;

  const unsigned char *uc = buf.get_t<const unsigned char *>(0);
  EXPECT_EQ(uc[0], 1);
}

// --- resize ---

TEST(buffer, resize_changes_size) {
  hlp::buffer<char> buf(10);
  EXPECT_EQ(buf.size(), 10u);

  buf.resize(20);
  EXPECT_EQ(buf.size(), 20u);
}

TEST(buffer, resize_to_smaller) {
  hlp::buffer<char> buf(100);
  buf.resize(5);
  EXPECT_EQ(buf.size(), 5u);
}

// --- Integer type ---

TEST(buffer, integer_buffer) {
  hlp::buffer<int> buf(3);
  buf[0] = 42;
  buf[1] = 99;
  buf[2] = -1;

  EXPECT_EQ(buf[0], 42);
  EXPECT_EQ(buf[1], 99);
  EXPECT_EQ(buf[2], -1);
  EXPECT_EQ(buf.size(), 3u);
}

TEST(buffer, integer_buffer_from_data) {
  int src[] = {10, 20, 30};
  hlp::buffer<int> buf(3, src);

  EXPECT_EQ(buf[0], 10);
  EXPECT_EQ(buf[1], 20);
  EXPECT_EQ(buf[2], 30);
}
