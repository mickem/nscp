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

// ============================================================================
// Tests for log message creation
// ============================================================================

TEST(log_message_factory, create_critical_returns_valid_message) {
  const std::string serialized = nsclient::logging::log_message_factory::create_critical("test_module", "test.cpp", 42, "critical message");
  EXPECT_FALSE(serialized.empty());

  PB::Log::LogEntry entry;
  EXPECT_TRUE(entry.ParseFromString(serialized));
  ASSERT_EQ(entry.entry_size(), 1);

  const auto& msg = entry.entry(0);
  EXPECT_EQ(msg.sender(), "test_module");
  EXPECT_EQ(msg.file(), "test.cpp");
  EXPECT_EQ(msg.line(), 42);
  EXPECT_EQ(msg.message(), "critical message");
  EXPECT_EQ(msg.level(), PB::Log::LogEntry_Entry_Level_LOG_CRITICAL);
}

TEST(log_message_factory, create_error_returns_valid_message) {
  const std::string serialized = nsclient::logging::log_message_factory::create_error("module", "file.cpp", 100, "error message");
  EXPECT_FALSE(serialized.empty());

  PB::Log::LogEntry entry;
  EXPECT_TRUE(entry.ParseFromString(serialized));
  ASSERT_EQ(entry.entry_size(), 1);

  const auto& msg = entry.entry(0);
  EXPECT_EQ(msg.sender(), "module");
  EXPECT_EQ(msg.file(), "file.cpp");
  EXPECT_EQ(msg.line(), 100);
  EXPECT_EQ(msg.message(), "error message");
  EXPECT_EQ(msg.level(), PB::Log::LogEntry_Entry_Level_LOG_ERROR);
}

TEST(log_message_factory, create_warning_returns_valid_message) {
  const std::string serialized = nsclient::logging::log_message_factory::create_warning("module", "file.cpp", 200, "warning message");
  EXPECT_FALSE(serialized.empty());

  PB::Log::LogEntry entry;
  EXPECT_TRUE(entry.ParseFromString(serialized));
  ASSERT_EQ(entry.entry_size(), 1);

  const auto& msg = entry.entry(0);
  EXPECT_EQ(msg.sender(), "module");
  EXPECT_EQ(msg.file(), "file.cpp");
  EXPECT_EQ(msg.line(), 200);
  EXPECT_EQ(msg.message(), "warning message");
  EXPECT_EQ(msg.level(), PB::Log::LogEntry_Entry_Level_LOG_WARNING);
}

TEST(log_message_factory, create_info_returns_valid_message) {
  const std::string serialized = nsclient::logging::log_message_factory::create_info("module", "file.cpp", 300, "info message");
  EXPECT_FALSE(serialized.empty());

  PB::Log::LogEntry entry;
  EXPECT_TRUE(entry.ParseFromString(serialized));
  ASSERT_EQ(entry.entry_size(), 1);

  const auto& msg = entry.entry(0);
  EXPECT_EQ(msg.sender(), "module");
  EXPECT_EQ(msg.file(), "file.cpp");
  EXPECT_EQ(msg.line(), 300);
  EXPECT_EQ(msg.message(), "info message");
  EXPECT_EQ(msg.level(), PB::Log::LogEntry_Entry_Level_LOG_INFO);
}

TEST(log_message_factory, create_debug_returns_valid_message) {
  const std::string serialized = nsclient::logging::log_message_factory::create_debug("module", "file.cpp", 400, "debug message");
  EXPECT_FALSE(serialized.empty());

  PB::Log::LogEntry entry;
  EXPECT_TRUE(entry.ParseFromString(serialized));
  ASSERT_EQ(entry.entry_size(), 1);

  const auto& msg = entry.entry(0);
  EXPECT_EQ(msg.sender(), "module");
  EXPECT_EQ(msg.file(), "file.cpp");
  EXPECT_EQ(msg.line(), 400);
  EXPECT_EQ(msg.message(), "debug message");
  EXPECT_EQ(msg.level(), PB::Log::LogEntry_Entry_Level_LOG_DEBUG);
}

TEST(log_message_factory, create_trace_returns_valid_message) {
  const std::string serialized = nsclient::logging::log_message_factory::create_trace("module", "file.cpp", 500, "trace message");
  EXPECT_FALSE(serialized.empty());

  PB::Log::LogEntry entry;
  EXPECT_TRUE(entry.ParseFromString(serialized));
  ASSERT_EQ(entry.entry_size(), 1);

  const auto& msg = entry.entry(0);
  EXPECT_EQ(msg.sender(), "module");
  EXPECT_EQ(msg.file(), "file.cpp");
  EXPECT_EQ(msg.line(), 500);
  EXPECT_EQ(msg.message(), "trace message");
  EXPECT_EQ(msg.level(), PB::Log::LogEntry_Entry_Level_LOG_TRACE);
}

// ============================================================================
// Tests for edge cases
// ============================================================================

TEST(log_message_factory, create_with_empty_module) {
  const std::string serialized = nsclient::logging::log_message_factory::create_info("", "file.cpp", 100, "message");
  EXPECT_FALSE(serialized.empty());

  PB::Log::LogEntry entry;
  EXPECT_TRUE(entry.ParseFromString(serialized));
  ASSERT_EQ(entry.entry_size(), 1);
  EXPECT_EQ(entry.entry(0).sender(), "");
}

TEST(log_message_factory, create_with_empty_message) {
  const std::string serialized = nsclient::logging::log_message_factory::create_info("module", "file.cpp", 100, "");
  EXPECT_FALSE(serialized.empty());

  PB::Log::LogEntry entry;
  EXPECT_TRUE(entry.ParseFromString(serialized));
  ASSERT_EQ(entry.entry_size(), 1);
  EXPECT_EQ(entry.entry(0).message(), "");
}

TEST(log_message_factory, create_with_multiline_message) {
  const std::string multiline = "line1\nline2\nline3";
  const std::string serialized = nsclient::logging::log_message_factory::create_info("module", "file.cpp", 100, multiline);
  EXPECT_FALSE(serialized.empty());

  PB::Log::LogEntry entry;
  EXPECT_TRUE(entry.ParseFromString(serialized));
  ASSERT_EQ(entry.entry_size(), 1);
  EXPECT_EQ(entry.entry(0).message(), multiline);
}

TEST(log_message_factory, create_with_special_characters) {
  const std::string special = "message with <xml> & \"quotes\" and 'apostrophes'";
  const std::string serialized = nsclient::logging::log_message_factory::create_info("module", "file.cpp", 100, special);
  EXPECT_FALSE(serialized.empty());

  PB::Log::LogEntry entry;
  EXPECT_TRUE(entry.ParseFromString(serialized));
  ASSERT_EQ(entry.entry_size(), 1);
  EXPECT_EQ(entry.entry(0).message(), special);
}

TEST(log_message_factory, create_with_zero_line_number) {
  const std::string serialized = nsclient::logging::log_message_factory::create_info("module", "file.cpp", 0, "message");
  EXPECT_FALSE(serialized.empty());

  PB::Log::LogEntry entry;
  EXPECT_TRUE(entry.ParseFromString(serialized));
  ASSERT_EQ(entry.entry_size(), 1);
  EXPECT_EQ(entry.entry(0).line(), 0);
}

TEST(log_message_factory, create_with_negative_line_number) {
  const std::string serialized = nsclient::logging::log_message_factory::create_info("module", "file.cpp", -1, "message");
  EXPECT_FALSE(serialized.empty());

  PB::Log::LogEntry entry;
  EXPECT_TRUE(entry.ParseFromString(serialized));
  ASSERT_EQ(entry.entry_size(), 1);
  EXPECT_EQ(entry.entry(0).line(), -1);
}

