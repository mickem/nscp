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

  // NSC_LOG_ERROR goes to stdout in a unit-test build, the rest to the core.
  bool logged(const std::string &needle) const { return out_.contains(needle) || nscapi::test_helpers::stub_core::instance().has_logged(needle); }

  nscapi::test_helpers::stdout_capture out_;
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

// ============================================================================
// Settings checked at load
// ============================================================================

TEST_F(ElasticModule, ANegativeTimeoutFallsBackToThirtySeconds) {
  core().set_setting("timeout", "-5");
  ASSERT_TRUE(load());
  EXPECT_TRUE(logged("Invalid elastic timeout (negative): -5, using 30 seconds"));
}

// Credentials over https with verification off go to whichever server
// answers: said once, at load.
TEST_F(ElasticModule, CredentialsWithoutCertificateVerificationAreCalledOut) {
  core().set_setting("address", "https://elastic.example.invalid:9200/_bulk");
  core().set_setting("api key", "k");
  core().set_setting("verify mode", "none");
  ASSERT_TRUE(load());
  EXPECT_TRUE(logged("TLS certificate verification is disabled for elastic.example.invalid (verify mode: none)"));
}

TEST_F(ElasticModule, VerificationOnIsNotCalledOut) {
  core().set_setting("address", "https://elastic.example.invalid:9200/_bulk");
  core().set_setting("user", "u");
  ASSERT_TRUE(load());
  EXPECT_FALSE(logged("TLS certificate verification is disabled"));
}

// ============================================================================
// The submit paths, against an address that has no host
// ============================================================================
//
// A started module builds the bulk payload and parses the address before it
// connects; an address with no host is reported and nothing is sent, which is
// what lets each path be driven here without a server.

namespace {
const char *const kNoHost = "not a url";
}  // namespace

TEST_F(ElasticModule, AStartedModuleReportsAnAddressWithoutAHost) {
  core().set_setting("address", kNoHost);
  ASSERT_TRUE(load(NSCAPI::normalStart));

  PB::Commands::EventMessage event;
  PB::Commands::EventMessage::Request *line = event.add_payload();
  PB::Common::KeyValue *kv = line->add_data();
  kv->set_key("message");
  kv->set_value("disk full");
  kv = line->add_data();
  kv->set_key("written_str");
  kv->set_value("2026-01-01T00:00:00");
  module_.onEvent(event, "");

  EXPECT_TRUE(logged(std::string("Invalid elastic address: ") + kNoHost));
}

TEST_F(ElasticModule, MetricsAreSubmittedOnceStarted) {
  core().set_setting("address", kNoHost);
  ASSERT_TRUE(load(NSCAPI::reloadStart));

  PB::Metrics::MetricsMessage metrics;
  PB::Metrics::MetricsBundle *bundle = metrics.add_payload()->add_bundles();
  bundle->set_key("system");
  PB::Metrics::Metric *m = bundle->add_value();
  m->set_key("cpu.total");
  m->mutable_gauge_value()->set_value(12);
  m = bundle->add_value();
  m->set_key("os.name");
  m->mutable_string_value()->set_value("linux");
  PB::Metrics::MetricsBundle *child = bundle->add_children();
  child->set_key("mem");
  child->add_value()->set_key("used");
  child->mutable_value(0)->mutable_gauge_value()->set_value(0.5);
  module_.submitMetrics(metrics);

  EXPECT_TRUE(logged(std::string("Invalid elastic address: ") + kNoHost));
}

TEST_F(ElasticModule, LogMessagesAreSubmittedOnceStarted) {
  core().set_setting("address", kNoHost);
  ASSERT_TRUE(load(NSCAPI::normalStart));

  PB::Log::LogEntry::Entry entry;
  entry.set_sender("core");
  entry.set_message("hello");
  module_.handleLogMessage(entry);
  EXPECT_TRUE(logged(std::string("Invalid elastic address: ") + kNoHost));
}

// A failure to send one of its own log lines must not log an error, which
// would be forwarded here again, for ever.
TEST_F(ElasticModule, ItsOwnLogMessagesNeverLogASendFailure) {
  core().set_setting("address", kNoHost);
  ASSERT_TRUE(load(NSCAPI::normalStart));

  PB::Log::LogEntry::Entry entry;
  entry.set_sender("elastic");
  entry.set_message("hello");
  module_.handleLogMessage(entry);
  EXPECT_FALSE(logged("Invalid elastic address"));
}

TEST_F(ElasticModule, NothingIsSubmittedWithoutAnAddress) {
  ASSERT_TRUE(load(NSCAPI::normalStart));

  PB::Commands::EventMessage event;
  event.add_payload()->add_data()->set_key("message");
  module_.onEvent(event, "");
  EXPECT_FALSE(logged("Invalid elastic address"));
}
