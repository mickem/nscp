/*
 * Copyright (C) 2004-2026 Michael Medin
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

#include "check_pdh.hpp"

#include <gtest/gtest.h>


// ============================================================================
// filter_obj struct tests
// ============================================================================

TEST(PdhFilterObj, Construction) {
  check_pdh::filter_obj obj("myalias", "\\Processor(_Total)\\% Processor Time", "5m", 42, 42.5);
  EXPECT_EQ(obj.get_alias(), "myalias");
  EXPECT_EQ(obj.get_counter(), "\\Processor(_Total)\\% Processor Time");
  EXPECT_EQ(obj.get_time(), "5m");
  EXPECT_EQ(obj.get_value_i(), 42);
  EXPECT_DOUBLE_EQ(obj.get_value_f(), 42.5);
}

TEST(PdhFilterObj, EmptyTime) {
  check_pdh::filter_obj obj("alias", "\\System\\Threads", "", 100, 100.0);
  EXPECT_EQ(obj.get_time(), "");
  EXPECT_EQ(obj.get_value_i(), 100);
  EXPECT_DOUBLE_EQ(obj.get_value_f(), 100.0);
}

TEST(PdhFilterObj, ZeroValues) {
  check_pdh::filter_obj obj("zero", "counter", "1s", 0, 0.0);
  EXPECT_EQ(obj.get_value_i(), 0);
  EXPECT_DOUBLE_EQ(obj.get_value_f(), 0.0);
}

TEST(PdhFilterObj, NegativeValues) {
  check_pdh::filter_obj obj("neg", "counter", "", -10, -10.5);
  EXPECT_EQ(obj.get_value_i(), -10);
  EXPECT_DOUBLE_EQ(obj.get_value_f(), -10.5);
}

TEST(PdhFilterObj, LargeValues) {
  check_pdh::filter_obj obj("big", "counter", "", 9999999999LL, 9999999999.99);
  EXPECT_EQ(obj.get_value_i(), 9999999999LL);
  EXPECT_DOUBLE_EQ(obj.get_value_f(), 9999999999.99);
}

TEST(PdhFilterObj, ShowFormat) {
  check_pdh::filter_obj obj("myalias", "\\Processor\\% Time", "5m", 42, 42.5);
  std::string s = obj.show();
  // show() returns "counter=value_f (value_i)"
  EXPECT_NE(s.find("\\Processor\\% Time"), std::string::npos);
  EXPECT_NE(s.find("42"), std::string::npos);
}

TEST(PdhFilterObj, DifferentIntAndFloat) {
  // When the int and float values differ (e.g. rate counters)
  check_pdh::filter_obj obj("alias", "counter", "", 10, 10.75);
  EXPECT_EQ(obj.get_value_i(), 10);
  EXPECT_DOUBLE_EQ(obj.get_value_f(), 10.75);
}

// ============================================================================
// counter_config_object tests
// ============================================================================

TEST(PdhCounterConfig, DefaultConstruction) {
  check_pdh::counter_config_object config("myalias", "/settings/test");
  EXPECT_EQ(config.get_alias(), "myalias");
  EXPECT_EQ(config.collection_strategy, "static");
  EXPECT_EQ(config.instances, "auto");
  EXPECT_EQ(config.type, "double");
  EXPECT_EQ(config.counter, "");
}

TEST(PdhCounterConfig, ToString) {
  check_pdh::counter_config_object config("cpu", "/settings/test");
  config.counter = "\\Processor(_Total)\\% Processor Time";
  config.collection_strategy = "rrd";
  config.type = "large";

  std::string s = config.to_string();
  EXPECT_NE(s.find("\\Processor(_Total)\\% Processor Time"), std::string::npos);
  EXPECT_NE(s.find("rrd"), std::string::npos);
  EXPECT_NE(s.find("large"), std::string::npos);
}
