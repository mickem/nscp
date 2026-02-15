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

#include <nscapi/nscapi_protobuf_log.hpp>
#include <nsclient/logger/log_message_factory.hpp>
#include <nsclient/logger/logger_helper.hpp>

// ============================================================================
// Tests for render_log_level_short
// ============================================================================

TEST(logger_helper, render_log_level_short_critical) {
  EXPECT_EQ(nsclient::logging::logger_helper::render_log_level_short(PB::Log::LogEntry_Entry_Level_LOG_CRITICAL), "C");
}

TEST(logger_helper, render_log_level_short_error) {
  EXPECT_EQ(nsclient::logging::logger_helper::render_log_level_short(PB::Log::LogEntry_Entry_Level_LOG_ERROR), "E");
}

TEST(logger_helper, render_log_level_short_warning) {
  EXPECT_EQ(nsclient::logging::logger_helper::render_log_level_short(PB::Log::LogEntry_Entry_Level_LOG_WARNING), "W");
}

TEST(logger_helper, render_log_level_short_info) {
  EXPECT_EQ(nsclient::logging::logger_helper::render_log_level_short(PB::Log::LogEntry_Entry_Level_LOG_INFO), "L");
}

TEST(logger_helper, render_log_level_short_debug) {
  EXPECT_EQ(nsclient::logging::logger_helper::render_log_level_short(PB::Log::LogEntry_Entry_Level_LOG_DEBUG), "D");
}

TEST(logger_helper, render_log_level_short_trace) {
  EXPECT_EQ(nsclient::logging::logger_helper::render_log_level_short(PB::Log::LogEntry_Entry_Level_LOG_TRACE), "T");
}

// ============================================================================
// Tests for render_log_level_long
// ============================================================================

TEST(logger_helper, render_log_level_long_critical) {
  EXPECT_EQ(nsclient::logging::logger_helper::render_log_level_long(PB::Log::LogEntry_Entry_Level_LOG_CRITICAL), "critical");
}

TEST(logger_helper, render_log_level_long_error) {
  EXPECT_EQ(nsclient::logging::logger_helper::render_log_level_long(PB::Log::LogEntry_Entry_Level_LOG_ERROR), "error");
}

TEST(logger_helper, render_log_level_long_warning) {
  EXPECT_EQ(nsclient::logging::logger_helper::render_log_level_long(PB::Log::LogEntry_Entry_Level_LOG_WARNING), "warning");
}

TEST(logger_helper, render_log_level_long_info) {
  EXPECT_EQ(nsclient::logging::logger_helper::render_log_level_long(PB::Log::LogEntry_Entry_Level_LOG_INFO), "info");
}

TEST(logger_helper, render_log_level_long_debug) {
  EXPECT_EQ(nsclient::logging::logger_helper::render_log_level_long(PB::Log::LogEntry_Entry_Level_LOG_DEBUG), "debug");
}

TEST(logger_helper, render_log_level_long_trace) {
  EXPECT_EQ(nsclient::logging::logger_helper::render_log_level_long(PB::Log::LogEntry_Entry_Level_LOG_TRACE), "trace");
}

// ============================================================================
// Tests for render_console_message
// ============================================================================

TEST(logger_helper, render_console_message_basic) {
  std::string data = nsclient::logging::log_message_factory::create_info("test_module", "test.cpp", 42, "Test message");
  auto result = nsclient::logging::logger_helper::render_console_message(false, data);

  EXPECT_FALSE(result.first);  // not an error
  EXPECT_EQ(result.second, "L est_module Test message\n");
}

TEST(logger_helper, render_console_message_oneline) {
  std::string data = nsclient::logging::log_message_factory::create_info("test_module", "test.cpp", 42, "Test message");
  auto result = nsclient::logging::logger_helper::render_console_message(true, data);

  EXPECT_FALSE(result.first);  // not an error
  EXPECT_EQ(result.second, "test.cpp(42): info: Test message\n");
}

TEST(logger_helper, render_console_message_error_level) {
  std::string data = nsclient::logging::log_message_factory::create_error("test_module", "test.cpp", 42, "Error message");
  auto result = nsclient::logging::logger_helper::render_console_message(false, data);

  EXPECT_FALSE(result.first);
  EXPECT_EQ(result.second, "E est_module Error message\n                    test.cpp:42\n");
}

TEST(logger_helper, render_console_message_multiline) {
  std::string data = nsclient::logging::log_message_factory::create_info("module", "file.cpp", 100, "line1\nline2\nline3");
  auto result = nsclient::logging::logger_helper::render_console_message(false, data);

  EXPECT_FALSE(result.first);
  EXPECT_EQ(result.second, "L     module line1\nline2\nline3\n");
}

TEST(logger_helper, render_console_message_invalid_data) {
  std::string invalid_data = "this is not a valid protobuf message";
  auto result = nsclient::logging::logger_helper::render_console_message(false, invalid_data);

  EXPECT_TRUE(result.first);  // should be an error
  EXPECT_EQ(result.second, "ERROR");
}

TEST(logger_helper, render_console_message_empty_data) {
  std::string empty_data;
  auto result = nsclient::logging::logger_helper::render_console_message(false, empty_data);

  // Empty data should fail to parse
  EXPECT_TRUE(result.first);
  EXPECT_EQ(result.second, "ERROR");
}

TEST(logger_helper, render_console_message_all_levels_oneline) {
  // Test all log levels in oneline mode
  std::vector<std::pair<std::string, std::string>> test_cases = {
      {nsclient::logging::log_message_factory::create_critical("mod", "f.cpp", 1, "msg"), "critical"},
      {nsclient::logging::log_message_factory::create_error("mod", "f.cpp", 1, "msg"), "error"},
      {nsclient::logging::log_message_factory::create_warning("mod", "f.cpp", 1, "msg"), "warning"},
      {nsclient::logging::log_message_factory::create_info("mod", "f.cpp", 1, "msg"), "info"},
      {nsclient::logging::log_message_factory::create_debug("mod", "f.cpp", 1, "msg"), "debug"},
      {nsclient::logging::log_message_factory::create_trace("mod", "f.cpp", 1, "msg"), "trace"},
  };

  for (const auto& test_case : test_cases) {
    auto result = nsclient::logging::logger_helper::render_console_message(true, test_case.first);
    EXPECT_FALSE(result.first);
    EXPECT_NE(result.second.find(test_case.second), std::string::npos) << "Expected level '" << test_case.second << "' in output: " << result.second;
  }
}

// ============================================================================
// Tests for get_formated_date
// ============================================================================

TEST(logger_helper, get_formated_date_basic) {
  std::string result = nsclient::logging::logger_helper::get_formated_date("%Y-%m-%d");
  EXPECT_FALSE(result.empty());
  // Should contain 4-digit year followed by dash
  EXPECT_NE(result.find('-'), std::string::npos);
}

TEST(logger_helper, get_formated_date_with_time) {
  std::string result = nsclient::logging::logger_helper::get_formated_date("%Y-%m-%d %H:%M:%S");
  EXPECT_FALSE(result.empty());
  // Should contain colons for time
  EXPECT_NE(result.find(':'), std::string::npos);
}

TEST(logger_helper, get_formated_date_year_only) {
  std::string result = nsclient::logging::logger_helper::get_formated_date("%Y");
  EXPECT_FALSE(result.empty());
  EXPECT_EQ(result.length(), 4u);  // Just the year
}

TEST(logger_helper, get_formated_date_empty_format) {
  std::string result = nsclient::logging::logger_helper::get_formated_date("");
  // Empty format should return empty or default behavior
  // The exact behavior depends on implementation
}
