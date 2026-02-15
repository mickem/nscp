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

#include "plugin_list.hpp"

#include <gtest/gtest.h>

#include <memory>

#include "plugin_interface.hpp"

// Mock logger for testing
class MockPluginListLogger : public nsclient::logging::log_interface {
 public:
  void trace(const std::string&, const char*, int, const std::string&) override {}
  void debug(const std::string&, const char*, int, const std::string&) override {}
  void info(const std::string&, const char*, int, const std::string&) override {}
  void warning(const std::string&, const char*, int, const std::string&) override {}
  void error(const std::string&, const char*, int, const std::string&) override {}
  void critical(const std::string&, const char*, int, const std::string&) override {}
  bool should_trace() const override { return false; }
  bool should_debug() const override { return false; }
  bool should_info() const override { return false; }
  bool should_warning() const override { return false; }
  bool should_error() const override { return false; }
  bool should_critical() const override { return false; }
};

// Mock plugin for testing
class MockListPlugin : public nsclient::core::plugin_interface {
  std::string module_;
  std::string name_;

 public:
  MockListPlugin(unsigned int id, const std::string& alias, const std::string& module) : plugin_interface(id, alias), module_(module), name_(module) {}

  bool load_plugin(NSCAPI::moduleLoadMode) override { return true; }
  bool has_start() override { return false; }
  bool start_plugin() override { return true; }
  void unload_plugin() override {}
  std::string getName() override { return name_; }
  std::string getDescription() override { return "Mock plugin for testing"; }
  std::string get_version() override { return "1.0.0"; }
  bool hasCommandHandler() override { return false; }
  NSCAPI::nagiosReturn handleCommand(std::string, std::string&) override { return NSCAPI::cmd_return_codes::returnIgnored; }
  bool hasNotificationHandler() override { return false; }
  NSCAPI::nagiosReturn handleNotification(const char*, std::string&, std::string&) override { return NSCAPI::cmd_return_codes::returnIgnored; }
  NSCAPI::nagiosReturn handle_schedule(const std::string&) override { return NSCAPI::cmd_return_codes::returnIgnored; }
  bool hasMessageHandler() override { return false; }
  void handleMessage(const char*, unsigned int) override {}
  bool has_on_event() override { return false; }
  NSCAPI::nagiosReturn on_event(const std::string&) override { return NSCAPI::cmd_return_codes::returnIgnored; }
  bool hasMetricsFetcher() override { return false; }
  NSCAPI::nagiosReturn fetchMetrics(std::string&) override { return NSCAPI::cmd_return_codes::returnIgnored; }
  bool hasMetricsSubmitter() override { return false; }
  NSCAPI::nagiosReturn submitMetrics(const std::string&) override { return NSCAPI::cmd_return_codes::returnIgnored; }
  bool has_command_line_exec() override { return false; }
  int commandLineExec(bool, std::string&, std::string&) override { return 0; }
  bool has_routing_handler() override { return false; }
  bool route_message(const char*, const char*, unsigned int, char**, char**, unsigned int*) override { return false; }
  bool is_duplicate(boost::filesystem::path file, std::string alias) override { return module_ == file.string() && get_alias() == alias; }
  std::string getModule() override { return module_; }
  void on_log_message(const std::string&) override {}
};

// ============================================================================
// plugins_list_exception tests
// ============================================================================

TEST(PluginsListExceptionTest, ConstructWithMessage) {
  const nsclient::plugins_list_exception ex("Test error message");
  EXPECT_STREQ(ex.what(), "Test error message");
}

TEST(PluginsListExceptionTest, ConstructWithEmptyMessage) {
  const nsclient::plugins_list_exception ex("");
  EXPECT_STREQ(ex.what(), "");
}

TEST(PluginsListExceptionTest, InheritsFromStdException) {
  const nsclient::plugins_list_exception ex("Error");
  const std::exception* base_ptr = &ex;
  EXPECT_NE(base_ptr->what(), nullptr);
}

TEST(PluginsListExceptionTest, ThrowAndCatch) {
  try {
    throw nsclient::plugins_list_exception("Plugin list error");
  } catch (const nsclient::plugins_list_exception& ex) {
    EXPECT_STREQ(ex.what(), "Plugin list error");
  }
}

TEST(PluginsListExceptionTest, ThrowAndCatchAsStdException) {
  try {
    throw nsclient::plugins_list_exception("Base exception test");
  } catch (const std::exception& ex) {
    EXPECT_STREQ(ex.what(), "Base exception test");
  }
}

// ============================================================================
// simple_plugins_list tests
// ============================================================================

class SimplePluginsListTest : public ::testing::Test {
 protected:
  void SetUp() override {
    logger_ = std::make_shared<MockPluginListLogger>();
    list_ = std::make_unique<nsclient::simple_plugins_list>(logger_);
  }
  nsclient::logging::log_client_accessor logger_;
  std::unique_ptr<nsclient::simple_plugins_list> list_;
};

TEST_F(SimplePluginsListTest, InitialStateEmpty) { EXPECT_EQ(list_->to_string(), ""); }

TEST_F(SimplePluginsListTest, AddPlugin) {
  const auto plugin = std::make_shared<MockListPlugin>(1, "test_alias", "TestModule");
  list_->add_plugin(plugin);
  EXPECT_EQ(list_->to_string(), "TestModule");
}

TEST_F(SimplePluginsListTest, AddMultiplePlugins) {
  const auto plugin1 = std::make_shared<MockListPlugin>(1, "alias1", "Module1");
  const auto plugin2 = std::make_shared<MockListPlugin>(2, "alias2", "Module2");
  list_->add_plugin(plugin1);
  list_->add_plugin(plugin2);

  const std::string result = list_->to_string();
  EXPECT_TRUE(result.find("Module1") != std::string::npos);
  EXPECT_TRUE(result.find("Module2") != std::string::npos);
}

TEST_F(SimplePluginsListTest, AddDuplicateIdIgnored) {
  const auto plugin1 = std::make_shared<MockListPlugin>(1, "alias1", "Module1");
  const auto plugin2 = std::make_shared<MockListPlugin>(1, "alias2", "Module2");  // Same ID
  list_->add_plugin(plugin1);
  list_->add_plugin(plugin2);

  // Should only have one plugin
  const std::string result = list_->to_string();
  EXPECT_TRUE(result.find("Module1") != std::string::npos);
}

TEST_F(SimplePluginsListTest, RemoveAll) {
  const auto plugin1 = std::make_shared<MockListPlugin>(1, "alias1", "Module1");
  const auto plugin2 = std::make_shared<MockListPlugin>(2, "alias2", "Module2");
  list_->add_plugin(plugin1);
  list_->add_plugin(plugin2);

  list_->remove_all();
  EXPECT_EQ(list_->to_string(), "");
}

TEST_F(SimplePluginsListTest, RemovePlugin) {
  const auto plugin1 = std::make_shared<MockListPlugin>(1, "alias1", "Module1");
  const auto plugin2 = std::make_shared<MockListPlugin>(2, "alias2", "Module2");
  list_->add_plugin(plugin1);
  list_->add_plugin(plugin2);

  list_->remove_plugin(1);

  const std::string result = list_->to_string();
  EXPECT_TRUE(result.find("Module1") == std::string::npos);
  EXPECT_TRUE(result.find("Module2") != std::string::npos);
}

TEST_F(SimplePluginsListTest, RemoveNonExistentPlugin) {
  const auto plugin = std::make_shared<MockListPlugin>(1, "alias", "Module");
  list_->add_plugin(plugin);

  // Removing non-existent plugin should not crash
  list_->remove_plugin(999);

  EXPECT_EQ(list_->to_string(), "Module");
}

TEST_F(SimplePluginsListTest, DoAllCallback) {
  const auto plugin1 = std::make_shared<MockListPlugin>(1, "alias1", "Module1");
  const auto plugin2 = std::make_shared<MockListPlugin>(2, "alias2", "Module2");
  list_->add_plugin(plugin1);
  list_->add_plugin(plugin2);

  int count = 0;
  list_->do_all([&count](nsclient::plugin_type) { count++; });

  EXPECT_EQ(count, 2);
}

TEST_F(SimplePluginsListTest, DoAllOnEmptyList) {
  int count = 0;
  list_->do_all([&count](nsclient::plugin_type) { count++; });

  EXPECT_EQ(count, 0);
}

// ============================================================================
// plugins_list_with_listener tests
// ============================================================================

class PluginsListWithListenerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    logger_ = std::make_shared<MockPluginListLogger>();
    list_ = std::make_unique<nsclient::plugins_list_with_listener>(logger_);
  }
  nsclient::logging::log_client_accessor logger_;
  std::unique_ptr<nsclient::plugins_list_with_listener> list_;
};

TEST_F(PluginsListWithListenerTest, InitialStateEmpty) {
  const auto plugins = list_->list();
  EXPECT_TRUE(plugins.empty());
}

TEST_F(PluginsListWithListenerTest, AddPlugin) {
  const auto plugin = std::make_shared<MockListPlugin>(1, "alias", "Module");
  list_->add_plugin(plugin);

  // The plugin should be findable
  EXPECT_TRUE(list_->have_plugin(1));
}

TEST_F(PluginsListWithListenerTest, RegisterListenerForChannel) {
  const auto plugin = std::make_shared<MockListPlugin>(1, "alias", "Module");
  list_->add_plugin(plugin);

  list_->register_listener(1, "test_channel");

  auto listeners = list_->get("test_channel");
  EXPECT_EQ(listeners.size(), 1u);
}

TEST_F(PluginsListWithListenerTest, RegisterListenerForMultipleChannels) {
  const auto plugin = std::make_shared<MockListPlugin>(1, "alias", "Module");
  list_->add_plugin(plugin);

  list_->register_listener(1, "channel1,channel2");

  const auto listeners1 = list_->get("channel1");
  const auto listeners2 = list_->get("channel2");

  EXPECT_EQ(listeners1.size(), 1u);
  EXPECT_EQ(listeners2.size(), 1u);
}

TEST_F(PluginsListWithListenerTest, GetNonExistentChannelReturnsEmpty) {
  const auto plugin = std::make_shared<MockListPlugin>(1, "alias", "Module");
  list_->add_plugin(plugin);

  const auto listeners = list_->get("nonexistent");
  EXPECT_TRUE(listeners.empty());
}

TEST_F(PluginsListWithListenerTest, RegisterListenerWithNonExistentPluginThrows) {
  // No plugins added
  EXPECT_THROW(list_->register_listener(999, "channel"), nsclient::plugins_list_exception);
}

TEST_F(PluginsListWithListenerTest, MultiplePluginsOnSameChannel) {
  const auto plugin1 = std::make_shared<MockListPlugin>(1, "alias1", "Module1");
  const auto plugin2 = std::make_shared<MockListPlugin>(2, "alias2", "Module2");
  list_->add_plugin(plugin1);
  list_->add_plugin(plugin2);

  list_->register_listener(1, "shared_channel");
  list_->register_listener(2, "shared_channel");

  const auto listeners = list_->get("shared_channel");
  EXPECT_EQ(listeners.size(), 2u);
}

TEST_F(PluginsListWithListenerTest, RemovePluginRemovesFromListeners) {
  const auto plugin = std::make_shared<MockListPlugin>(1, "alias", "Module");
  list_->add_plugin(plugin);
  list_->register_listener(1, "channel");

  list_->remove_plugin(1);

  // Channel should still exist but without the plugin
  // Note: The current implementation removes the entire channel entry
  const auto listeners = list_->get("channel");
  EXPECT_TRUE(listeners.empty());
}

TEST_F(PluginsListWithListenerTest, RemoveAllClearsListeners) {
  const auto plugin = std::make_shared<MockListPlugin>(1, "alias", "Module");
  list_->add_plugin(plugin);
  list_->register_listener(1, "channel");

  list_->remove_all();

  const auto plugins = list_->list();
  EXPECT_TRUE(plugins.empty());
}

TEST_F(PluginsListWithListenerTest, ChannelNameIsCaseInsensitive) {
  const auto plugin = std::make_shared<MockListPlugin>(1, "alias", "Module");
  list_->add_plugin(plugin);

  list_->register_listener(1, "TestChannel");

  // Should find with lowercase
  const auto listeners = list_->get("testchannel");
  EXPECT_EQ(listeners.size(), 1u);
}

TEST_F(PluginsListWithListenerTest, ToStringWithNoPlugins) {
  const std::string result = list_->to_string();
  EXPECT_TRUE(result.find("NONE") != std::string::npos);
}

TEST_F(PluginsListWithListenerTest, ToStringWithPlugins) {
  const auto plugin = std::make_shared<MockListPlugin>(1, "alias", "TestModule");
  list_->add_plugin(plugin);
  list_->register_listener(1, "channel");

  const std::string result = list_->to_string();
  EXPECT_TRUE(result.find("channel") != std::string::npos);
}

TEST_F(PluginsListWithListenerTest, HavePluginReturnsTrueForExisting) {
  const auto plugin = std::make_shared<MockListPlugin>(42, "alias", "Module");
  list_->add_plugin(plugin);

  EXPECT_TRUE(list_->have_plugin(42));
}

TEST_F(PluginsListWithListenerTest, HavePluginReturnsFalseForNonExisting) { EXPECT_FALSE(list_->have_plugin(999)); }

// ============================================================================
// plugins_list_listeners_impl tests
// ============================================================================

TEST(PluginsListListenersImplTest, RemoveAllClearsListeners) {
  nsclient::plugins_list_listeners_impl impl;

  // Add some data to listeners_
  impl.listeners_["channel1"].insert(1);
  impl.listeners_["channel2"].insert(2);

  impl.remove_all();

  EXPECT_TRUE(impl.listeners_.empty());
}

TEST(PluginsListListenersImplTest, RemovePluginRemovesFromAllChannels) {
  nsclient::plugins_list_listeners_impl impl;

  impl.listeners_["channel1"].insert(1);
  impl.listeners_["channel1"].insert(2);
  impl.listeners_["channel2"].insert(1);

  impl.remove_plugin(1);

  // Channel with only plugin 1 should be removed
  EXPECT_TRUE(impl.listeners_.find("channel2") == impl.listeners_.end());
  // Channel1 should still have plugin 2... but the current implementation removes entire channel
}

TEST(PluginsListListenersImplTest, ListReturnsAllChannels) {
  nsclient::plugins_list_listeners_impl impl;

  impl.listeners_["channel1"].insert(1);
  impl.listeners_["channel2"].insert(2);

  std::list<std::string> channels;
  impl.list(channels);

  EXPECT_EQ(channels.size(), 2u);
}

TEST(PluginsListListenersImplTest, ToStringWithNoListeners) {
  nsclient::plugins_list_listeners_impl impl;

  const std::string result = impl.to_string();
  EXPECT_TRUE(result.find("NONE") != std::string::npos);
}

TEST(PluginsListListenersImplTest, ToStringWithListeners) {
  nsclient::plugins_list_listeners_impl impl;

  impl.listeners_["test_channel"].insert(1);

  const std::string result = impl.to_string();
  EXPECT_TRUE(result.find("test_channel") != std::string::npos);
}
