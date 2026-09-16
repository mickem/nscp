// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Unit tests for the GearmanClient module class - the plugin shell around the
// two halves, not the halves themselves. The binary protocol, the envelope,
// the job and result text and the worker loop each have their own test; what
// is covered here is the settings the module registers, the channel the submit
// half listens on, and the rule that decides whether a worker is started.
//
// The settings keys and their defaults are part of the module's contract with
// every existing nsclient.ini, so a rename or a changed default is a breaking
// change: asserting on them here is what makes that visible in review.

#include <gtest/gtest.h>

#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/test_helpers.hpp>
#include <string>

#include "GearmanClient.h"

// Test binaries have no generated module glue, so the plugin singleton
// (normally provided by NSC_WRAP_DLL()) must be defined here.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

namespace {

class GearmanModule : public ::testing::Test {
 protected:
  nscapi::test_helpers::stub_core &core() { return nscapi::test_helpers::stub_core::instance(); }

  void SetUp() override {
    core().reset();
    module_.set_id(42);
  }
  void TearDown() override { core().reset(); }

  /**
   * `dontStart` reads every setting and registers everything, but never opens
   * a socket: the worker pool is only started for normalStart/reloadStart.
   */
  bool load(const std::string &alias = "") { return module_.loadModuleEx(alias, NSCAPI::dontStart); }

  // The settings root differs per module, so a section is matched by its tail
  // rather than its full path.
  bool has_section(const std::string &name) const {
    for (const std::string &path : nscapi::test_helpers::stub_core::instance().registered_paths()) {
      if (path.size() > name.size() && path.compare(path.size() - name.size(), name.size(), name) == 0) return true;
    }
    return false;
  }

  GearmanClient module_;
};

}  // namespace

// ============================================================================
// The settings contract
// ============================================================================

TEST_F(GearmanModule, LoadRegistersTheWorkerKeysWithTheDocumentedDefaults) {
  ASSERT_TRUE(load());

  EXPECT_EQ(core().default_for("server"), "");
  EXPECT_EQ(core().default_for("mode"), "agent");
  EXPECT_EQ(core().default_for("encryption"), "true");
  EXPECT_EQ(core().default_for("insecure"), "false");
  EXPECT_EQ(core().default_for("allow shared queues"), "false");
  EXPECT_EQ(core().default_for("workers"), "2");
  EXPECT_EQ(core().default_for("timeout return"), "2");
  EXPECT_EQ(core().default_for("max age"), "0");
  // On, unlike NRPE: the core has already expanded $ARGn$, so a job that may
  // carry no arguments can only run bare commands.
  EXPECT_EQ(core().default_for("allow arguments"), "true");
  EXPECT_EQ(core().default_for("allow nasty characters"), "false");
}

TEST_F(GearmanModule, LoadRegistersTheSubmitChannelKeysWithTheDocumentedDefaults) {
  ASSERT_TRUE(load());

  EXPECT_EQ(core().default_for("channel"), "GEARMAN");
  EXPECT_EQ(core().default_for("hostname"), "auto");
}

TEST_F(GearmanModule, LoadRegistersTheSubmitChannelSections) {
  ASSERT_TRUE(load());

  EXPECT_TRUE(has_section("targets")) << "no targets section registered";
  EXPECT_TRUE(has_section("handlers")) << "no handlers section registered";
}

// ============================================================================
// The channel: what the core routes submissions on
// ============================================================================

TEST_F(GearmanModule, LoadRegistersTheDefaultChannel) {
  ASSERT_TRUE(load());
  EXPECT_TRUE(core().has_channel("GEARMAN")) << "GEARMAN channel not registered";
}

TEST_F(GearmanModule, TheChannelFollowsTheSetting) {
  core().set_setting("/settings/gearman/client", "channel", "RESULTS");
  ASSERT_TRUE(load());
  EXPECT_TRUE(core().has_channel("RESULTS"));
}

// ============================================================================
// The two halves are independent deployments
// ============================================================================

TEST_F(GearmanModule, AnEmptyWorkerSectionStillLoadsForTheSubmitChannel) {
  // An installation that replaced NSCA with Mod-Gearman configures the client
  // half only. That is a deployment, not a misconfiguration, so the module
  // loads and its channel works.
  ASSERT_TRUE(load());
  EXPECT_TRUE(core().has_channel("GEARMAN"));
}

TEST_F(GearmanModule, AWorkerWithNoQueueDoesNotTakeTheSubmitChannelDownWithIt) {
  // A server but no hostgroup: a worker that would never be handed a check.
  // The reason is logged as an error, but the submit half is a separate
  // deployment and keeps working.
  core().set_setting("/settings/gearman/worker", "server", "127.0.0.1:4730");
  core().set_setting("/settings/gearman/worker", "key", "secret");

  EXPECT_TRUE(load());
  EXPECT_TRUE(core().has_channel("GEARMAN"));
}

TEST_F(GearmanModule, AWorkerWithNoKeyDoesNotTakeTheSubmitChannelDownWithIt) {
  // Encryption is on by default and the key is the only thing separating a
  // check the core scheduled from one anybody who can reach gearmand made up.
  core().set_setting("/settings/gearman/worker", "server", "127.0.0.1:4730");
  core().set_setting("/settings/gearman/worker", "hostgroups", "windows");

  EXPECT_TRUE(load());
  EXPECT_TRUE(core().has_channel("GEARMAN"));
}

// ============================================================================
// Reload
// ============================================================================

TEST_F(GearmanModule, ReloadingDoesNotDuplicateTheChannel) {
  // loadModuleEx is called again on the live module by a settings reload. The
  // targets and relay commands are rebuilt from scratch each time, which is
  // what keeps the settings callbacks from appending the same entries twice.
  ASSERT_TRUE(load());
  ASSERT_TRUE(module_.loadModuleEx("", NSCAPI::dontStart));
  EXPECT_TRUE(core().has_channel("GEARMAN"));
  EXPECT_TRUE(module_.unloadModule());
}
