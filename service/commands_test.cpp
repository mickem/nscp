// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "commands.hpp"

#include <gtest/gtest.h>

#include <memory>

#include "plugins/plugin_interface.hpp"

// Mock logger for testing
class MockCommandsLogger : public nsclient::logging::log_interface {
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

// Mock plugin for testing; hasCommandHandler() must be true or add_plugin() ignores it.
class MockCommandPlugin : public nsclient::core::plugin_interface {
  std::string module_;

 public:
  MockCommandPlugin(unsigned int id, const std::string& alias, const std::string& module) : plugin_interface(id, alias), module_(module) {}
  bool load_plugin(NSCAPI::moduleLoadMode) override { return true; }
  bool has_start() override { return false; }
  bool start_plugin() override { return true; }
  bool has_prepare_shutdown() override { return false; }
  void prepare_shutdown_plugin() override {}
  void unload_plugin() override {}
  std::string getName() override { return module_; }
  std::string getDescription() override { return "Mock plugin for testing"; }
  std::string get_version() override { return "1.0.0"; }
  bool hasCommandHandler() override { return true; }
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

class CommandsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    logger_ = std::make_shared<MockCommandsLogger>();
    commands_ = std::make_unique<nsclient::commands>(logger_);
    plugin_a_ = std::make_shared<MockCommandPlugin>(1, "alias_a", "ModuleA");
    plugin_b_ = std::make_shared<MockCommandPlugin>(2, "alias_b", "ModuleB");
    commands_->add_plugin(plugin_a_);
    commands_->add_plugin(plugin_b_);
  }
  nsclient::logging::log_client_accessor logger_;
  std::unique_ptr<nsclient::commands> commands_;
  std::shared_ptr<MockCommandPlugin> plugin_a_;
  std::shared_ptr<MockCommandPlugin> plugin_b_;
};

TEST_F(CommandsTest, RegisterAndResolveCommand) {
  commands_->register_command(1, "check_foo", "foo check");
  EXPECT_EQ(commands_->get("check_foo"), plugin_a_);
  // Lookup is case-insensitive.
  EXPECT_EQ(commands_->get("Check_Foo"), plugin_a_);
  EXPECT_EQ(commands_->describe("check_foo").plugin_id, 1u);
}

TEST_F(CommandsTest, RegisterAndResolveAlias) {
  commands_->register_alias(1, "check_foo_alias", "foo alias");
  EXPECT_EQ(commands_->get("check_foo_alias"), plugin_a_);
  EXPECT_EQ(commands_->describe("check_foo_alias").plugin_id, 1u);
}

TEST_F(CommandsTest, UnregisterCommand) {
  commands_->register_command(1, "check_foo", "foo check");
  commands_->unregister_command(1, "check_foo");
  EXPECT_EQ(commands_->get("check_foo"), nullptr);
  EXPECT_EQ(commands_->describe("check_foo").description, "Command not found: check_foo");
}

TEST_F(CommandsTest, UnregisterUnknownCommandIsANoOp) {
  commands_->register_command(1, "check_foo", "foo check");
  // Never registered: must not crash (used to erase(end())) and must not
  // disturb what is registered.
  commands_->unregister_command(1, "check_never_registered");
  EXPECT_EQ(commands_->get("check_foo"), plugin_a_);
}

TEST_F(CommandsTest, UnregisterTwiceIsANoOp) {
  commands_->register_command(1, "check_foo", "foo check");
  commands_->unregister_command(1, "check_foo");
  commands_->unregister_command(1, "check_foo");
  EXPECT_EQ(commands_->get("check_foo"), nullptr);
}

TEST_F(CommandsTest, UnregisterLeavesCommandTakenOverByAnotherPlugin) {
  commands_->register_command(1, "check_shared", "owned by A");
  // Plugin B takes over the same name (register_command allows this).
  commands_->register_command(2, "check_shared", "owned by B");
  // When A unregisters, B's registration must survive.
  commands_->unregister_command(1, "check_shared");
  EXPECT_EQ(commands_->get("check_shared"), plugin_b_);
  EXPECT_EQ(commands_->describe("check_shared").plugin_id, 2u);
  EXPECT_EQ(commands_->describe("check_shared").description, "owned by B");
}

TEST_F(CommandsTest, RemovePluginDropsItsCommandsAndAliases) {
  commands_->register_command(1, "check_cmd", "a command");
  commands_->register_alias(1, "check_als", "an alias");
  commands_->remove_plugin(1);
  EXPECT_EQ(commands_->get("check_cmd"), nullptr);
  EXPECT_EQ(commands_->get("check_als"), nullptr);
  EXPECT_EQ(commands_->describe("check_cmd").description, "Command not found: check_cmd");
  EXPECT_EQ(commands_->describe("check_als").description, "Command not found: check_als");
}

TEST_F(CommandsTest, RemovePluginKeepsDescriptionTakenOverViaAlias) {
  commands_->register_command(1, "check_shared", "owned by A");
  // B registers an alias with the same name, taking over the description.
  commands_->register_alias(2, "check_shared", "owned by B");
  commands_->remove_plugin(1);
  // A's command is gone but the name still resolves through B's alias,
  // and B's description must not have been deleted with A.
  EXPECT_EQ(commands_->get("check_shared"), plugin_b_);
  EXPECT_EQ(commands_->describe("check_shared").plugin_id, 2u);
  EXPECT_EQ(commands_->describe("check_shared").description, "owned by B");
}

TEST_F(CommandsTest, RemovePluginAliasKeepsDescriptionTakenOverViaCommand) {
  commands_->register_alias(1, "check_shared", "owned by A");
  // B registers a command with the same name, taking over the description.
  commands_->register_command(2, "check_shared", "owned by B");
  commands_->remove_plugin(1);
  EXPECT_EQ(commands_->get("check_shared"), plugin_b_);
  EXPECT_EQ(commands_->describe("check_shared").plugin_id, 2u);
  EXPECT_EQ(commands_->describe("check_shared").description, "owned by B");
}

TEST_F(CommandsTest, RemoveAllClearsEverything) {
  commands_->register_command(1, "check_cmd", "a command");
  commands_->register_alias(2, "check_als", "an alias");
  commands_->remove_all();
  EXPECT_EQ(commands_->get("check_cmd"), nullptr);
  EXPECT_EQ(commands_->get("check_als"), nullptr);
}
