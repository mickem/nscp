// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "plugin_cache.hpp"

#include <gtest/gtest.h>

#include <boost/filesystem/path.hpp>
#include <memory>

#include "plugin_interface.hpp"
// Mock logger for testing
class MockCacheLogger : public nsclient::logging::logger {
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

  void raw(const std::string& message) override {}

  void add_subscriber(nsclient::logging::logging_subscriber_instance subscriber) override {}
  nsclient::logging::unsubscribe_result close_subscriber(nsclient::logging::logging_subscriber_instance) override { return {}; }
  void reopen_subscriber(nsclient::logging::logging_subscriber_instance) override {}
  bool drop_subscriber(nsclient::logging::logging_subscriber_instance) override { return false; }
  nsclient::logging::unsubscribe_result remove_subscriber(nsclient::logging::logging_subscriber_instance) override { return {}; }
  std::vector<nsclient::logging::logging_subscriber_instance> clear_subscribers() override { return {}; }

  bool startup() override { return false; }
  bool shutdown() override { return false; }
  void destroy() override {}
  void configure() override {}

  void set_log_level(std::string level) override {}
  std::string get_log_level() const override { return ""; }

  void set_backend(std::string backend) override {}
};
class PluginCacheTest : public ::testing::Test {
 protected:
  void SetUp() override {
    logger_ = std::make_shared<MockCacheLogger>();
    cache_ = std::make_unique<nsclient::core::plugin_cache>(logger_);
  }
  nsclient::logging::logger_instance logger_;
  std::unique_ptr<nsclient::core::plugin_cache> cache_;
};
TEST(PluginCacheItemTest, DefaultConstructor) {
  const nsclient::core::plugin_cache_item item;
  EXPECT_EQ(item.id, 0u);
  EXPECT_TRUE(item.dll.empty());
  EXPECT_FALSE(item.is_loaded);
  // A module says it is experimental; nothing says it for it.
  EXPECT_FALSE(item.experimental);
}
TEST(PluginCacheItemTest, CopyConstructor) {
  nsclient::core::plugin_cache_item item1;
  item1.id = 42;
  item1.dll = "test.dll";
  item1.alias = "test_alias";
  item1.is_loaded = true;
  item1.experimental = true;
  nsclient::core::plugin_cache_item item2(item1);
  EXPECT_EQ(item2.id, 42u);
  EXPECT_EQ(item2.dll, "test.dll");
  EXPECT_TRUE(item2.is_loaded);
  EXPECT_TRUE(item2.experimental);
  nsclient::core::plugin_cache_item item3;
  item3 = item1;
  EXPECT_TRUE(item3.experimental);
}
TEST_F(PluginCacheTest, InitialState) {
  EXPECT_FALSE(cache_->has_all());
  EXPECT_TRUE(cache_->get_list().empty());
}
TEST_F(PluginCacheTest, AddPluginsSetHasAll) {
  nsclient::core::plugin_cache::plugin_cache_list_type list;
  nsclient::core::plugin_cache_item item;
  item.id = 1;
  item.dll = "plugin1.dll";
  list.push_back(item);
  cache_->add_plugins(list);
  EXPECT_TRUE(cache_->has_all());
}
TEST_F(PluginCacheTest, RememberPluginsDoesNotSetHasAll) {
  nsclient::core::plugin_cache::plugin_cache_list_type list;
  nsclient::core::plugin_cache_item item;
  item.id = 1;
  item.dll = "plugin1.dll";
  list.push_back(item);
  cache_->remember_plugins(list);
  EXPECT_TRUE(cache_->has_module("plugin1.dll"));
  EXPECT_FALSE(cache_->has_all());
}
TEST_F(PluginCacheTest, HasModuleByDll) {
  nsclient::core::plugin_cache::plugin_cache_list_type list;
  nsclient::core::plugin_cache_item item;
  item.id = 1;
  item.dll = "CheckSystem";
  item.alias = "sys";
  list.push_back(item);
  cache_->add_plugins(list);
  EXPECT_TRUE(cache_->has_module("CheckSystem"));
  EXPECT_FALSE(cache_->has_module("NonExistent"));
}
TEST_F(PluginCacheTest, FindPluginByName) {
  nsclient::core::plugin_cache::plugin_cache_list_type list;
  nsclient::core::plugin_cache_item item;
  item.id = 42;
  item.dll = "TestPlugin";
  list.push_back(item);
  cache_->add_plugins(list);
  auto result = cache_->find_plugin("TestPlugin");
  EXPECT_TRUE(result.is_initialized());
  EXPECT_EQ(result.get(), 42u);
}
TEST_F(PluginCacheTest, FindPluginNotFound) {
  const auto result = cache_->find_plugin("NonExistent");
  EXPECT_FALSE(result.is_initialized());
}
TEST_F(PluginCacheTest, FindPluginInfoById) {
  nsclient::core::plugin_cache::plugin_cache_list_type list;
  nsclient::core::plugin_cache_item item;
  item.id = 100;
  item.dll = "MyPlugin";
  item.alias = "my";
  item.is_loaded = true;
  list.push_back(item);
  cache_->add_plugins(list);
  auto result = cache_->find_plugin_info(100);
  EXPECT_TRUE(result.is_initialized());
  EXPECT_EQ(result->dll, "MyPlugin");
}
TEST_F(PluginCacheTest, FindPluginInfoNotFound) {
  const auto result = cache_->find_plugin_info(999);
  EXPECT_FALSE(result.is_initialized());
}
TEST_F(PluginCacheTest, FindPluginAliasReturnsAlias) {
  nsclient::core::plugin_cache::plugin_cache_list_type list;
  nsclient::core::plugin_cache_item item;
  item.id = 50;
  item.dll = "CheckDisk";
  item.alias = "disk";
  list.push_back(item);
  cache_->add_plugins(list);
  const std::string alias = cache_->find_plugin_alias(50);
  EXPECT_EQ(alias, "disk");
}
TEST_F(PluginCacheTest, FindPluginAliasReturnsDllWhenAliasEmpty) {
  nsclient::core::plugin_cache::plugin_cache_list_type list;
  nsclient::core::plugin_cache_item item;
  item.id = 50;
  item.dll = "CheckDisk";
  item.alias = "";
  list.push_back(item);
  cache_->add_plugins(list);
  const std::string alias = cache_->find_plugin_alias(50);
  EXPECT_EQ(alias, "CheckDisk");
}
TEST_F(PluginCacheTest, FindPluginAliasNotFound) {
  std::string alias = cache_->find_plugin_alias(999);
  EXPECT_NE(alias.find("Failed to find plugin"), std::string::npos);
}
TEST_F(PluginCacheTest, HasModuleEmptyCache) { EXPECT_FALSE(cache_->has_module("AnyModule")); }

// ===== add_plugin / remove_plugin ===========================================
//
// The cache is filled two ways: add_plugins() from a scan of the module
// folder, and add_plugin() as each module is actually loaded. A load must
// update the entry the scan left rather than list the module twice, and an
// unload keeps the entry (so `nscp test` can still offer the module) while
// marking it unloaded.

namespace {
class cache_test_plugin : public nsclient::core::plugin_interface {
  std::string module_;
  bool experimental_;

 public:
  cache_test_plugin(unsigned int id, const std::string& alias, const std::string& module, bool experimental = false)
      : plugin_interface(id, alias), module_(module), experimental_(experimental) {}
  bool load_plugin(NSCAPI::moduleLoadMode) override { return true; }
  bool has_start() override { return false; }
  bool start_plugin() override { return true; }
  bool has_prepare_shutdown() override { return false; }
  void prepare_shutdown_plugin() override {}
  void unload_plugin() override {}
  std::string getName() override { return module_ + " title"; }
  std::string getDescription() override { return module_ + " description"; }
  std::string get_version() override { return "2.0.0"; }
  bool is_experimental() override { return experimental_; }
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
  bool hasFactsFetcher() override { return false; }
  NSCAPI::nagiosReturn fetchFacts(const std::string&, std::string&) override { return NSCAPI::cmd_return_codes::returnIgnored; }
  NSCAPI::nagiosReturn fetchMetrics(std::string&) override { return NSCAPI::cmd_return_codes::returnIgnored; }
  bool hasMetricsSubmitter() override { return false; }
  NSCAPI::nagiosReturn submitMetrics(const std::string&) override { return NSCAPI::cmd_return_codes::returnIgnored; }
  bool has_command_line_exec() override { return false; }
  int commandLineExec(bool, std::string&, std::string&) override { return 0; }
  bool has_routing_handler() override { return false; }
  bool route_message(const char*, const char*, unsigned int, char**, char**, unsigned int*) override { return false; }
  bool is_duplicate(boost::filesystem::path, std::string) override { return false; }
  std::string getModule() override { return module_; }
  void on_log_message(const std::string&) override {}
};
}  // namespace

TEST(PluginCacheItemTest, ConstructedFromALoadedPlugin) {
  const auto plugin = std::make_shared<cache_test_plugin>(7, "alias", "CheckThing", true);
  const nsclient::core::plugin_cache_item item(plugin);
  EXPECT_EQ(7u, item.id);
  EXPECT_EQ("CheckThing", item.dll);
  EXPECT_EQ("alias", item.alias);
  EXPECT_EQ("CheckThing title", item.title);
  EXPECT_EQ("CheckThing description", item.desc);
  EXPECT_EQ("2.0.0", item.version);
  EXPECT_TRUE(item.is_loaded);
  EXPECT_TRUE(item.experimental);
}

TEST_F(PluginCacheTest, AddPluginListsANewModuleAsLoaded) {
  cache_->add_plugin(std::make_shared<cache_test_plugin>(3, "", "CheckThing"));

  const auto list = cache_->get_list();
  ASSERT_EQ(1u, list.size());
  EXPECT_EQ("CheckThing", list.front().dll);
  EXPECT_EQ(3u, list.front().id);
  EXPECT_TRUE(list.front().is_loaded);
  EXPECT_TRUE(cache_->has_module("CheckThing"));
  // Adding one loaded module says nothing about the rest of the folder.
  EXPECT_FALSE(cache_->has_all());
}

TEST_F(PluginCacheTest, AddPluginUpdatesTheEntryAScanLeft) {
  nsclient::core::plugin_cache::plugin_cache_list_type scanned;
  nsclient::core::plugin_cache_item item;
  item.id = 99;
  item.dll = "CheckThing";
  item.desc = "from the scan";
  item.is_loaded = false;
  scanned.push_back(item);
  cache_->add_plugins(scanned);

  cache_->add_plugin(std::make_shared<cache_test_plugin>(4, "", "CheckThing", true));

  const auto list = cache_->get_list();
  ASSERT_EQ(1u, list.size());
  EXPECT_EQ(4u, list.front().id);
  EXPECT_TRUE(list.front().is_loaded);
  EXPECT_EQ("CheckThing title", list.front().title);
  EXPECT_EQ("CheckThing description", list.front().desc);
  EXPECT_TRUE(list.front().experimental);
}

TEST_F(PluginCacheTest, AddPluginKeepsAnAliasedInstanceSeparate) {
  cache_->add_plugin(std::make_shared<cache_test_plugin>(1, "", "CheckThing"));
  cache_->add_plugin(std::make_shared<cache_test_plugin>(2, "second", "CheckThing"));

  ASSERT_EQ(2u, cache_->get_list().size());
  EXPECT_EQ(1u, cache_->find_plugin("CheckThing").value());
  EXPECT_EQ(2u, cache_->find_plugin("second").value());
  EXPECT_EQ("second", cache_->find_plugin_alias(2));
}

TEST_F(PluginCacheTest, RemovePluginMarksTheEntryUnloadedButKeepsIt) {
  cache_->add_plugin(std::make_shared<cache_test_plugin>(5, "", "CheckThing"));
  cache_->remove_plugin(5);

  const auto list = cache_->get_list();
  ASSERT_EQ(1u, list.size());
  EXPECT_FALSE(list.front().is_loaded);
  EXPECT_EQ(static_cast<unsigned int>(-1), list.front().id);
  EXPECT_TRUE(cache_->has_module("CheckThing"));
  // The old id no longer resolves: it may be handed to another module.
  EXPECT_FALSE(cache_->find_plugin_info(5));
}

TEST_F(PluginCacheTest, RemovePluginOfAnUnknownIdChangesNothing) {
  cache_->add_plugin(std::make_shared<cache_test_plugin>(5, "", "CheckThing"));
  cache_->remove_plugin(6);

  const auto list = cache_->get_list();
  ASSERT_EQ(1u, list.size());
  EXPECT_TRUE(list.front().is_loaded);
  EXPECT_EQ(5u, list.front().id);
}

TEST_F(PluginCacheTest, ReloadingAfterRemoveRestoresTheEntry) {
  cache_->add_plugin(std::make_shared<cache_test_plugin>(5, "", "CheckThing"));
  cache_->remove_plugin(5);
  cache_->add_plugin(std::make_shared<cache_test_plugin>(8, "", "CheckThing"));

  const auto list = cache_->get_list();
  ASSERT_EQ(1u, list.size());
  EXPECT_TRUE(list.front().is_loaded);
  EXPECT_EQ(8u, list.front().id);
}
