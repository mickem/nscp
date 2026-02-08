/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 */
#include "plugin_cache.hpp"

#include <gtest/gtest.h>

#include <memory>
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
  void clear_subscribers() override {}

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
}
TEST(PluginCacheItemTest, CopyConstructor) {
  nsclient::core::plugin_cache_item item1;
  item1.id = 42;
  item1.dll = "test.dll";
  item1.alias = "test_alias";
  item1.is_loaded = true;
  nsclient::core::plugin_cache_item item2(item1);
  EXPECT_EQ(item2.id, 42u);
  EXPECT_EQ(item2.dll, "test.dll");
  EXPECT_TRUE(item2.is_loaded);
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
