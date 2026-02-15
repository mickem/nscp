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

#include <nsclient/logger/log_driver_interface_impl.hpp>
#include <nsclient/logger/log_message_factory.hpp>
#include <string>
#include <vector>

// A concrete implementation for testing
class TestLogDriver : public nsclient::logging::log_driver_interface_impl {
 public:
  std::vector<std::string> logged_messages;

  void do_log(std::string data) override { logged_messages.push_back(data); }

  void synch_configure() override {}

  void asynch_configure() override {}

  void clear_logs() { logged_messages.clear(); }
};

// ============================================================================
// Tests for default state
// ============================================================================

TEST(log_driver_interface_impl, default_not_console) {
  const TestLogDriver driver;
  EXPECT_FALSE(driver.is_console());
}

TEST(log_driver_interface_impl, default_not_oneline) {
  const TestLogDriver driver;
  EXPECT_FALSE(driver.is_oneline());
}

TEST(log_driver_interface_impl, default_not_no_std_err) {
  const TestLogDriver driver;
  EXPECT_FALSE(driver.is_no_std_err());
}

TEST(log_driver_interface_impl, default_not_started) {
  const TestLogDriver driver;
  EXPECT_FALSE(driver.is_started());
}

// ============================================================================
// Tests for set_config with string
// ============================================================================

TEST(log_driver_interface_impl, set_config_console) {
  TestLogDriver driver;
  driver.set_config("console");
  EXPECT_TRUE(driver.is_console());
  EXPECT_FALSE(driver.is_oneline());
  EXPECT_FALSE(driver.is_no_std_err());
}

TEST(log_driver_interface_impl, set_config_oneline) {
  TestLogDriver driver;
  driver.set_config("oneline");
  EXPECT_FALSE(driver.is_console());
  EXPECT_TRUE(driver.is_oneline());
  EXPECT_FALSE(driver.is_no_std_err());
}

TEST(log_driver_interface_impl, set_config_no_std_err) {
  TestLogDriver driver;
  driver.set_config("no-std-err");
  EXPECT_FALSE(driver.is_console());
  EXPECT_FALSE(driver.is_oneline());
  EXPECT_TRUE(driver.is_no_std_err());
}

TEST(log_driver_interface_impl, set_config_multiple) {
  TestLogDriver driver;
  driver.set_config("console");
  driver.set_config("oneline");
  driver.set_config("no-std-err");
  EXPECT_TRUE(driver.is_console());
  EXPECT_TRUE(driver.is_oneline());
  EXPECT_TRUE(driver.is_no_std_err());
}

TEST(log_driver_interface_impl, set_config_invalid_logs_error) {
  TestLogDriver driver;
  driver.set_config("invalid-key");

  // Should have logged an error message
  ASSERT_FALSE(driver.logged_messages.empty());
}

// ============================================================================
// Tests for startup and shutdown
// ============================================================================

TEST(log_driver_interface_impl, startup_sets_is_started) {
  TestLogDriver driver;
  EXPECT_FALSE(driver.is_started());
  EXPECT_TRUE(driver.startup());
  EXPECT_TRUE(driver.is_started());
}

TEST(log_driver_interface_impl, shutdown_clears_is_started) {
  TestLogDriver driver;
  driver.startup();
  EXPECT_TRUE(driver.is_started());
  EXPECT_TRUE(driver.shutdown());
  EXPECT_FALSE(driver.is_started());
}

TEST(log_driver_interface_impl, startup_returns_true) {
  TestLogDriver driver;
  EXPECT_TRUE(driver.startup());
}

TEST(log_driver_interface_impl, shutdown_returns_true) {
  TestLogDriver driver;
  EXPECT_TRUE(driver.shutdown());
}

// ============================================================================
// Tests for set_config from another driver
// ============================================================================

TEST(log_driver_interface_impl, set_config_from_other_console) {
  TestLogDriver source;
  source.set_config("console");

  TestLogDriver target;
  target.set_config(std::shared_ptr<nsclient::logging::log_driver_interface>(&source, [](nsclient::logging::log_driver_interface*) {}));

  EXPECT_TRUE(target.is_console());
  EXPECT_FALSE(target.is_oneline());
  EXPECT_FALSE(target.is_no_std_err());
}

TEST(log_driver_interface_impl, set_config_from_other_all_options) {
  TestLogDriver source;
  source.set_config("console");
  source.set_config("oneline");
  source.set_config("no-std-err");
  source.startup();

  TestLogDriver target;
  target.set_config(std::shared_ptr<nsclient::logging::log_driver_interface>(&source, [](nsclient::logging::log_driver_interface*) {}));

  EXPECT_TRUE(target.is_console());
  EXPECT_TRUE(target.is_oneline());
  EXPECT_TRUE(target.is_no_std_err());
  EXPECT_TRUE(target.is_started());
}

TEST(log_driver_interface_impl, set_config_from_other_started) {
  TestLogDriver source;
  source.startup();

  TestLogDriver target;
  target.set_config(std::shared_ptr<nsclient::logging::log_driver_interface>(&source, [](nsclient::logging::log_driver_interface*) {}));

  EXPECT_TRUE(target.is_started());
}

TEST(log_driver_interface_impl, set_config_from_other_not_started) {
  TestLogDriver source;
  // source not started

  TestLogDriver target;
  target.set_config(std::shared_ptr<nsclient::logging::log_driver_interface>(&source, [](nsclient::logging::log_driver_interface*) {}));

  EXPECT_FALSE(target.is_started());
}

// ============================================================================
// Tests for do_log
// ============================================================================

TEST(log_driver_interface_impl, do_log_stores_message) {
  TestLogDriver driver;
  const std::string msg = nsclient::logging::log_message_factory::create_info("mod", "f.cpp", 1, "test");
  driver.do_log(msg);

  ASSERT_EQ(driver.logged_messages.size(), 1u);
  EXPECT_EQ(driver.logged_messages[0], msg);
}

TEST(log_driver_interface_impl, do_log_multiple_messages) {
  TestLogDriver driver;
  const std::string msg1 = nsclient::logging::log_message_factory::create_info("mod", "f.cpp", 1, "msg1");
  const std::string msg2 = nsclient::logging::log_message_factory::create_error("mod", "f.cpp", 2, "msg2");
  const std::string msg3 = nsclient::logging::log_message_factory::create_debug("mod", "f.cpp", 3, "msg3");

  driver.do_log(msg1);
  driver.do_log(msg2);
  driver.do_log(msg3);

  ASSERT_EQ(driver.logged_messages.size(), 3u);
  EXPECT_EQ(driver.logged_messages[0], msg1);
  EXPECT_EQ(driver.logged_messages[1], msg2);
  EXPECT_EQ(driver.logged_messages[2], msg3);
}
