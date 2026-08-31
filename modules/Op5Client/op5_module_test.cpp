// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Unit tests for the Op5Client module class: the settings it registers and
// reads in loadModuleEx(), the default check table it seeds, and the two CLI
// subcommands (`nscp op5 install` / `add`) which read and write settings
// through the core and never touch the network.
//
// Everything loads with NSCAPI::dontStart, so the op5_client that would start
// submitting on an interval is never constructed - which is also what makes
// the "not configured" branch of handleNotification reachable here.

#include "Op5Client.h"

#include <gtest/gtest.h>

#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/test_helpers.hpp>
#include <string>
#include <vector>

// Test binaries have no generated module glue, so the plugin singleton
// (normally provided by NSC_WRAP_DLL()) must be defined here.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

namespace {

class Op5Module : public ::testing::Test {
 protected:
  nscapi::test_helpers::stub_core &core() { return nscapi::test_helpers::stub_core::instance(); }

  void SetUp() override {
    core().reset();
    module_.set_id(42);
  }
  void TearDown() override { core().reset(); }

  bool load() { return module_.loadModuleEx("", NSCAPI::dontStart); }

  // Run one `nscp op5 <argv...>` invocation and return what it wrote back.
  std::string cli(const std::vector<std::string> &args, bool *handled = nullptr) {
    PB::Commands::ExecuteRequestMessage request_message;
    PB::Commands::ExecuteRequestMessage::Request request;
    request.set_command("op5");
    for (const std::string &a : args) request.add_arguments(a);
    PB::Commands::ExecuteResponseMessage::Response response;

    const bool ret = module_.commandLineExec(NSCAPI::target_module, request, &response, request_message);
    if (handled != nullptr) *handled = ret;
    return response.message();
  }

  Op5Client module_;
};

}  // namespace

// ============================================================================
// The settings contract
// ============================================================================

TEST_F(Op5Module, LoadRegistersItsKeysWithTheDocumentedDefaults) {
  ASSERT_TRUE(load());

  EXPECT_EQ(core().default_for("hostname"), "auto");
  EXPECT_EQ(core().default_for("channel"), "op5");
  EXPECT_EQ(core().default_for("interval"), "5m");
  EXPECT_EQ(core().default_for("server"), "");
  EXPECT_EQ(core().default_for("user"), "");
}

TEST_F(Op5Module, TlsDefaultsAreTheHardenedOnes) {
  ASSERT_TRUE(load());

  EXPECT_EQ(core().default_for("tls version"), "1.2+");
  EXPECT_EQ(core().default_for("verify mode"), "peer");
  EXPECT_EQ(core().default_for("ca"), "${ca-path}");
}

TEST_F(Op5Module, ThePasswordIsRegisteredAsSensitive) {
  ASSERT_TRUE(load());

  EXPECT_TRUE(core().is_sensitive("password")) << "password is not redacted by the settings API";
  EXPECT_FALSE(core().is_sensitive("user"));
}

// Removing the registration on exit is destructive, and installing the default
// check set is not - so the defaults are off and on respectively.
TEST_F(Op5Module, BooleanDefaults) {
  ASSERT_TRUE(load());

  EXPECT_EQ(core().default_for("remove"), "false");
  EXPECT_EQ(core().default_for("default checks"), "true");
}

TEST_F(Op5Module, LoadRegistersTheDefaultChannel) {
  ASSERT_TRUE(load());
  EXPECT_TRUE(core().has_channel("op5"));
}

TEST_F(Op5Module, ConfiguredChannelIsRegisteredInsteadOfTheDefault) {
  core().set_setting("channel", "MY_OP5");

  ASSERT_TRUE(load());
  EXPECT_TRUE(core().has_channel("MY_OP5"));
  EXPECT_FALSE(core().has_channel("op5"));
}

// ============================================================================
// Submission before the client exists
// ============================================================================

// dontStart leaves the op5 client unconstructed; a submission arriving then
// must be answered with an error rather than dereferencing it.
TEST_F(Op5Module, SubmissionWithoutAConfiguredClientIsRejected) {
  ASSERT_TRUE(load());

  PB::Commands::SubmitRequestMessage request;
  request.add_payload()->set_command("check_cpu");
  PB::Commands::SubmitResponseMessage response;

  module_.handleNotification("op5", request, &response);

  ASSERT_EQ(response.payload_size(), 1);
  EXPECT_NE(response.payload(0).result().message().find("Invalid op5 configuration"), std::string::npos) << response.payload(0).result().message();
}

// ============================================================================
// `nscp op5 install`
// ============================================================================

TEST_F(Op5Module, InstallPersistsTheConnectionSettings) {
  ASSERT_TRUE(load());

  bool handled = false;
  const std::string out = cli({"install", "--user", "monitor", "--password", "secret", "--server", "https://op5.example.com", "--interval", "5m"}, &handled);

  EXPECT_TRUE(handled);
  EXPECT_EQ(core().updated_value("server"), "https://op5.example.com");
  EXPECT_EQ(core().updated_value("user"), "monitor");
  EXPECT_EQ(core().updated_value("interval"), "5m");
  EXPECT_NE(out.find("Sending status every 300 seconds"), std::string::npos) << out;
}

// Anything not given on the command line is taken from what is already
// configured, so a partial re-install does not blank the rest.
TEST_F(Op5Module, InstallFallsBackToTheConfiguredValues) {
  core().set_setting("/settings/op5", "user", "existing_user");
  core().set_setting("/settings/op5", "server", "https://existing.example.com");
  core().set_setting("/settings/op5", "interval", "10m");
  ASSERT_TRUE(load());

  bool handled = false;
  cli({"install", "--password", "secret"}, &handled);

  EXPECT_TRUE(handled);
  EXPECT_EQ(core().updated_value("user"), "existing_user");
  EXPECT_EQ(core().updated_value("server"), "https://existing.example.com");
  EXPECT_EQ(core().updated_value("interval"), "10m");
}

// The optional group lists are only written when given: writing an empty value
// would clear a group set through the web UI.
TEST_F(Op5Module, InstallOnlyWritesGroupsWhenGiven) {
  ASSERT_TRUE(load());

  cli({"install", "--user", "u", "--password", "p", "--server", "s", "--interval", "1m"});
  EXPECT_FALSE(core().was_updated("hostgroups"));
  EXPECT_FALSE(core().was_updated("contactgroups"));

  core().reset();
  ASSERT_TRUE(load());
  const std::string out = cli({"install", "--user", "u", "--password", "p", "--server", "s", "--interval", "1m", "--hostgroups", "linux-servers"});
  EXPECT_EQ(core().updated_value("hostgroups"), "linux-servers");
  EXPECT_NE(out.find("linux-servers"), std::string::npos) << out;
}

TEST_F(Op5Module, InstallHelpWritesNothing) {
  ASSERT_TRUE(load());

  const std::string out = cli({"install", "--help"});

  EXPECT_NE(out.find("server=ARG"), std::string::npos) << out;
  EXPECT_TRUE(core().updated_settings().empty()) << "help persisted settings";
}

TEST_F(Op5Module, InstallRejectsAnUnknownOption) {
  ASSERT_TRUE(load());

  const std::string out = cli({"install", "--not-an-option"});

  EXPECT_NE(out.find("Invalid command line"), std::string::npos) << out;
  EXPECT_TRUE(core().updated_settings().empty()) << "a rejected command line persisted settings";
}

// ============================================================================
// `nscp op5 add`
// ============================================================================

TEST_F(Op5Module, AddPersistsTheCheck) {
  ASSERT_TRUE(load());

  bool handled = false;
  const std::string out = cli({"add", "--alias", "CPU Load", "--command", "check_cpu"}, &handled);

  EXPECT_TRUE(handled);
  EXPECT_EQ(core().updated_value("CPU Load"), "check_cpu");
  EXPECT_NE(out.find("Adding check CPU Load as check_cpu"), std::string::npos) << out;
}

TEST_F(Op5Module, AddCheckIsAnAliasForAdd) {
  ASSERT_TRUE(load());

  cli({"add-check", "--alias", "Uptime", "--command", "check_uptime"});

  EXPECT_EQ(core().updated_value("Uptime"), "check_uptime");
}

// ============================================================================
// Dispatch and lifecycle
// ============================================================================

TEST_F(Op5Module, UnknownSubcommandExplainsTheUsage) {
  ASSERT_TRUE(load());

  const std::string out = cli({"wobble"});

  EXPECT_NE(out.find("Usage: nscp op5"), std::string::npos) << out;
}

TEST_F(Op5Module, UnloadWithoutAStartedClientIsSafe) {
  ASSERT_TRUE(load());
  EXPECT_TRUE(module_.unloadModule());
  EXPECT_TRUE(module_.unloadModule());
}
