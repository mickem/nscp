// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Unit tests for the CommandClient module class (the `nscp test` console):
// the settings it registers, and the requests commandLineExec must answer or
// decline without starting the console.
//
// The console itself - the input thread, the signal handler, the line
// editor - only starts for a request aimed at this module with no command,
// and then reads stdin until `exit`; it is not something a unit test can
// drive deterministically, so no test here makes that request. What is pinned
// instead is that every *other* request is declined: an untargeted one used
// to start a second console on the caller's thread and never return.

#include "CommandClient.h"

#include <gtest/gtest.h>

#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/test_helpers.hpp>
#include <string>

// Test binaries have no generated module glue, so the plugin singleton
// (normally provided by NSC_WRAP_DLL()) must be defined here.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

namespace {

const char *const kRoot = "/settings/cli";

class CommandClientModule : public ::testing::Test {
 protected:
  nscapi::test_helpers::stub_core &core() { return nscapi::test_helpers::stub_core::instance(); }

  void SetUp() override {
    core().reset();
    module_.set_id(42);
  }
  void TearDown() override {
    module_.unloadModule();
    core().reset();
  }

  bool load(const NSCAPI::moduleLoadMode mode = NSCAPI::dontStart) { return module_.loadModuleEx("", mode); }

  bool exec(const int target, const std::string &command, PB::Commands::ExecuteResponseMessage::Response *response) {
    PB::Commands::ExecuteRequestMessage request_message;
    PB::Commands::ExecuteRequestMessage::Request request;
    request.set_command(command);
    return module_.commandLineExec(target, request, response, request_message);
  }

  CommandClient module_;
};

}  // namespace

// ============================================================================
// The settings contract
// ============================================================================

TEST_F(CommandClientModule, LoadRegistersItsKeysWithTheDocumentedDefaults) {
  ASSERT_TRUE(load());

  EXPECT_EQ(core().default_for(kRoot, "history file"), "") << "empty means the per-user default location";
  EXPECT_EQ(core().default_for(kRoot, "history size"), "500");
  EXPECT_EQ(core().default_for(kRoot, "color"), "true");
}

// All three only matter to someone at the prompt, so they are tucked away.
TEST_F(CommandClientModule, TheKeysAreAdvanced) {
  ASSERT_TRUE(load());

  for (const nscapi::test_helpers::registered_key &k : core().registered_keys()) {
    if (k.path == kRoot) EXPECT_TRUE(k.advanced) << k.key;
  }
}

TEST_F(CommandClientModule, LoadRegistersTheSection) {
  ASSERT_TRUE(load());

  const std::vector<std::string> &paths = core().registered_paths();
  EXPECT_NE(std::find(paths.begin(), paths.end(), kRoot), paths.end());
}

// A reload re-enters loadModuleEx while the prompt may be up; it must keep the
// client the prompt's hooks reach, and register the settings again.
TEST_F(CommandClientModule, ReloadIsSafe) {
  ASSERT_TRUE(load());
  core().reset();
  ASSERT_TRUE(load(NSCAPI::reloadStart));
  EXPECT_EQ(core().default_for(kRoot, "history size"), "500");
}

// ============================================================================
// commandLineExec
// ============================================================================

TEST_F(CommandClientModule, ExitOutsideTheConsoleIsRefused) {
  ASSERT_TRUE(load());

  PB::Commands::ExecuteResponseMessage::Response response;
  EXPECT_TRUE(exec(NSCAPI::target_module, "exit", &response));
  EXPECT_NE(response.result(), PB::Common::ResultCode::OK);
  EXPECT_EQ(response.message(), "Command client is not running (this only works in test mode)!");
}

// `all`, `*`, `any` and a script's Core.simple_exec are untargeted: none of
// them may start a console on the caller's thread.
TEST_F(CommandClientModule, AnUntargetedRequestIsDeclined) {
  ASSERT_TRUE(load());

  PB::Commands::ExecuteResponseMessage::Response response;
  EXPECT_FALSE(exec(NSCAPI::target_any, "", &response));
  EXPECT_TRUE(response.message().empty());
}

TEST_F(CommandClientModule, ATargetedRequestForAnotherCommandIsDeclined) {
  ASSERT_TRUE(load());

  PB::Commands::ExecuteResponseMessage::Response response;
  EXPECT_FALSE(exec(NSCAPI::target_module, "settings", &response));
  EXPECT_FALSE(exec(NSCAPI::target_any, "settings", &response));
}

// ============================================================================
// The callbacks the core drives
// ============================================================================

// Log messages are only rendered while the prompt owns the console; without
// one the core prints them itself and this must be a harmless no-op.
TEST_F(CommandClientModule, LogMessagesWithoutAPromptAreIgnored) {
  ASSERT_TRUE(load());

  const std::size_t before = core().logged().size();
  nscapi::test_helpers::stdout_capture out;
  PB::Log::LogEntry::Entry entry;
  entry.set_level(PB::Log::LogEntry_Entry_Level_LOG_ERROR);
  entry.set_message("something happened");
  module_.handleLogMessage(entry);

  EXPECT_EQ(out.str(), "") << "rendered without a prompt: the core would print it twice";
  EXPECT_EQ(core().logged().size(), before) << "echoed back into the log";
}

TEST_F(CommandClientModule, UnloadIsAlwaysSafe) {
  EXPECT_TRUE(module_.unloadModule());
  ASSERT_TRUE(load());
  EXPECT_TRUE(module_.unloadModule());
}
