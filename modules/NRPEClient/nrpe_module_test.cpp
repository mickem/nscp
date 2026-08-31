// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Unit tests for the NRPEClient module class: the settings it registers and
// reads in loadModuleEx(), the target/handler sections it turns into client
// targets and relay commands, and `nscp nrpe install` - the subcommand that
// writes the NRPE *server's* TLS configuration.
//
// That last one is why this file is longer than the other client-module tests:
// install decides whether the agent ends up with certificate verification and
// a modern cipher list, or with the legacy insecure profile. Which settings
// each mode writes is pinned here, because nothing else in the build would
// notice if --insecure started being the effective default.

#include "NRPEClient.h"

#include <gtest/gtest.h>

#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/test_helpers.hpp>
#include <string>
#include <vector>

// Test binaries have no generated module glue, so the plugin singleton
// (normally provided by NSC_WRAP_DLL()) must be defined here.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

namespace {

class NrpeModule : public ::testing::Test {
 protected:
  nscapi::test_helpers::stub_core &core() { return nscapi::test_helpers::stub_core::instance(); }

  void SetUp() override {
    core().reset();
    module_.set_id(42);
  }
  void TearDown() override { core().reset(); }

  bool load(const std::string &alias = "") { return module_.loadModuleEx(alias, NSCAPI::dontStart); }

  bool has_section(const std::string &name) const {
    for (const std::string &path : nscapi::test_helpers::stub_core::instance().registered_paths()) {
      if (path.size() > name.size() && path.compare(path.size() - name.size(), name.size(), name) == 0) return true;
    }
    return false;
  }

  // Run one `nscp nrpe <argv...>` invocation and return what it wrote back.
  std::string cli(const std::vector<std::string> &args, bool *handled = nullptr) {
    PB::Commands::ExecuteRequestMessage request;
    PB::Commands::ExecuteRequestMessage::Request *payload = request.add_payload();
    payload->set_command("nrpe");
    for (const std::string &a : args) payload->add_arguments(a);
    PB::Commands::ExecuteResponseMessage response;

    const bool ret = module_.commandLineExec(NSCAPI::target_module, request, response);
    if (handled != nullptr) *handled = ret;
    return response.payload_size() > 0 ? response.payload(0).message() : std::string();
  }

  // What `install` wrote for one of the server's TLS keys.
  std::string wrote(const std::string &key) const { return nscapi::test_helpers::stub_core::instance().updated_value(key); }

  NRPEClient module_;
};

}  // namespace

// ============================================================================
// The settings contract
// ============================================================================

TEST_F(NrpeModule, LoadRegistersItsKeysWithTheDocumentedDefaults) {
  ASSERT_TRUE(load());
  EXPECT_EQ(core().default_for("channel"), "NRPE");
}

TEST_F(NrpeModule, LoadRegistersItsSettingsSections) {
  ASSERT_TRUE(load());
  EXPECT_TRUE(has_section("targets")) << "no targets section registered";
  EXPECT_TRUE(has_section("handlers")) << "no handlers section registered";
}

TEST_F(NrpeModule, LoadRegistersTheDefaultChannel) {
  ASSERT_TRUE(load());
  EXPECT_TRUE(core().has_channel("NRPE"));
}

TEST_F(NrpeModule, ConfiguredChannelIsRegisteredInsteadOfTheDefault) {
  core().set_setting("channel", "MY_NRPE");

  ASSERT_TRUE(load());
  EXPECT_TRUE(core().has_channel("MY_NRPE"));
  EXPECT_FALSE(core().has_channel("NRPE"));
}

TEST_F(NrpeModule, HandlerKeysAreRegisteredAsCommands) {
  core().set_keys("handlers", {{"check_nrpe_remote", "host=127.0.0.1"}});

  ASSERT_TRUE(load());
  EXPECT_TRUE(core().has_command("check_nrpe_remote")) << "handler key was not registered as a command";
}

TEST_F(NrpeModule, TargetKeysAreAccepted) {
  core().set_keys("targets", {{"default", "host=127.0.0.1,port=5666"}});

  EXPECT_TRUE(load());
}

// One unparseable target in nsclient.ini must be reported and skipped, not
// take the whole module offline.
TEST_F(NrpeModule, UnparseableTargetDoesNotFailTheLoad) {
  core().set_keys("targets", {{"broken", "this is not a target definition"}});

  EXPECT_TRUE(load());
}

// ============================================================================
// `nscp nrpe install`: the secure profile
// ============================================================================

// The default run is refused rather than silently configuring a server that
// verifies nothing: peer-cert verification needs a CA to verify against.
TEST_F(NrpeModule, InstallRefusesVerificationWithoutACa) {
  ASSERT_TRUE(load());

  const std::string out = cli({"install"});

  EXPECT_NE(out.find("you need to specify a CA"), std::string::npos) << out;
  EXPECT_TRUE(core().updated_settings().empty()) << "a refused install wrote settings anyway";
}

TEST_F(NrpeModule, InstallWithACaWritesTheHardenedProfile) {
  ASSERT_TRUE(load());

  bool handled = false;
  const std::string out = cli({"install", "--ca", "/etc/nscp/ca.pem"}, &handled);

  EXPECT_TRUE(handled);
  EXPECT_EQ(wrote("ssl"), "true");
  EXPECT_EQ(wrote("insecure"), "false");
  EXPECT_EQ(wrote("ca"), "/etc/nscp/ca.pem");
  EXPECT_EQ(wrote("allowed ciphers"), "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");
  EXPECT_EQ(wrote("ssl options"), "no-sslv2,no-sslv3");
  EXPECT_NE(out.find("Enabling NRPE via SSL"), std::string::npos) << out;
}

// `verify` is initialised to "peer-cert", but install() then reads the
// server's current `verify mode` with an empty default and assigns it
// unconditionally - so on a machine that has never had one configured, the
// initialiser is wiped before the option's default_value even captures it.
// A clean `install --ca ...` therefore writes an empty verify mode (encryption
// but no client-certificate requirement), which the command's own output does
// report as "not secure ... no authentication". Pinned as it behaves today.
TEST_F(NrpeModule, InstallVerifyDefaultIsOverwrittenByTheUnsetSetting) {
  ASSERT_TRUE(load());

  const std::string out = cli({"install", "--ca", "/etc/nscp/ca.pem"});

  EXPECT_EQ(wrote("verify mode"), "");
  EXPECT_NE(out.find("no authentication"), std::string::npos) << out;
}

// Asking for it explicitly does work, and says so.
TEST_F(NrpeModule, InstallWithExplicitPeerCertRequiresClientCertificates) {
  ASSERT_TRUE(load());

  const std::string out = cli({"install", "--ca", "/etc/nscp/ca.pem", "--verify", "peer-cert"});

  EXPECT_EQ(wrote("verify mode"), "peer-cert");
  EXPECT_NE(out.find("will require client certificates"), std::string::npos) << out;
}

// An already-configured verify mode is carried over rather than reset.
TEST_F(NrpeModule, InstallKeepsTheConfiguredVerifyMode) {
  core().set_setting("/settings/NRPE/server", "verify mode", "peer-cert");
  ASSERT_TRUE(load());

  cli({"install", "--ca", "/etc/nscp/ca.pem"});

  EXPECT_EQ(wrote("verify mode"), "peer-cert");
}

TEST_F(NrpeModule, InstallEnablesTheServerModuleAndPort) {
  ASSERT_TRUE(load());

  cli({"install", "--ca", "/etc/nscp/ca.pem", "--port", "5777"});

  EXPECT_EQ(wrote("port"), "5777");
  EXPECT_EQ(wrote("NRPEServer"), "enabled");
}

// verify=none is a deliberate opt-out and needs no CA, but it still leaves the
// modern cipher list in place - it is not the same thing as --insecure.
TEST_F(NrpeModule, InstallWithVerifyNoneStillWritesModernCiphers) {
  ASSERT_TRUE(load());

  cli({"install", "--verify", "none"});

  EXPECT_EQ(wrote("insecure"), "false");
  EXPECT_EQ(wrote("verify mode"), "none");
  EXPECT_EQ(wrote("allowed ciphers"), "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");
}

// ============================================================================
// `nscp nrpe install --insecure`: the legacy profile
// ============================================================================

// The legacy profile is what talks to old check_nrpe builds. It must clear
// every verification setting *and* say so in the output - an operator who asks
// for it should be told what they got.
TEST_F(NrpeModule, InsecureInstallWritesTheLegacyProfileAndWarns) {
  ASSERT_TRUE(load());

  const std::string out = cli({"install", "--insecure"});

  EXPECT_EQ(wrote("insecure"), "true");
  EXPECT_EQ(wrote("allowed ciphers"), "ALL:!MD5:@STRENGTH:@SECLEVEL=0");
  EXPECT_EQ(wrote("verify mode"), "");
  EXPECT_EQ(wrote("ca"), "");
  EXPECT_EQ(wrote("certificate"), "");
  EXPECT_NE(out.find("WARNING: NRPE is currently insecure"), std::string::npos) << out;
}

// ============================================================================
// `nscp nrpe install`: argument handling
// ============================================================================

// Arguments are off by default because a remote caller passing arguments to a
// check is how an NRPE agent turns into a remote execution service.
TEST_F(NrpeModule, InstallLeavesArgumentsOffByDefault) {
  ASSERT_TRUE(load());

  cli({"install", "--ca", "/etc/nscp/ca.pem"});

  EXPECT_EQ(wrote("allow arguments"), "false");
  EXPECT_EQ(wrote("allow nasty characters"), "false");
}

// `safe` is the middle setting: arguments, but not shell metacharacters.
TEST_F(NrpeModule, InstallWithSafeArgumentsAllowsArgumentsOnly) {
  ASSERT_TRUE(load());

  cli({"install", "--ca", "/etc/nscp/ca.pem", "--arguments", "safe"});

  EXPECT_EQ(wrote("allow arguments"), "true");
  EXPECT_EQ(wrote("allow nasty characters"), "false");
}

TEST_F(NrpeModule, InstallWithAllArgumentsAllowsNastyCharactersToo) {
  ASSERT_TRUE(load());

  cli({"install", "--ca", "/etc/nscp/ca.pem", "--arguments", "all"});

  EXPECT_EQ(wrote("allow arguments"), "true");
  EXPECT_EQ(wrote("allow nasty characters"), "true");
}

// ============================================================================
// Dispatch
// ============================================================================

TEST_F(NrpeModule, InstallHelpWritesNothing) {
  ASSERT_TRUE(load());

  const std::string out = cli({"install", "--help"});

  EXPECT_NE(out.find("insecure"), std::string::npos) << out;
  EXPECT_TRUE(core().updated_settings().empty()) << "help persisted settings";
}

TEST_F(NrpeModule, NoSubcommandExplainsTheUsage) {
  ASSERT_TRUE(load());

  const std::string out = cli({"help"});

  EXPECT_NE(out.find("Usage: nscp nrpe"), std::string::npos) << out;
}

TEST_F(NrpeModule, UnloadIsSafe) {
  ASSERT_TRUE(load());
  EXPECT_TRUE(module_.unloadModule());
}
