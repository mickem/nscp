/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 */
#include "master_plugin_list.hpp"

#include <NSCAPI.h>
#include <gtest/gtest.h>

#include <boost/make_shared.hpp>

#include "plugin_interface.hpp"
// Mock logger for testing
class MockListLogger : public nsclient::logging::logger {
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
// Mock plugin for testing
class MockPlugin : public nsclient::core::plugin_interface {
  std::string module_;

 public:
  MockPlugin(unsigned int id, const std::string& alias, const std::string& module) : plugin_interface(id, alias), module_(module) {}
  bool load_plugin(NSCAPI::moduleLoadMode) override { return true; }
  bool has_start() override { return false; }
  bool start_plugin() override { return true; }
  void unload_plugin() override {}
  std::string getName() override { return module_; }
  std::string getDescription() override { return "Mock plugin"; }
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
  void on_log_message(std::string&) override {}
};
class MasterPluginListTest : public ::testing::Test {
 protected:
  void SetUp() override {
    logger_ = boost::make_shared<MockListLogger>();
    list_ = std::make_unique<nsclient::core::master_plugin_list>(logger_);
  }
  nsclient::logging::logger_instance logger_;
  std::unique_ptr<nsclient::core::master_plugin_list> list_;
};
TEST_F(MasterPluginListTest, InitialStateEmpty) {
  const auto plugins = list_->get_plugins();
  EXPECT_TRUE(plugins.empty());
}
TEST_F(MasterPluginListTest, GetNextIdIncrementsSequentially) {
  EXPECT_EQ(list_->get_next_id(), 0u);
  EXPECT_EQ(list_->get_next_id(), 1u);
  EXPECT_EQ(list_->get_next_id(), 2u);
}
TEST_F(MasterPluginListTest, AppendPlugin) {
  const auto plugin = boost::make_shared<MockPlugin>(0, "alias1", "module1");
  list_->append_plugin(plugin);
  const auto plugins = list_->get_plugins();
  EXPECT_EQ(plugins.size(), 1u);
}
TEST_F(MasterPluginListTest, AppendMultiplePlugins) {
  for (int i = 0; i < 5; ++i) {
    auto plugin = boost::make_shared<MockPlugin>(i, "alias" + std::to_string(i), "module" + std::to_string(i));
    list_->append_plugin(plugin);
  }
  const auto plugins = list_->get_plugins();
  EXPECT_EQ(plugins.size(), 5u);
}
TEST_F(MasterPluginListTest, FindByModule) {
  const auto plugin = boost::make_shared<MockPlugin>(1, "test_alias", "TestModule");
  list_->append_plugin(plugin);
  const auto found = list_->find_by_module("TestModule");
  EXPECT_TRUE(found != nullptr);
  EXPECT_EQ(found->getModule(), "TestModule");
}
TEST_F(MasterPluginListTest, FindByModuleNotFound) {
  const auto plugin = boost::make_shared<MockPlugin>(1, "alias", "SomeModule");
  list_->append_plugin(plugin);
  const auto found = list_->find_by_module("NonExistent");
  EXPECT_TRUE(found == nullptr);
}
TEST_F(MasterPluginListTest, FindByModuleEmptyString) {
  const auto found = list_->find_by_module("");
  EXPECT_TRUE(found == nullptr);
}
TEST_F(MasterPluginListTest, FindByAlias) {
  const auto plugin = boost::make_shared<MockPlugin>(1, "my_alias", "MyModule");
  list_->append_plugin(plugin);
  const auto found = list_->find_by_alias("my_alias");
  EXPECT_TRUE(found != nullptr);
  EXPECT_EQ(found->get_alias(), "my_alias");
}
TEST_F(MasterPluginListTest, FindByAliasNotFound) {
  const auto plugin = boost::make_shared<MockPlugin>(1, "alias", "Module");
  list_->append_plugin(plugin);
  const auto found = list_->find_by_alias("wrong_alias");
  EXPECT_TRUE(found == nullptr);
}
TEST_F(MasterPluginListTest, FindByAliasEmptyString) {
  const auto found = list_->find_by_alias("");
  EXPECT_TRUE(found == nullptr);
}
TEST_F(MasterPluginListTest, FindById) {
  const auto plugin = boost::make_shared<MockPlugin>(42, "alias", "Module");
  list_->append_plugin(plugin);
  const auto found = list_->find_by_id(42);
  EXPECT_TRUE(found != nullptr);
  EXPECT_EQ(found->get_id(), 42u);
}
TEST_F(MasterPluginListTest, FindByIdNotFound) {
  const auto plugin = boost::make_shared<MockPlugin>(1, "alias", "Module");
  list_->append_plugin(plugin);
  const auto found = list_->find_by_id(999);
  EXPECT_TRUE(found == nullptr);
}
TEST_F(MasterPluginListTest, RemovePlugin) {
  const auto plugin = boost::make_shared<MockPlugin>(10, "alias", "Module");
  list_->append_plugin(plugin);
  EXPECT_EQ(list_->get_plugins().size(), 1u);
  list_->remove(10);
  EXPECT_EQ(list_->get_plugins().size(), 0u);
}
TEST_F(MasterPluginListTest, RemoveNonExistentPlugin) {
  const auto plugin = boost::make_shared<MockPlugin>(1, "alias", "Module");
  list_->append_plugin(plugin);
  list_->remove(999);
  EXPECT_EQ(list_->get_plugins().size(), 1u);
}
TEST_F(MasterPluginListTest, Clear) {
  for (int i = 0; i < 5; ++i) {
    auto plugin = boost::make_shared<MockPlugin>(i, "alias" + std::to_string(i), "module" + std::to_string(i));
    list_->append_plugin(plugin);
  }
  EXPECT_EQ(list_->get_plugins().size(), 5u);
  list_->clear();
  EXPECT_EQ(list_->get_plugins().size(), 0u);
}
TEST_F(MasterPluginListTest, FindDuplicate) {
  const auto plugin = boost::make_shared<MockPlugin>(1, "alias", "module.dll");
  list_->append_plugin(plugin);
  const auto dup = list_->find_duplicate(boost::filesystem::path("module.dll"), "alias");
  EXPECT_TRUE(dup != nullptr);
  EXPECT_EQ(dup->get_id(), 1u);
}
TEST_F(MasterPluginListTest, FindDuplicateNotFound) {
  const auto plugin = boost::make_shared<MockPlugin>(1, "alias", "module.dll");
  list_->append_plugin(plugin);
  const auto dup = list_->find_duplicate(boost::filesystem::path("other.dll"), "alias");
  EXPECT_TRUE(dup == nullptr);
}
