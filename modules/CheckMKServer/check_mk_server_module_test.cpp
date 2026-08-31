// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Unit tests for the CheckMKServer module class: the settings it registers and
// reads in loadModuleEx(), the two passive submission channels it takes out,
// and the lifecycle around the Lua script manager. handler_impl (the cache and
// the <<<mrpe>>>/<<<local>>> rendering) has its own test.
//
// Everything here loads with NSCAPI::dontStart, so the socket server at :6556
// is never created - the settings and the registrations are parsed either way.

#include "CheckMKServer.h"

#include <gtest/gtest.h>

#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/test_helpers.hpp>
#include <string>

// Test binaries have no generated module glue, so the plugin singleton
// (normally provided by NSC_WRAP_DLL()) must be defined here.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

namespace {

class CheckMkServerModule : public ::testing::Test {
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

  bool load(const std::string &alias = "") { return module_.loadModuleEx(alias, NSCAPI::dontStart); }

  bool has_section(const std::string &name) const {
    for (const std::string &path : nscapi::test_helpers::stub_core::instance().registered_paths()) {
      if (path.size() > name.size() && path.compare(path.size() - name.size(), name.size(), name) == 0) return true;
    }
    return false;
  }

  CheckMKServer module_;
};

}  // namespace

// ============================================================================
// The settings contract
// ============================================================================

TEST_F(CheckMkServerModule, LoadRegistersItsKeysWithTheDocumentedDefaults) {
  ASSERT_TRUE(load());

  EXPECT_EQ(core().default_for("port"), "6556");
  EXPECT_EQ(core().default_for("mrpe channel"), "check_mk-mrpe");
  EXPECT_EQ(core().default_for("local channel"), "check_mk-local");
  EXPECT_EQ(core().default_for("submission ttl"), "60");
}

// The socket options every server module shares come from
// socket_helpers::settings_helper, so a module that forgets to call it loses
// its listener configuration silently.
TEST_F(CheckMkServerModule, LoadRegistersTheSharedServerAndSslOptions) {
  ASSERT_TRUE(load());

  EXPECT_TRUE(core().has_key(core().registered_keys().front().path, "allowed hosts")) << "no allowed hosts key";
  EXPECT_EQ(core().default_for("certificate"), "${certificate-path}/certificate.pem");
}

// scripts/ drives the Lua side; local/ and mrpe/ are read by
// default_check_mk.lua but registered here so they get help text and show up
// in `nscp settings --generate`.
TEST_F(CheckMkServerModule, LoadRegistersItsSettingsSections) {
  ASSERT_TRUE(load());

  EXPECT_TRUE(has_section("scripts")) << "no scripts section registered";
  EXPECT_TRUE(has_section("local")) << "no local section registered";
  EXPECT_TRUE(has_section("mrpe")) << "no mrpe section registered";
}

// ============================================================================
// The passive submission channels
// ============================================================================

TEST_F(CheckMkServerModule, LoadRegistersBothDefaultChannels) {
  ASSERT_TRUE(load());

  EXPECT_TRUE(core().has_channel("check_mk-mrpe"));
  EXPECT_TRUE(core().has_channel("check_mk-local"));
}

TEST_F(CheckMkServerModule, ConfiguredChannelsAreRegisteredInsteadOfTheDefaults) {
  core().set_setting("mrpe channel", "my-mrpe");
  core().set_setting("local channel", "my-local");

  ASSERT_TRUE(load());
  EXPECT_TRUE(core().has_channel("my-mrpe"));
  EXPECT_TRUE(core().has_channel("my-local"));
  EXPECT_FALSE(core().has_channel("check_mk-mrpe"));
}

// An empty channel name means "do not accept submissions on it"; registering
// the empty string would make the core route every unrouted submission here.
TEST_F(CheckMkServerModule, EmptyChannelNamesAreNotRegistered) {
  core().set_setting("mrpe channel", "");
  core().set_setting("local channel", "");

  ASSERT_TRUE(load());
  EXPECT_FALSE(core().has_channel(""));
}

// ============================================================================
// Submissions
// ============================================================================

TEST_F(CheckMkServerModule, SubmissionOnARegisteredChannelIsAccepted) {
  ASSERT_TRUE(load());

  PB::Commands::SubmitRequestMessage request;
  PB::Commands::QueryResponseMessage::Response *payload = request.add_payload();
  payload->set_command("Uptime");
  payload->set_result(PB::Common::ResultCode::OK);
  payload->add_lines()->set_message("uptime: 4d");

  PB::Commands::SubmitResponseMessage response;
  EXPECT_NO_THROW(module_.handleNotification("check_mk-mrpe", request, &response));
}

TEST_F(CheckMkServerModule, MetricsTickIsAccepted) {
  ASSERT_TRUE(load());

  PB::Metrics::MetricsMessage metrics;
  EXPECT_NO_THROW(module_.submitMetrics(metrics));
}

// ============================================================================
// Lifecycle
// ============================================================================

// dontStart must leave the listener uncreated, so shutting down is a no-op
// rather than a null dereference.
TEST_F(CheckMkServerModule, PrepareShutdownWithoutAServerIsSafe) {
  ASSERT_TRUE(load());
  EXPECT_NO_THROW(module_.prepareShutdown());
}

TEST_F(CheckMkServerModule, UnloadReleasesTheScriptManager) {
  ASSERT_TRUE(load());
  EXPECT_TRUE(module_.unloadModule());
  // Idempotent: prepareShutdown/unloadModule can both run on a reload.
  EXPECT_TRUE(module_.unloadModule());
}

TEST_F(CheckMkServerModule, ReloadIsSafe) {
  ASSERT_TRUE(load());
  ASSERT_TRUE(module_.unloadModule());
  EXPECT_TRUE(load());
}
