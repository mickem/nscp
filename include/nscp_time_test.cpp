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

#include <boost/date_time/posix_time/posix_time.hpp>
#include <nscp_time.hpp>
#include <string>

TEST(nscp_time, normalize_trims_and_lowercases) {
  EXPECT_EQ(nscp_time::normalize(""), "");
  EXPECT_EQ(nscp_time::normalize("UTC"), "utc");
  EXPECT_EQ(nscp_time::normalize("  Local  "), "local");
  EXPECT_EQ(nscp_time::normalize("MST-07"), "mst-07");
}

TEST(nscp_time, is_local_default_and_explicit) {
  EXPECT_TRUE(nscp_time::is_local(""));
  EXPECT_TRUE(nscp_time::is_local("local"));
  EXPECT_TRUE(nscp_time::is_local("LOCAL"));
  EXPECT_TRUE(nscp_time::is_local("  local "));
  EXPECT_FALSE(nscp_time::is_local("utc"));
  EXPECT_FALSE(nscp_time::is_local("MST-07"));
}

TEST(nscp_time, is_utc_accepts_utc_and_gmt) {
  EXPECT_TRUE(nscp_time::is_utc("utc"));
  EXPECT_TRUE(nscp_time::is_utc("UTC"));
  EXPECT_TRUE(nscp_time::is_utc("gmt"));
  EXPECT_FALSE(nscp_time::is_utc("local"));
  EXPECT_FALSE(nscp_time::is_utc(""));
}

TEST(nscp_time, tz_label_basic) {
  EXPECT_EQ(nscp_time::tz_label(""), "local");
  EXPECT_EQ(nscp_time::tz_label("local"), "local");
  EXPECT_EQ(nscp_time::tz_label("LOCAL"), "local");
  EXPECT_EQ(nscp_time::tz_label("utc"), "UTC");
  EXPECT_EQ(nscp_time::tz_label("UTC"), "UTC");
  EXPECT_EQ(nscp_time::tz_label("gmt"), "UTC");
}

TEST(nscp_time, tz_label_named_posix_zone_returns_abbrev) {
  // POSIX zone "MST-07" -> std abbrev "MST"
  EXPECT_EQ(nscp_time::tz_label("MST-07"), "MST");
}

TEST(nscp_time, tz_label_unparseable_returns_marker) {
  // An unparseable string should not throw and should mark the misconfig.
  EXPECT_EQ(nscp_time::tz_label("not-a-tz"), "UTC?");
}

TEST(nscp_time, now_does_not_throw_for_known_zones) {
  EXPECT_NO_THROW({ (void)nscp_time::now(""); });
  EXPECT_NO_THROW({ (void)nscp_time::now("local"); });
  EXPECT_NO_THROW({ (void)nscp_time::now("utc"); });
  EXPECT_NO_THROW({ (void)nscp_time::now("MST-07"); });
}

TEST(nscp_time, now_for_utc_is_close_to_universal_time) {
  // The two should be within a few seconds of each other.
  const auto a = boost::posix_time::second_clock::universal_time();
  const auto b = nscp_time::now("utc");
  const auto delta = (b > a) ? (b - a) : (a - b);
  EXPECT_LT(delta.total_seconds(), 5);
}

TEST(nscp_time, now_for_unparseable_falls_back_to_utc) {
  EXPECT_NO_THROW({ (void)nscp_time::now("not-a-tz"); });
  const auto a = boost::posix_time::second_clock::universal_time();
  const auto b = nscp_time::now("not-a-tz");
  const auto delta = (b > a) ? (b - a) : (a - b);
  EXPECT_LT(delta.total_seconds(), 5);
}
