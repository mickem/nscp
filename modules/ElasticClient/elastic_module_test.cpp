// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Unit tests for the ElasticClient module class - the settings it registers
// and reads in loadModuleEx(), the event subscription it takes out, and the
// guard that keeps the submit paths quiet until the module has been started.
// The bulk payload format lives in elastic_bulk and has its own tests.
//
// This module carries credentials (password, api key) and TLS settings, so
// their defaults and their sensitive flags are pinned here: the flag is what
// makes the core redact the value in the REST settings API, and a default that
// silently loosens TLS is a security change.

#include "ElasticClient.h"

#include <gtest/gtest.h>

#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/test_helpers.hpp>
#include <string>

// Test binaries have no generated module glue, so the plugin singleton
// (normally provided by NSC_WRAP_DLL()) must be defined here.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

namespace {

class ElasticModule : public ::testing::Test {
 protected:
  nscapi::test_helpers::stub_core &core() { return nscapi::test_helpers::stub_core::instance(); }

  void SetUp() override {
    core().reset();
    module_.set_id(42);
  }
  void TearDown() override { core().reset(); }

  bool load(const NSCAPI::moduleLoadMode mode = NSCAPI::dontStart) { return module_.loadModuleEx("", mode); }

  ElasticClient module_;
};

}  // namespace

// ============================================================================
// The settings contract
// ============================================================================

TEST_F(ElasticModule, LoadRegistersItsKeysWithTheDocumentedDefaults) {
  ASSERT_TRUE(load());

  EXPECT_EQ(core().default_for("hostname"), "auto");
  EXPECT_EQ(core().default_for("events"), "eventlog:*,logfile:*");
  EXPECT_EQ(core().default_for("timeout"), "30");
  EXPECT_EQ(core().default_for("event index"), "nsclient_event-%(date)");
  EXPECT_EQ(core().default_for("metrics index"), "nsclient_metrics-%(date)");
  EXPECT_EQ(core().default_for("nsclient log index"), "nsclient_log-%(date)");
}

// Mapping types were removed in Elasticsearch 8, which rejects a request that
// carries one - so the types must default to unset.
TEST_F(ElasticModule, MappingTypesDefaultToUnset) {
  ASSERT_TRUE(load());

  EXPECT_EQ(core().default_for("event type"), "");
  EXPECT_EQ(core().default_for("metrics type"), "");
  EXPECT_EQ(core().default_for("nsclient log type"), "");
}

TEST_F(ElasticModule, TlsDefaultsAreTheHardenedOnes) {
  ASSERT_TRUE(load());

  EXPECT_EQ(core().default_for("tls version"), "1.2+");
  EXPECT_EQ(core().default_for("verify mode"), "peer");
  EXPECT_EQ(core().default_for("ca"), "${ca-path}");
}

TEST_F(ElasticModule, CredentialsAreRegisteredAsSensitive) {
  ASSERT_TRUE(load());

  EXPECT_TRUE(core().is_sensitive("password")) << "password is not redacted by the settings API";
  EXPECT_TRUE(core().is_sensitive("api key")) << "api key is not redacted by the settings API";
  EXPECT_FALSE(core().is_sensitive("user"));
}

// ============================================================================
// The event subscription
// ============================================================================

TEST_F(ElasticModule, LoadSubscribesToTheDefaultEvents) {
  ASSERT_TRUE(load());
  EXPECT_TRUE(core().has_event("eventlog:*,logfile:*"));
}

TEST_F(ElasticModule, ConfiguredEventsAreSubscribedInstead) {
  core().set_setting("events", "logfile:mylog");

  ASSERT_TRUE(load());
  EXPECT_TRUE(core().has_event("logfile:mylog"));
  EXPECT_FALSE(core().has_event("eventlog:*,logfile:*"));
}

// ============================================================================
// Start mode and the submit guard
// ============================================================================

TEST_F(ElasticModule, LoadsInEveryMode) {
  EXPECT_TRUE(load(NSCAPI::dontStart));
  EXPECT_TRUE(load(NSCAPI::normalStart));
  EXPECT_TRUE(load(NSCAPI::reloadStart));
}

// Not started: every submit path must return without touching the network.
TEST_F(ElasticModule, SubmitPathsAreQuietBeforeStart) {
  ASSERT_TRUE(load(NSCAPI::dontStart));

  PB::Metrics::MetricsMessage metrics;
  EXPECT_NO_THROW(module_.submitMetrics(metrics));

  PB::Log::LogEntry::Entry entry;
  entry.set_message("hello");
  EXPECT_NO_THROW(module_.handleLogMessage(entry));
}

// Started but with no address configured: the module must report the bad
// address and drop the payload rather than throw out of the callback.
TEST_F(ElasticModule, SubmitWithoutAnAddressIsHandled) {
  ASSERT_TRUE(load(NSCAPI::reloadStart));

  PB::Log::LogEntry::Entry entry;
  entry.set_message("hello");
  EXPECT_NO_THROW(module_.handleLogMessage(entry));
}

TEST_F(ElasticModule, UnloadStopsTheSubmitPaths) {
  ASSERT_TRUE(load(NSCAPI::reloadStart));
  EXPECT_TRUE(module_.unloadModule());

  PB::Metrics::MetricsMessage metrics;
  EXPECT_NO_THROW(module_.submitMetrics(metrics));
}

TEST_F(ElasticModule, UnloadWithoutLoadIsSafe) { EXPECT_TRUE(module_.unloadModule()); }
