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

#include <handle.hpp>

namespace {

// ============================================================================
// A simple mock handle type and its closer
// ============================================================================
struct MockCloser {
  static int close_count;
  static void close(int* h) {
    ++close_count;
    delete h;
  }
};
int MockCloser::close_count = 0;

using mock_handle = hlp::handle<int*, MockCloser>;

}  // namespace

// ============================================================================
// Construction / destruction
// ============================================================================

TEST(handle, default_ctor_is_null) {
  mock_handle h;
  EXPECT_EQ(h.get(), nullptr);
  EXPECT_FALSE(static_cast<bool>(h));
}

TEST(handle, value_ctor_stores_handle) {
  int* raw = new int(42);
  MockCloser::close_count = 0;
  {
    mock_handle h(raw);
    EXPECT_EQ(h.get(), raw);
    EXPECT_TRUE(static_cast<bool>(h));
  }
  // Destructor should have closed once
  EXPECT_EQ(MockCloser::close_count, 1);
}

TEST(handle, destructor_closes_handle) {
  MockCloser::close_count = 0;
  {
    mock_handle h(new int(1));
    EXPECT_EQ(MockCloser::close_count, 0);
  }
  EXPECT_EQ(MockCloser::close_count, 1);
}

TEST(handle, destructor_does_not_close_null_handle) {
  MockCloser::close_count = 0;
  { mock_handle h; }
  EXPECT_EQ(MockCloser::close_count, 0);
}

// ============================================================================
// close()
// ============================================================================

TEST(handle, close_releases_handle) {
  MockCloser::close_count = 0;
  mock_handle h(new int(5));
  EXPECT_NE(h.get(), nullptr);
  h.close();
  EXPECT_EQ(h.get(), nullptr);
  EXPECT_EQ(MockCloser::close_count, 1);
}

TEST(handle, close_twice_only_closes_once) {
  MockCloser::close_count = 0;
  mock_handle h(new int(7));
  h.close();
  EXPECT_EQ(MockCloser::close_count, 1);
  h.close();  // no-op: handle is already null
  EXPECT_EQ(MockCloser::close_count, 1);
}

// ============================================================================
// set()
// ============================================================================

TEST(handle, set_closes_old_and_stores_new) {
  MockCloser::close_count = 0;
  int* raw1 = new int(10);
  int* raw2 = new int(20);
  mock_handle h(raw1);
  h.set(raw2);
  EXPECT_EQ(MockCloser::close_count, 1);  // raw1 was closed
  EXPECT_EQ(h.get(), raw2);
  // Destructor will close raw2
}

TEST(handle, set_on_null_handle_stores_new) {
  MockCloser::close_count = 0;
  int* raw = new int(99);
  mock_handle h;
  h.set(raw);
  EXPECT_EQ(MockCloser::close_count, 0);  // nothing was closed
  EXPECT_EQ(h.get(), raw);
}

// ============================================================================
// ref()
// ============================================================================

TEST(handle, ref_returns_address_of_internal_handle) {
  mock_handle h;
  int** pptr = h.ref();
  int* raw = new int(77);
  *pptr = raw;
  EXPECT_EQ(h.get(), raw);
  // clean up via close to avoid leak
  MockCloser::close_count = 0;
  h.close();
  EXPECT_EQ(MockCloser::close_count, 1);
}

// ============================================================================
// detach()
// ============================================================================

TEST(handle, detach_returns_raw_pointer_and_nulls_handle) {
  MockCloser::close_count = 0;
  int* raw = new int(55);
  mock_handle h(raw);
  int* detached = h.detach();
  EXPECT_EQ(detached, raw);
  EXPECT_EQ(h.get(), nullptr);
  // Destructor must NOT close the detached handle
  // Clean up manually to avoid leak
  delete detached;
  EXPECT_EQ(MockCloser::close_count, 0);
}

// ============================================================================
// Implicit conversion operators
// ============================================================================

TEST(handle, implicit_conversion_to_handle_type) {
  int* raw = new int(3);
  mock_handle h(raw);
  int* converted = static_cast<int*>(h);
  EXPECT_EQ(converted, raw);
}

TEST(handle, implicit_bool_true_when_non_null) {
  mock_handle h(new int(1));
  EXPECT_TRUE(static_cast<bool>(h));
}

TEST(handle, implicit_bool_false_when_null) {
  mock_handle h;
  EXPECT_FALSE(static_cast<bool>(h));
}

// ============================================================================
// Assignment from raw pointer
// ============================================================================

TEST(handle, assign_from_raw_pointer_closes_old) {
  MockCloser::close_count = 0;
  int* raw1 = new int(11);
  int* raw2 = new int(22);
  mock_handle h(raw1);
  h = raw2;
  EXPECT_EQ(MockCloser::close_count, 1);  // raw1 was closed
  EXPECT_EQ(h.get(), raw2);
}

// ============================================================================
// Assignment from another handle (move ownership)
// ============================================================================

TEST(handle, assign_from_other_handle_transfers_ownership) {
  MockCloser::close_count = 0;
  int* raw = new int(33);
  mock_handle src(raw);
  mock_handle dst;
  dst = src;  // src.detach() is called inside
  EXPECT_EQ(dst.get(), raw);
  EXPECT_EQ(src.get(), nullptr);
  // Destructor of src must not close anything
  EXPECT_EQ(MockCloser::close_count, 0);
}
