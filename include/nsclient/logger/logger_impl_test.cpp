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
#include <nsclient/logger/logger_impl.hpp>
#include <string>
#include <vector>

// A concrete test implementation of logger_impl
class TestLogger : public nsclient::logging::logger_impl {
 public:
  std::vector<std::string> logged_messages;
  std::vector<nsclient::logging::logging_subscriber_instance> subscribers_;

  void do_log(const std::string data) override {
    logged_messages.push_back(data);
    for (const auto& sub : subscribers_) {
      sub->on_log_message(data);
    }
  }

  void add_subscriber(const nsclient::logging::logging_subscriber_instance subscriber) override { subscribers_.push_back(subscriber); }

  void clear_subscribers() override { subscribers_.clear(); }

  bool startup() override { return true; }
  bool shutdown() override { return true; }
  void destroy() override {}
  void configure() override {}
  void set_backend(std::string backend) override {}

  void clear_logs() { logged_messages.clear(); }
};

// A test subscriber
class TestSubscriber : public nsclient::logging::logging_subscriber {
 public:
  std::vector<std::string> received_messages;
  void on_log_message(const std::string& payload) override { received_messages.push_back(payload); }
};

// ============================================================================
// Tests for default log level behavior
// ============================================================================

TEST(logger_impl, default_level_is_info) {
  const TestLogger logger;
  EXPECT_EQ(logger.get_log_level(), "message");
}

TEST(logger_impl, default_should_methods) {
  const TestLogger logger;
  EXPECT_TRUE(logger.should_info());
  EXPECT_TRUE(logger.should_warning());
  EXPECT_TRUE(logger.should_error());
  EXPECT_TRUE(logger.should_critical());
  EXPECT_FALSE(logger.should_debug());
  EXPECT_FALSE(logger.should_trace());
}

// ============================================================================
// Tests for set_log_level
// ============================================================================

TEST(logger_impl, set_log_level_debug) {
  TestLogger logger;
  logger.set_log_level("debug");
  EXPECT_EQ(logger.get_log_level(), "debug");
  EXPECT_TRUE(logger.should_debug());
}

TEST(logger_impl, set_log_level_trace) {
  TestLogger logger;
  logger.set_log_level("trace");
  EXPECT_EQ(logger.get_log_level(), "trace");
  EXPECT_TRUE(logger.should_trace());
}

TEST(logger_impl, set_log_level_invalid_logs_error) {
  TestLogger logger;
  logger.set_log_level("invalid");
  // Should have logged an error about invalid log level
  EXPECT_FALSE(logger.logged_messages.empty());
}

// ============================================================================
// Tests for logging methods
// ============================================================================

TEST(logger_impl, info_logs_when_level_permits) {
  TestLogger logger;
  logger.set_log_level("info");
  logger.info("module", "file.cpp", 100, "test info message");

  ASSERT_EQ(logger.logged_messages.size(), 1u);

  PB::Log::LogEntry entry;
  EXPECT_TRUE(entry.ParseFromString(logger.logged_messages[0]));
  ASSERT_GT(entry.entry_size(), 0);
  EXPECT_EQ(entry.entry(0).level(), PB::Log::LogEntry_Entry_Level_LOG_INFO);
  EXPECT_EQ(entry.entry(0).message(), "test info message");
}

TEST(logger_impl, info_does_not_log_when_level_is_warning) {
  TestLogger logger;
  logger.set_log_level("warning");
  logger.info("module", "file.cpp", 100, "test info message");

  EXPECT_TRUE(logger.logged_messages.empty());
}

TEST(logger_impl, debug_logs_when_level_permits) {
  TestLogger logger;
  logger.set_log_level("debug");
  logger.debug("module", "file.cpp", 100, "test debug message");

  ASSERT_EQ(logger.logged_messages.size(), 1u);

  PB::Log::LogEntry entry;
  EXPECT_TRUE(entry.ParseFromString(logger.logged_messages[0]));
  ASSERT_GT(entry.entry_size(), 0);
  EXPECT_EQ(entry.entry(0).level(), PB::Log::LogEntry_Entry_Level_LOG_DEBUG);
}

TEST(logger_impl, debug_does_not_log_when_level_is_info) {
  TestLogger logger;
  logger.set_log_level("info");
  logger.debug("module", "file.cpp", 100, "test debug message");

  EXPECT_TRUE(logger.logged_messages.empty());
}

TEST(logger_impl, trace_logs_when_level_permits) {
  TestLogger logger;
  logger.set_log_level("trace");
  logger.trace("module", "file.cpp", 100, "test trace message");

  ASSERT_EQ(logger.logged_messages.size(), 1u);

  PB::Log::LogEntry entry;
  EXPECT_TRUE(entry.ParseFromString(logger.logged_messages[0]));
  ASSERT_GT(entry.entry_size(), 0);
  EXPECT_EQ(entry.entry(0).level(), PB::Log::LogEntry_Entry_Level_LOG_TRACE);
}

TEST(logger_impl, trace_does_not_log_when_level_is_debug) {
  TestLogger logger;
  logger.set_log_level("debug");
  logger.trace("module", "file.cpp", 100, "test trace message");

  EXPECT_TRUE(logger.logged_messages.empty());
}

TEST(logger_impl, warning_logs_when_level_permits) {
  TestLogger logger;
  logger.set_log_level("warning");
  logger.warning("module", "file.cpp", 100, "test warning message");

  ASSERT_EQ(logger.logged_messages.size(), 1u);

  PB::Log::LogEntry entry;
  EXPECT_TRUE(entry.ParseFromString(logger.logged_messages[0]));
  ASSERT_GT(entry.entry_size(), 0);
  EXPECT_EQ(entry.entry(0).level(), PB::Log::LogEntry_Entry_Level_LOG_WARNING);
}

TEST(logger_impl, warning_does_not_log_when_level_is_error) {
  TestLogger logger;
  logger.set_log_level("error");
  logger.warning("module", "file.cpp", 100, "test warning message");

  EXPECT_TRUE(logger.logged_messages.empty());
}

TEST(logger_impl, error_logs_when_level_permits) {
  TestLogger logger;
  logger.set_log_level("error");
  logger.error("module", "file.cpp", 100, "test error message");

  ASSERT_EQ(logger.logged_messages.size(), 1u);

  PB::Log::LogEntry entry;
  EXPECT_TRUE(entry.ParseFromString(logger.logged_messages[0]));
  ASSERT_GT(entry.entry_size(), 0);
  EXPECT_EQ(entry.entry(0).level(), PB::Log::LogEntry_Entry_Level_LOG_ERROR);
}

TEST(logger_impl, error_does_not_log_when_level_is_critical) {
  TestLogger logger;
  logger.set_log_level("critical");
  logger.error("module", "file.cpp", 100, "test error message");

  EXPECT_TRUE(logger.logged_messages.empty());
}

TEST(logger_impl, critical_always_logs) {
  TestLogger logger;
  logger.set_log_level("critical");
  logger.critical("module", "file.cpp", 100, "test critical message");

  ASSERT_EQ(logger.logged_messages.size(), 1u);

  PB::Log::LogEntry entry;
  EXPECT_TRUE(entry.ParseFromString(logger.logged_messages[0]));
  ASSERT_GT(entry.entry_size(), 0);
  EXPECT_EQ(entry.entry(0).level(), PB::Log::LogEntry_Entry_Level_LOG_CRITICAL);
}

// ============================================================================
// Tests for raw logging
// ============================================================================

TEST(logger_impl, raw_bypasses_level_check) {
  TestLogger logger;
  logger.set_log_level("critical");

  const std::string raw_message = "raw test message";
  logger.raw(raw_message);

  ASSERT_EQ(logger.logged_messages.size(), 1u);
  EXPECT_EQ(logger.logged_messages[0], raw_message);
}

// ============================================================================
// Tests for subscribers
// ============================================================================

TEST(logger_impl, add_subscriber) {
  TestLogger logger;
  const auto subscriber = std::make_shared<TestSubscriber>();

  logger.add_subscriber(subscriber);
  logger.info("mod", "f.cpp", 1, "test");

  EXPECT_FALSE(subscriber->received_messages.empty());
}

TEST(logger_impl, clear_subscribers) {
  TestLogger logger;
  const auto subscriber = std::make_shared<TestSubscriber>();

  logger.add_subscriber(subscriber);
  logger.clear_subscribers();
  logger.info("mod", "f.cpp", 1, "test");

  EXPECT_TRUE(subscriber->received_messages.empty());
}

TEST(logger_impl, multiple_subscribers_receive_messages) {
  TestLogger logger;
  const auto subscriber1 = std::make_shared<TestSubscriber>();
  const auto subscriber2 = std::make_shared<TestSubscriber>();

  logger.add_subscriber(subscriber1);
  logger.add_subscriber(subscriber2);
  logger.info("mod", "f.cpp", 1, "test");

  EXPECT_EQ(subscriber1->received_messages.size(), 1u);
  EXPECT_EQ(subscriber2->received_messages.size(), 1u);
}

// ============================================================================
// Tests for message content
// ============================================================================

TEST(logger_impl, log_message_contains_correct_module) {
  TestLogger logger;
  logger.set_log_level("info");
  logger.info("my_module", "file.cpp", 100, "message");

  ASSERT_EQ(logger.logged_messages.size(), 1u);

  PB::Log::LogEntry entry;
  EXPECT_TRUE(entry.ParseFromString(logger.logged_messages[0]));
  ASSERT_GT(entry.entry_size(), 0);
  EXPECT_EQ(entry.entry(0).sender(), "my_module");
}

TEST(logger_impl, log_message_contains_correct_file) {
  TestLogger logger;
  logger.set_log_level("info");
  logger.info("module", "my_file.cpp", 100, "message");

  ASSERT_EQ(logger.logged_messages.size(), 1u);

  PB::Log::LogEntry entry;
  EXPECT_TRUE(entry.ParseFromString(logger.logged_messages[0]));
  ASSERT_GT(entry.entry_size(), 0);
  EXPECT_EQ(entry.entry(0).file(), "my_file.cpp");
}

TEST(logger_impl, log_message_contains_correct_line) {
  TestLogger logger;
  logger.set_log_level("info");
  logger.info("module", "file.cpp", 42, "message");

  ASSERT_EQ(logger.logged_messages.size(), 1u);

  PB::Log::LogEntry entry;
  EXPECT_TRUE(entry.ParseFromString(logger.logged_messages[0]));
  ASSERT_GT(entry.entry_size(), 0);
  EXPECT_EQ(entry.entry(0).line(), 42);
}
