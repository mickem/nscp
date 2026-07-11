// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_uncpath.hpp"

#include <gtest/gtest.h>

// nscapi::plugin_singleton is defined once in check_disk_io_test.cpp for the
// merged check_disk_test target.

using uncpath_check::unc_obj;

TEST(UncPath, DerivedSpaceValues) {
  unc_obj o;
  o.path = "\\\\srv\\share";
  o.size = 100000;
  o.free = 40000;
  o.user_free = 35000;
  EXPECT_EQ(o.get_path(), "\\\\srv\\share");
  EXPECT_EQ(o.get_size(), 100000);
  EXPECT_EQ(o.get_free(), 40000);
  EXPECT_EQ(o.get_used(), 60000);
  EXPECT_EQ(o.get_user_free(), 35000);
  EXPECT_EQ(o.get_free_pct(), 40);
  EXPECT_EQ(o.get_used_pct(), 60);
}

TEST(UncPath, PctGuardsZeroSize) {
  unc_obj o;  // size 0 -> no divide by zero
  EXPECT_EQ(o.get_free_pct(), 0);
  EXPECT_EQ(o.get_used_pct(), 0);
}

TEST(UncPath, DefaultsNotOk) {
  unc_obj o;
  EXPECT_FALSE(o.ok);
  EXPECT_EQ(o.show(), "");
}
