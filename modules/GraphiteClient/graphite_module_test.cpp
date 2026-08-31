// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Unit tests for the GraphiteClient module class - the settings it registers
// and reads in loadModuleEx(), and the target/handler sections it turns into
// client targets and relay commands. The wire format and the option parsing
// live in graphite_client.hpp / graphite_handler.hpp and have their own tests;
// what is covered here is only the plugin shell around them.
//
// The settings keys and their defaults are part of the module's contract with
// every existing nsclient.ini, so a rename or a changed default is a breaking
// change: asserting on them here is what makes that visible in review.

#include "GraphiteClient.h"

#include <gtest/gtest.h>

#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/test_helpers.hpp>
#include <string>

// Test binaries have no generated module glue, so the plugin singleton
// (normally provided by NSC_WRAP_DLL()) must be defined here.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

namespace {

class GraphiteModule : public ::testing::Test {
 protected:
  nscapi::test_helpers::stub_core &core() { return nscapi::test_helpers::stub_core::instance(); }

  void SetUp() override {
    core().reset();
    module_.set_id(42);
  }
  void TearDown() override { core().reset(); }

  // dontStart: nothing in this module starts a thread, but every module test
  // loads the same way so the pattern stays copyable.
  bool load(const std::string &alias = "") { return module_.loadModuleEx(alias, NSCAPI::dontStart); }

  GraphiteClient module_;
};

}  // namespace

// ============================================================================
// The settings contract
// ============================================================================

TEST_F(GraphiteModule, LoadRegistersItsKeysWithTheDocumentedDefaults) {
  ASSERT_TRUE(load());

  EXPECT_EQ(core().default_for("hostname"), "auto");
  EXPECT_EQ(core().default_for("channel"), "GRAPHITE");
}

TEST_F(GraphiteModule, LoadRegistersTheTargetAndHandlerSections) {
  ASSERT_TRUE(load());

  bool targets = false, handlers = false;
  for (const std::string &path : core().registered_paths()) {
    if (path.size() >= 7 && path.compare(path.size() - 7, 7, "targets") == 0) targets = true;
    if (path.size() >= 8 && path.compare(path.size() - 8, 8, "handlers") == 0) handlers = true;
  }
  EXPECT_TRUE(targets) << "no targets section registered";
  EXPECT_TRUE(handlers) << "no handlers section registered";
}

// ============================================================================
// The channel: what the core routes submissions on
// ============================================================================

TEST_F(GraphiteModule, LoadRegistersTheDefaultChannel) {
  ASSERT_TRUE(load());
  EXPECT_TRUE(core().has_channel("GRAPHITE")) << "GRAPHITE channel not registered";
}

TEST_F(GraphiteModule, ConfiguredChannelIsRegisteredInsteadOfTheDefault) {
  core().set_setting("channel", "MY_GRAPHITE");

  ASSERT_TRUE(load());
  EXPECT_TRUE(core().has_channel("MY_GRAPHITE"));
  EXPECT_FALSE(core().has_channel("GRAPHITE"));
}

// ============================================================================
// The handlers section becomes relay commands
// ============================================================================

TEST_F(GraphiteModule, HandlerKeysAreRegisteredAsCommands) {
  core().set_keys("handlers", {{"submit_graphite", "host=127.0.0.1"}});

  ASSERT_TRUE(load());
  EXPECT_TRUE(core().has_command("submit_graphite")) << "handler key was not registered as a command";
}

// A handler the client cannot make a command out of must not be registered -
// and must not take the module load down with it.
TEST_F(GraphiteModule, EmptyHandlerKeyRegistersNoCommand) {
  core().set_keys("handlers", {{"", ""}});

  ASSERT_TRUE(load());
  EXPECT_FALSE(core().has_command(""));
}

// ============================================================================
// The targets section
// ============================================================================

TEST_F(GraphiteModule, TargetKeysAreAccepted) {
  core().set_keys("targets", {{"default", "host=127.0.0.1,port=2003"}});

  // A target that does not parse must be reported and skipped, not thrown out
  // of loadModuleEx - one bad target in nsclient.ini would otherwise take the
  // whole module offline.
  EXPECT_TRUE(load());
}

TEST_F(GraphiteModule, UnparseableTargetDoesNotFailTheLoad) {
  core().set_keys("targets", {{"broken", "this is not a target definition"}});

  EXPECT_TRUE(load());
}

// ============================================================================
// Lifecycle and dispatch
// ============================================================================

TEST_F(GraphiteModule, UnloadWithoutLoadIsSafe) { EXPECT_TRUE(module_.unloadModule()); }

TEST_F(GraphiteModule, UnloadAfterLoadClearsTheClient) {
  ASSERT_TRUE(load());
  EXPECT_TRUE(module_.unloadModule());
  EXPECT_TRUE(module_.unloadModule());
}

TEST_F(GraphiteModule, CommandLineExecOnlyHandlesItsOwnTargetMode) {
  ASSERT_TRUE(load());
  PB::Commands::ExecuteRequestMessage request;
  PB::Commands::ExecuteResponseMessage response;

  EXPECT_FALSE(module_.commandLineExec(NSCAPI::target_any, request, response));
}
