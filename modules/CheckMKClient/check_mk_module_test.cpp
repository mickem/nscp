// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Unit tests for the CheckMKClient module class - the settings it registers
// and reads in loadModuleEx(), and the target/handler sections it turns into
// client targets and relay commands. The check_mk wire format and the option
// parsing live in check_mk_handler.hpp and have their own tests.

#include "CheckMKClient.h"

#include <gtest/gtest.h>

#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/test_helpers.hpp>
#include <string>

// Test binaries have no generated module glue, so the plugin singleton
// (normally provided by NSC_WRAP_DLL()) must be defined here.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

namespace {

class CheckMkModule : public ::testing::Test {
 protected:
  nscapi::test_helpers::stub_core &core() { return nscapi::test_helpers::stub_core::instance(); }

  void SetUp() override {
    core().reset();
    module_.set_id(42);
  }
  void TearDown() override { core().reset(); }

  bool load(const std::string &alias = "") { return module_.loadModuleEx(alias, NSCAPI::dontStart); }

  // The settings root differs per module, so a section is matched by its tail
  // rather than its full path.
  bool has_section(const std::string &name) const {
    for (const std::string &path : nscapi::test_helpers::stub_core::instance().registered_paths()) {
      if (path.size() > name.size() && path.compare(path.size() - name.size(), name.size(), name) == 0) return true;
    }
    return false;
  }

  CheckMKClient module_;
};

}  // namespace

TEST_F(CheckMkModule, LoadRegistersItsKeysWithTheDocumentedDefaults) {
  ASSERT_TRUE(load());
  EXPECT_EQ(core().default_for("channel"), "CheckMK");
}

TEST_F(CheckMkModule, LoadRegistersItsSettingsSections) {
  ASSERT_TRUE(load());
  EXPECT_TRUE(has_section("targets")) << "no targets section registered";
  EXPECT_TRUE(has_section("handlers")) << "no handlers section registered";
}

TEST_F(CheckMkModule, LoadRegistersTheDefaultChannel) {
  ASSERT_TRUE(load());
  EXPECT_TRUE(core().has_channel("CheckMK"));
}

TEST_F(CheckMkModule, ConfiguredChannelIsRegisteredInsteadOfTheDefault) {
  core().set_setting("channel", "MY_CHANNEL");

  ASSERT_TRUE(load());
  EXPECT_TRUE(core().has_channel("MY_CHANNEL"));
  EXPECT_FALSE(core().has_channel("CheckMK"));
}

TEST_F(CheckMkModule, HandlerKeysAreRegisteredAsCommands) {
  core().set_keys("handlers", {{"submit_check_mk", "host=127.0.0.1"}});

  ASSERT_TRUE(load());
  EXPECT_TRUE(core().has_command("submit_check_mk")) << "handler key was not registered as a command";
}

TEST_F(CheckMkModule, EmptyHandlerKeyRegistersNoCommand) {
  core().set_keys("handlers", {{"", ""}});

  ASSERT_TRUE(load());
  EXPECT_FALSE(core().has_command(""));
}

TEST_F(CheckMkModule, TargetKeysAreAccepted) {
  core().set_keys("targets", {{"default", "host=127.0.0.1,port=6556"}});

  EXPECT_TRUE(load());
}

// One unparseable target in nsclient.ini must be reported and skipped, not
// take the whole module offline.
TEST_F(CheckMkModule, UnparseableTargetDoesNotFailTheLoad) {
  core().set_keys("targets", {{"broken", "this is not a target definition"}});

  EXPECT_TRUE(load());
}

TEST_F(CheckMkModule, UnloadWithoutLoadIsSafe) { EXPECT_TRUE(module_.unloadModule()); }

TEST_F(CheckMkModule, UnloadAfterLoadIsIdempotent) {
  ASSERT_TRUE(load());
  EXPECT_TRUE(module_.unloadModule());
  EXPECT_TRUE(module_.unloadModule());
}

TEST_F(CheckMkModule, CommandLineExecOnlyHandlesItsOwnTargetMode) {
  ASSERT_TRUE(load());
  PB::Commands::ExecuteRequestMessage request;
  PB::Commands::ExecuteResponseMessage response;

  EXPECT_FALSE(module_.commandLineExec(NSCAPI::target_any, request, response));
}
