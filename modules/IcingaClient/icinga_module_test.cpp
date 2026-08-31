// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Unit tests for the IcingaClient module class - the settings it registers and
// reads in loadModuleEx(), and the targets sections it turns into
// client targets. The wire format and the option parsing live in the
// module's other translation units and have their own tests; what is covered
// here is only the plugin shell around them.
//
// The settings keys and their defaults are part of the module's contract with
// every existing nsclient.ini, so a rename or a changed default is a breaking
// change: asserting on them here is what makes that visible in review.

#include "IcingaClient.h"

#include <gtest/gtest.h>

#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/test_helpers.hpp>
#include <string>

// Test binaries have no generated module glue, so the plugin singleton
// (normally provided by NSC_WRAP_DLL()) must be defined here.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

namespace {

class IcingaModule : public ::testing::Test {
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

  IcingaClient module_;
};

}  // namespace

// ============================================================================
// The settings contract
// ============================================================================

TEST_F(IcingaModule, LoadRegistersItsKeysWithTheDocumentedDefaults) {
  ASSERT_TRUE(load());

  EXPECT_EQ(core().default_for("hostname"), "auto");
  EXPECT_EQ(core().default_for("channel"), "ICINGA");
}

TEST_F(IcingaModule, LoadRegistersItsSettingsSections) {
  ASSERT_TRUE(load());

  EXPECT_TRUE(has_section("targets")) << "no targets section registered";
}

// ============================================================================
// The channel: what the core routes submissions on
// ============================================================================

TEST_F(IcingaModule, LoadRegistersTheDefaultChannel) {
  ASSERT_TRUE(load());
  EXPECT_TRUE(core().has_channel("ICINGA")) << "ICINGA channel not registered";
}

TEST_F(IcingaModule, ConfiguredChannelIsRegisteredInsteadOfTheDefault) {
  core().set_setting("channel", "MY_CHANNEL");

  ASSERT_TRUE(load());
  EXPECT_TRUE(core().has_channel("MY_CHANNEL"));
  EXPECT_FALSE(core().has_channel("ICINGA"));
}

// ============================================================================
// The targets section
// ============================================================================

TEST_F(IcingaModule, TargetKeysAreAccepted) {
  core().set_keys("targets", {{"default", "address=https://localhost:5665"}});

  EXPECT_TRUE(load());
}

// One unparseable target in nsclient.ini must be reported and skipped, not
// take the whole module offline.
TEST_F(IcingaModule, UnparseableTargetDoesNotFailTheLoad) {
  core().set_keys("targets", {{"broken", "this is not a target definition"}});

  EXPECT_TRUE(load());
}

// ============================================================================
// Lifecycle and dispatch
// ============================================================================

TEST_F(IcingaModule, UnloadWithoutLoadIsSafe) { EXPECT_TRUE(module_.unloadModule()); }

TEST_F(IcingaModule, UnloadAfterLoadIsIdempotent) {
  ASSERT_TRUE(load());
  EXPECT_TRUE(module_.unloadModule());
  EXPECT_TRUE(module_.unloadModule());
}

TEST_F(IcingaModule, CommandLineExecOnlyHandlesItsOwnTargetMode) {
  ASSERT_TRUE(load());
  PB::Commands::ExecuteRequestMessage request;
  PB::Commands::ExecuteResponseMessage response;

  EXPECT_FALSE(module_.commandLineExec(NSCAPI::target_any, request, response));
}
