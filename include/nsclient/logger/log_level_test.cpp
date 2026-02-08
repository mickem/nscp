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

#include <nsclient/logger/log_level.hpp>

// ============================================================================
// Tests for log_level default behavior
// ============================================================================

TEST(log_level, default_level_is_info) {
  const nsclient::logging::log_level level;
  EXPECT_EQ(level.get(), "message");  // info returns "message"
}

TEST(log_level, default_should_info) {
  const nsclient::logging::log_level level;
  EXPECT_TRUE(level.should_info());
  EXPECT_TRUE(level.should_warning());
  EXPECT_TRUE(level.should_error());
  EXPECT_TRUE(level.should_critical());
  EXPECT_FALSE(level.should_debug());
  EXPECT_FALSE(level.should_trace());
}

// ============================================================================
// Tests for set() with various input formats
// ============================================================================

TEST(log_level, set_critical_full) {
  nsclient::logging::log_level level;
  EXPECT_TRUE(level.set("critical"));
  EXPECT_EQ(level.get(), "critical");
  EXPECT_TRUE(level.should_critical());
  EXPECT_FALSE(level.should_error());
  EXPECT_FALSE(level.should_warning());
  EXPECT_FALSE(level.should_info());
  EXPECT_FALSE(level.should_debug());
  EXPECT_FALSE(level.should_trace());
}

TEST(log_level, set_critical_short) {
  nsclient::logging::log_level level;
  EXPECT_TRUE(level.set("crit"));
  EXPECT_EQ(level.get(), "critical");
}

TEST(log_level, set_critical_single_char) {
  nsclient::logging::log_level level;
  EXPECT_TRUE(level.set("c"));
  EXPECT_EQ(level.get(), "critical");
}

TEST(log_level, set_error_full) {
  nsclient::logging::log_level level;
  EXPECT_TRUE(level.set("error"));
  EXPECT_EQ(level.get(), "error");
  EXPECT_TRUE(level.should_critical());
  EXPECT_TRUE(level.should_error());
  EXPECT_FALSE(level.should_warning());
  EXPECT_FALSE(level.should_info());
  EXPECT_FALSE(level.should_debug());
  EXPECT_FALSE(level.should_trace());
}

TEST(log_level, set_error_short) {
  nsclient::logging::log_level level;
  EXPECT_TRUE(level.set("err"));
  EXPECT_EQ(level.get(), "error");
}

TEST(log_level, set_error_single_char) {
  nsclient::logging::log_level level;
  EXPECT_TRUE(level.set("e"));
  EXPECT_EQ(level.get(), "error");
}

TEST(log_level, set_warning_full) {
  nsclient::logging::log_level level;
  EXPECT_TRUE(level.set("warning"));
  EXPECT_EQ(level.get(), "warning");
  EXPECT_TRUE(level.should_critical());
  EXPECT_TRUE(level.should_error());
  EXPECT_TRUE(level.should_warning());
  EXPECT_FALSE(level.should_info());
  EXPECT_FALSE(level.should_debug());
  EXPECT_FALSE(level.should_trace());
}

TEST(log_level, set_warning_short) {
  nsclient::logging::log_level level;
  EXPECT_TRUE(level.set("warn"));
  EXPECT_EQ(level.get(), "warning");
}

TEST(log_level, set_warning_single_char) {
  nsclient::logging::log_level level;
  EXPECT_TRUE(level.set("w"));
  EXPECT_EQ(level.get(), "warning");
}

TEST(log_level, set_info_full) {
  nsclient::logging::log_level level;
  EXPECT_TRUE(level.set("info"));
  EXPECT_EQ(level.get(), "message");  // info returns "message"
  EXPECT_TRUE(level.should_critical());
  EXPECT_TRUE(level.should_error());
  EXPECT_TRUE(level.should_warning());
  EXPECT_TRUE(level.should_info());
  EXPECT_FALSE(level.should_debug());
  EXPECT_FALSE(level.should_trace());
}

TEST(log_level, set_info_log) {
  nsclient::logging::log_level level;
  EXPECT_TRUE(level.set("log"));
  EXPECT_EQ(level.get(), "message");
}

TEST(log_level, set_info_single_char) {
  nsclient::logging::log_level level;
  EXPECT_TRUE(level.set("i"));
  EXPECT_EQ(level.get(), "message");
}

TEST(log_level, set_debug_full) {
  nsclient::logging::log_level level;
  EXPECT_TRUE(level.set("debug"));
  EXPECT_EQ(level.get(), "debug");
  EXPECT_TRUE(level.should_critical());
  EXPECT_TRUE(level.should_error());
  EXPECT_TRUE(level.should_warning());
  EXPECT_TRUE(level.should_info());
  EXPECT_TRUE(level.should_debug());
  EXPECT_FALSE(level.should_trace());
}

TEST(log_level, set_debug_single_char) {
  nsclient::logging::log_level level;
  EXPECT_TRUE(level.set("d"));
  EXPECT_EQ(level.get(), "debug");
}

TEST(log_level, set_trace_full) {
  nsclient::logging::log_level level;
  EXPECT_TRUE(level.set("trace"));
  EXPECT_EQ(level.get(), "trace");
  EXPECT_TRUE(level.should_critical());
  EXPECT_TRUE(level.should_error());
  EXPECT_TRUE(level.should_warning());
  EXPECT_TRUE(level.should_info());
  EXPECT_TRUE(level.should_debug());
  EXPECT_TRUE(level.should_trace());
}

TEST(log_level, set_trace_single_char) {
  nsclient::logging::log_level level;
  EXPECT_TRUE(level.set("t"));
  EXPECT_EQ(level.get(), "trace");
}

// ============================================================================
// Tests for case insensitivity
// ============================================================================

TEST(log_level, set_case_insensitive_upper) {
  nsclient::logging::log_level level;
  EXPECT_TRUE(level.set("CRITICAL"));
  EXPECT_EQ(level.get(), "critical");
}

TEST(log_level, set_case_insensitive_mixed) {
  nsclient::logging::log_level level;
  EXPECT_TRUE(level.set("Debug"));
  EXPECT_EQ(level.get(), "debug");
}

TEST(log_level, set_case_insensitive_single_char_upper) {
  nsclient::logging::log_level level;
  EXPECT_TRUE(level.set("E"));
  EXPECT_EQ(level.get(), "error");
}

// ============================================================================
// Tests for invalid input
// ============================================================================

TEST(log_level, set_invalid_returns_false) {
  nsclient::logging::log_level level;
  EXPECT_FALSE(level.set("invalid"));
}

TEST(log_level, set_invalid_does_not_change_level) {
  nsclient::logging::log_level level;
  level.set("debug");
  EXPECT_EQ(level.get(), "debug");
  EXPECT_FALSE(level.set("invalid"));
  EXPECT_EQ(level.get(), "debug");  // Level unchanged
}

TEST(log_level, set_empty_returns_false) {
  nsclient::logging::log_level level;
  EXPECT_FALSE(level.set(""));
}

