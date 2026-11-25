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

#include <boost/filesystem.hpp>
#include <fstream>
#include <map>
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

// ============================================================================
// Which worker configurations are refused, and why
// ============================================================================
//
// Each of these loads with dontStart, which builds and validates the worker
// configuration exactly as a start does and then stops short of the pool. The
// only observable effect of a refusal is the line it logs.

namespace {

const char *const kWorker = "/settings/gearman/worker";

class GearmanWorkerConfig : public GearmanModule {
 protected:
  // A complete, valid worker configuration, which each test then breaks.
  void valid() {
    core().set_setting(kWorker, "server", "127.0.0.1:4730");
    core().set_setting(kWorker, "key", "secret");
    core().set_setting(kWorker, "hostgroups", "windows");
  }
  bool logged(const std::string &needle) const { return nscapi::test_helpers::stub_core::instance().has_logged(needle) || out_.contains(needle); }

  nscapi::test_helpers::stdout_capture out_;
};

}  // namespace

TEST_F(GearmanWorkerConfig, AValidConfigurationIsAccepted) {
  valid();
  ASSERT_TRUE(load());
  EXPECT_FALSE(logged("gearman: no job server")) << out_.str();
  EXPECT_FALSE(logged("Refusing")) << out_.str();
  EXPECT_FALSE(logged("no queue to answer"));
}

TEST_F(GearmanWorkerConfig, QueuesWithoutAServerAreRefused) {
  core().set_setting(kWorker, "hostgroups", "windows");
  ASSERT_TRUE(load());
  EXPECT_TRUE(logged("gearman: no job server configured. Set /settings/gearman/worker/server"));
}

TEST_F(GearmanWorkerConfig, AnUnreadableServerListIsRefused) {
  valid();
  core().set_setting(kWorker, "server", "[::1");
  ASSERT_TRUE(load());
  EXPECT_TRUE(logged("gearman: could not read the server list: Unterminated IPv6 address in server list: [::1"));
}

TEST_F(GearmanWorkerConfig, AnEmptyServerListIsRefused) {
  valid();
  core().set_setting(kWorker, "server", " , ");
  ASSERT_TRUE(load());
  EXPECT_TRUE(logged("gearman: the server list is empty."));
}

TEST_F(GearmanWorkerConfig, EncryptionWithoutAKeyIsRefused) {
  valid();
  core().set_setting(kWorker, "key", "");
  ASSERT_TRUE(load());
  EXPECT_TRUE(logged("gearman: encryption is on but no key is set."));
}

TEST_F(GearmanWorkerConfig, AnUnreadableKeyFileIsRefused) {
  valid();
  core().set_setting(kWorker, "key", "");
  core().set_setting(kWorker, "key file", "/nonexistent-nscp-test/gearman.key");
  ASSERT_TRUE(load());
  EXPECT_TRUE(logged("gearman: could not read the key file: /nonexistent-nscp-test/gearman.key"));
}

TEST_F(GearmanWorkerConfig, AKeyFileSuppliesTheKey) {
  const boost::filesystem::path file = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("gearman-key-%%%%-%%%%");
  std::ofstream(file.string().c_str()) << "secret-from-file  \n";
#ifndef WIN32
  boost::filesystem::permissions(file, boost::filesystem::owner_read | boost::filesystem::owner_write);
#endif
  valid();
  core().set_setting(kWorker, "key", "");
  core().set_setting(kWorker, "key file", file.string());

  ASSERT_TRUE(load());
  EXPECT_FALSE(logged("no key is set"));
  EXPECT_FALSE(logged("readable by more than its owner"));
  boost::system::error_code ec;
  boost::filesystem::remove(file, ec);
}

#ifndef WIN32
// Anyone who can read the key can queue a check on every queue this agent
// answers, so a world-readable key file is called out.
TEST_F(GearmanWorkerConfig, AKeyFileReadableByOthersIsReported) {
  const boost::filesystem::path file = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("gearman-key-%%%%-%%%%");
  std::ofstream(file.string().c_str()) << "secret\n";
  boost::filesystem::permissions(file, boost::filesystem::owner_read | boost::filesystem::owner_write | boost::filesystem::others_read);
  valid();
  core().set_setting(kWorker, "key", "");
  core().set_setting(kWorker, "key file", file.string());

  ASSERT_TRUE(load());
  EXPECT_TRUE(logged("is readable by more than its owner"));
  boost::system::error_code ec;
  boost::filesystem::remove(file, ec);
}
#endif

TEST_F(GearmanWorkerConfig, UnencryptedWithoutInsecureIsRefused) {
  valid();
  core().set_setting(kWorker, "encryption", "false");
  ASSERT_TRUE(load());
  EXPECT_TRUE(logged("gearman: encryption is off but 'insecure' is not set."));
}

TEST_F(GearmanWorkerConfig, UnencryptedWithInsecureIsAcceptedLoudly) {
  valid();
  core().set_setting(kWorker, "encryption", "false");
  core().set_setting(kWorker, "insecure", "true");
  ASSERT_TRUE(load());
  EXPECT_TRUE(logged("gearman: running with encryption disabled."));
  EXPECT_FALSE(logged("'insecure' is not set"));
}

TEST_F(GearmanWorkerConfig, AServerWithNoQueueIsRefused) {
  valid();
  core().set_setting(kWorker, "hostgroups", "");
  ASSERT_TRUE(load());
  EXPECT_TRUE(logged("gearman: no queue to answer."));
}

// The generic queues alone are a queue.
TEST_F(GearmanWorkerConfig, SharedQueuesAreAQueue) {
  valid();
  core().set_setting(kWorker, "hostgroups", "");
  core().set_setting(kWorker, "allow shared queues", "true");
  ASSERT_TRUE(load());
  EXPECT_FALSE(logged("no queue to answer"));
}

TEST_F(GearmanWorkerConfig, ProxyModeSaysWhatItRuns) {
  valid();
  core().set_setting(kWorker, "mode", "Proxy");
  core().set_setting(kWorker, "servicegroups", "db");
  core().set_setting(kWorker, "host names", "other-name");
  ASSERT_TRUE(load());
  EXPECT_TRUE(logged("gearman: running in proxy mode: every check on hostgroup_windows, servicegroup_db is executed here"));
  EXPECT_TRUE(logged("'host names' is set but has no effect in proxy mode"));
}

TEST_F(GearmanWorkerConfig, AnUnknownModeIsRefused) {
  valid();
  core().set_setting(kWorker, "mode", "relay");
  ASSERT_TRUE(load());
  EXPECT_TRUE(logged("gearman: unknown mode 'relay'"));
}

// ============================================================================
// Metrics, and the submit half without targets
// ============================================================================

TEST_F(GearmanModule, MetricsBeforeAnyJobSayNoneHasArrived) {
  ASSERT_TRUE(load());

  PB::Metrics::MetricsMessage::Response response;
  module_.fetchMetrics(&response);
  ASSERT_EQ(response.bundles_size(), 1);
  const PB::Metrics::MetricsBundle &bundle = response.bundles(0);
  EXPECT_EQ(bundle.key(), "gearman");
  std::map<std::string, double> values;
  for (const PB::Metrics::Metric &m : bundle.value()) values[m.key()] = m.has_gauge_value() ? m.gauge_value().value() : m.counter_value().value();
  EXPECT_EQ(values["worker.jobs"], 0);
  EXPECT_EQ(values["worker.errors"], 0);
  EXPECT_EQ(values["worker.connected"], 0);
  EXPECT_EQ(values["worker.last_job_age"], -1) << "no job yet is -1, not 0";
}

TEST_F(GearmanModule, CommandLineExecOutsideTheModuleIsNotClaimed) {
  ASSERT_TRUE(load());

  PB::Commands::ExecuteRequestMessage request;
  request.add_payload()->set_command("submit_gearman");
  PB::Commands::ExecuteResponseMessage response;
  EXPECT_FALSE(module_.commandLineExec(NSCAPI::target_any, request, response));
}
