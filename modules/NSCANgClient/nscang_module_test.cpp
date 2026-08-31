// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Unit tests for the NSCANgClient module class - the settings it registers and
// reads in loadModuleEx(), and the targets and handlers sections it turns into
// client targets and relay commands. The wire format and the option parsing live in the
// module's other translation units and have their own tests; what is covered
// here is only the plugin shell around them.
//
// The settings keys and their defaults are part of the module's contract with
// every existing nsclient.ini, so a rename or a changed default is a breaking
// change: asserting on them here is what makes that visible in review.

#include "NSCANgClient.h"

#include <gtest/gtest.h>

#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/test_helpers.hpp>
#include <string>

// Test binaries have no generated module glue, so the plugin singleton
// (normally provided by NSC_WRAP_DLL()) must be defined here.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

namespace {

class NscaNgModule : public ::testing::Test {
 protected:
  nscapi::test_helpers::stub_core &core() { return nscapi::test_helpers::stub_core::instance(); }

  void SetUp() override {
    core().reset();
    module_.set_id(42);
  }
  void TearDown() override { core().reset(); }

  bool load(const std::string &alias = "") { return module_.loadModuleEx(alias, NSCAPI::dontStart); }

  // The settings root differs per module, so a section is matched by its
  // tail rather than its full path.
  bool has_section(const std::string &name) const {
    for (const std::string &path : nscapi::test_helpers::stub_core::instance().registered_paths()) {
      if (path.size() > name.size() && path.compare(path.size() - name.size(), name.size(), name) == 0) return true;
    }
    return false;
  }

  NSCANgClient module_;
};

}  // namespace

// ============================================================================
// The settings contract
// ============================================================================

TEST_F(NscaNgModule, LoadRegistersItsKeysWithTheDocumentedDefaults) {
  ASSERT_TRUE(load());

  EXPECT_EQ(core().default_for("hostname"), "auto");
  EXPECT_EQ(core().default_for("channel"), "NSCA-NG");
}

TEST_F(NscaNgModule, LoadRegistersItsSettingsSections) {
  ASSERT_TRUE(load());

  EXPECT_TRUE(has_section("targets")) << "no targets section registered";
  EXPECT_TRUE(has_section("handlers")) << "no handlers section registered";
}

// ============================================================================
// The channel: what the core routes submissions on
// ============================================================================

TEST_F(NscaNgModule, LoadRegistersTheDefaultChannel) {
  ASSERT_TRUE(load());
  EXPECT_TRUE(core().has_channel("NSCA-NG")) << "NSCA-NG channel not registered";
}

TEST_F(NscaNgModule, ConfiguredChannelIsRegisteredInsteadOfTheDefault) {
  core().set_setting("channel", "MY_CHANNEL");

  ASSERT_TRUE(load());
  EXPECT_TRUE(core().has_channel("MY_CHANNEL"));
  EXPECT_FALSE(core().has_channel("NSCA-NG"));
}

// ============================================================================
// The handlers section becomes relay commands
// ============================================================================

TEST_F(NscaNgModule, HandlerKeysAreRegisteredAsCommands) {
  core().set_keys("handlers", {{"submit_nsca_ng", "host=127.0.0.1,port=5668"}});

  ASSERT_TRUE(load());
  EXPECT_TRUE(core().has_command("submit_nsca_ng")) << "handler key was not registered as a command";
}

// A handler the client cannot make a command out of must not be registered -
// and must not take the module load down with it.
TEST_F(NscaNgModule, EmptyHandlerKeyRegistersNoCommand) {
  core().set_keys("handlers", {{"", ""}});

  ASSERT_TRUE(load());
  EXPECT_FALSE(core().has_command(""));
}

// ============================================================================
// The targets section
// ============================================================================

TEST_F(NscaNgModule, TargetKeysAreAccepted) {
  core().set_keys("targets", {{"default", "host=127.0.0.1,port=5668"}});

  EXPECT_TRUE(load());
}

// One unparseable target in nsclient.ini must be reported and skipped, not
// take the whole module offline.
TEST_F(NscaNgModule, UnparseableTargetDoesNotFailTheLoad) {
  core().set_keys("targets", {{"broken", "this is not a target definition"}});

  EXPECT_TRUE(load());
}

// ============================================================================
// Lifecycle and dispatch
// ============================================================================

TEST_F(NscaNgModule, UnloadWithoutLoadIsSafe) { EXPECT_TRUE(module_.unloadModule()); }

TEST_F(NscaNgModule, UnloadAfterLoadIsIdempotent) {
  ASSERT_TRUE(load());
  EXPECT_TRUE(module_.unloadModule());
  EXPECT_TRUE(module_.unloadModule());
}

TEST_F(NscaNgModule, CommandLineExecOnlyHandlesItsOwnTargetMode) {
  ASSERT_TRUE(load());
  PB::Commands::ExecuteRequestMessage request;
  PB::Commands::ExecuteResponseMessage response;

  EXPECT_FALSE(module_.commandLineExec(NSCAPI::target_any, request, response));
}
