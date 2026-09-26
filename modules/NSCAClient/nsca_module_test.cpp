// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Unit tests for the NSCAClient module class - the settings it registers and
// reads in loadModuleEx(), and the targets and handlers sections it turns into
// client targets and relay commands. The wire format and the option parsing live in the
// module's other translation units and have their own tests; what is covered
// here is only the plugin shell around them.
//
// The settings keys and their defaults are part of the module's contract with
// every existing nsclient.ini, so a rename or a changed default is a breaking
// change: asserting on them here is what makes that visible in review.

#include "NSCAClient.h"

#include <gtest/gtest.h>

#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/test_helpers.hpp>
#include <string>
#include <vector>

// Test binaries have no generated module glue, so the plugin singleton
// (normally provided by NSC_WRAP_DLL()) must be defined here.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

namespace {

class NscaModule : public ::testing::Test {
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

  NSCAClient module_;
};

}  // namespace

// ============================================================================
// The settings contract
// ============================================================================

TEST_F(NscaModule, LoadRegistersItsKeysWithTheDocumentedDefaults) {
  ASSERT_TRUE(load());

  EXPECT_EQ(core().default_for("hostname"), "auto");
  EXPECT_EQ(core().default_for("encoding"), "");
  EXPECT_EQ(core().default_for("channel"), "NSCA");
}

TEST_F(NscaModule, LoadRegistersItsSettingsSections) {
  ASSERT_TRUE(load());

  EXPECT_TRUE(has_section("targets")) << "no targets section registered";
  EXPECT_TRUE(has_section("handlers")) << "no handlers section registered";
}

// ============================================================================
// The channel: what the core routes submissions on
// ============================================================================

TEST_F(NscaModule, LoadRegistersTheDefaultChannel) {
  ASSERT_TRUE(load());
  EXPECT_TRUE(core().has_channel("NSCA")) << "NSCA channel not registered";
}

TEST_F(NscaModule, ConfiguredChannelIsRegisteredInsteadOfTheDefault) {
  core().set_setting("channel", "MY_CHANNEL");

  ASSERT_TRUE(load());
  EXPECT_TRUE(core().has_channel("MY_CHANNEL"));
  EXPECT_FALSE(core().has_channel("NSCA"));
}

// ============================================================================
// The handlers section becomes relay commands
// ============================================================================

TEST_F(NscaModule, HandlerKeysAreRegisteredAsCommands) {
  core().set_keys("handlers", {{"submit_nsca", "host=127.0.0.1,port=5667"}});

  ASSERT_TRUE(load());
  EXPECT_TRUE(core().has_command("submit_nsca")) << "handler key was not registered as a command";
}

// A handler the client cannot make a command out of must not be registered -
// and must not take the module load down with it.
TEST_F(NscaModule, EmptyHandlerKeyRegistersNoCommand) {
  core().set_keys("handlers", {{"", ""}});

  ASSERT_TRUE(load());
  EXPECT_FALSE(core().has_command(""));
}

// ============================================================================
// The targets section
// ============================================================================

TEST_F(NscaModule, TargetKeysAreAccepted) {
  core().set_keys("targets", {{"default", "host=127.0.0.1,port=5667"}});

  EXPECT_TRUE(load());
}

// One unparseable target in nsclient.ini must be reported and skipped, not
// take the whole module offline.
TEST_F(NscaModule, UnparseableTargetDoesNotFailTheLoad) {
  core().set_keys("targets", {{"broken", "this is not a target definition"}});

  EXPECT_TRUE(load());
}

// ============================================================================
// Lifecycle and dispatch
// ============================================================================

TEST_F(NscaModule, UnloadWithoutLoadIsSafe) { EXPECT_TRUE(module_.unloadModule()); }

TEST_F(NscaModule, UnloadAfterLoadIsIdempotent) {
  ASSERT_TRUE(load());
  EXPECT_TRUE(module_.unloadModule());
  EXPECT_TRUE(module_.unloadModule());
}

TEST_F(NscaModule, CommandLineExecOnlyHandlesItsOwnTargetMode) {
  ASSERT_TRUE(load());
  PB::Commands::ExecuteRequestMessage request;
  PB::Commands::ExecuteResponseMessage response;

  EXPECT_FALSE(module_.commandLineExec(NSCAPI::target_any, request, response));
}

// ============================================================================
// `nscp nsca install`
// ============================================================================
//
// What the command writes is this agent's submission configuration, and the
// shared NSCA key lives with it rather than under /settings/default - that
// section is the password inbound protocols verify a caller against, and it is
// stored hashed, which is not a key anything can encrypt with. These tests pin
// the paths, because they are the contract the MSI's NSCA_* options write to,
// and they pin that the command stays out of the NSCAServer section: the
// listening key is shared with different peers and is the operator's to set.

namespace {

// `nscp nsca install <args...>`, as commandLineExec sees it.
PB::Commands::ExecuteRequestMessage install_request(const std::vector<std::string> &args) {
  PB::Commands::ExecuteRequestMessage request;
  PB::Commands::ExecuteRequestMessage::Request *payload = request.add_payload();
  payload->set_command("nsca");
  payload->add_arguments("install");
  for (const std::string &arg : args) {
    payload->add_arguments(arg);
  }
  return request;
}

const char *kTarget = "/settings/NSCA/client/targets/default";

}  // namespace

TEST_F(NscaModule, InstallWritesTheTargetAndEnablesTheModule) {
  ASSERT_TRUE(load());
  PB::Commands::ExecuteResponseMessage response;
  ASSERT_TRUE(module_.commandLineExec(NSCAPI::target_module,
                                      install_request({"--host", "nagios.example.com", "--password", "the-nsca-key", "--encryption", "aes256"}), response));
  ASSERT_EQ(response.payload_size(), 1);
  EXPECT_EQ(response.payload(0).result(), PB::Common::ResultCode::OK) << response.payload(0).message();

  EXPECT_EQ(core().updated_value("address"), "nagios.example.com");
  EXPECT_EQ(core().updated_value("password"), "the-nsca-key");
  EXPECT_EQ(core().updated_value("encryption"), "aes256");
  EXPECT_EQ(core().updated_value("NSCAClient"), "enabled");

  // The key goes with the target, never into the shared inbound password.
  for (const auto &update : core().updated_settings()) {
    EXPECT_NE(update.path, "/settings/default") << "wrote " << update.key << " into the shared inbound password section";
    if (update.key == "password") EXPECT_EQ(update.path, kTarget);
  }
  EXPECT_TRUE(response.payload(0).message().find("nagios.example.com") != std::string::npos);
}

TEST_F(NscaModule, InstallRefusesWithoutAHostToSubmitTo) {
  ASSERT_TRUE(load());
  PB::Commands::ExecuteResponseMessage response;
  ASSERT_TRUE(module_.commandLineExec(NSCAPI::target_module, install_request({"--password", "the-nsca-key"}), response));
  ASSERT_EQ(response.payload_size(), 1);
  // A module that loads, registers its channel and drops every result is worse
  // than a command that refuses.
  EXPECT_EQ(response.payload(0).result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_TRUE(response.payload(0).message().find("--host") != std::string::npos) << response.payload(0).message();
  EXPECT_TRUE(core().updated_settings().empty()) << "nothing should be written when the command refuses";
}

TEST_F(NscaModule, InstallKeepsWhatItWasNotGiven) {
  core().set_setting(kTarget, "address", "old.example.com");
  core().set_setting(kTarget, "password", "old-key");
  core().set_setting(kTarget, "encryption", "3des");
  ASSERT_TRUE(load());

  PB::Commands::ExecuteResponseMessage response;
  ASSERT_TRUE(module_.commandLineExec(NSCAPI::target_module, install_request({"--host", "new.example.com"}), response));
  ASSERT_EQ(response.payload(0).result(), PB::Common::ResultCode::OK) << response.payload(0).message();

  EXPECT_EQ(core().updated_value("address"), "new.example.com");
  // Re-running to move the server must not silently reset the cipher the
  // daemon is configured for, nor drop the key.
  EXPECT_EQ(core().updated_value("encryption"), "3des");
  EXPECT_EQ(core().updated_value("password"), "old-key");
}

TEST_F(NscaModule, InstallDefaultsTheCipherOnlyOnAFreshTarget) {
  ASSERT_TRUE(load());
  PB::Commands::ExecuteResponseMessage response;
  ASSERT_TRUE(module_.commandLineExec(NSCAPI::target_module, install_request({"--host", "nagios.example.com"}), response));
  EXPECT_EQ(core().updated_value("encryption"), "aes256");
}

TEST_F(NscaModule, InstallWarnsAboutAnEmptyKey) {
  ASSERT_TRUE(load());
  PB::Commands::ExecuteResponseMessage response;
  ASSERT_TRUE(module_.commandLineExec(NSCAPI::target_module, install_request({"--host", "nagios.example.com"}), response));
  ASSERT_EQ(response.payload(0).result(), PB::Common::ResultCode::OK) << response.payload(0).message();
  // NSCA derives its key from the password, so an empty one is a well-known key.
  EXPECT_TRUE(response.payload(0).message().find("no password set") != std::string::npos) << response.payload(0).message();
}

TEST_F(NscaModule, InstallWarnsThatNSCAServerStillNeedsItsOwnKey) {
  core().set_setting("/modules", "NSCAServer", "enabled");
  ASSERT_TRUE(load());

  PB::Commands::ExecuteResponseMessage response;
  ASSERT_TRUE(module_.commandLineExec(NSCAPI::target_module, install_request({"--host", "nagios.example.com", "--password", "the-nsca-key"}), response));
  ASSERT_EQ(response.payload(0).result(), PB::Common::ResultCode::OK) << response.payload(0).message();
  // The listener does not borrow this key and refuses to start without one, so
  // the operator has to hear about it here rather than at the next restart.
  EXPECT_TRUE(response.payload(0).message().find("NSCAServer is enabled but has no key of its own") != std::string::npos) << response.payload(0).message();
  EXPECT_TRUE(response.payload(0).message().find("does not use this one") != std::string::npos) << response.payload(0).message();
}

TEST_F(NscaModule, InstallStaysQuietWhenNSCAServerHasItsOwnKey) {
  core().set_setting("/modules", "NSCAServer", "enabled");
  core().set_setting("/settings/NSCA/server", "password", "a-different-key");
  ASSERT_TRUE(load());

  PB::Commands::ExecuteResponseMessage response;
  ASSERT_TRUE(module_.commandLineExec(NSCAPI::target_module, install_request({"--host", "nagios.example.com", "--password", "the-nsca-key"}), response));
  EXPECT_TRUE(response.payload(0).message().find("NSCAServer is enabled") == std::string::npos) << response.payload(0).message();
}

TEST_F(NscaModule, InstallNeverWritesTheNSCAServerKey) {
  core().set_setting("/modules", "NSCAServer", "enabled");
  ASSERT_TRUE(load());

  PB::Commands::ExecuteResponseMessage response;
  ASSERT_TRUE(module_.commandLineExec(NSCAPI::target_module, install_request({"--host", "nagios.example.com", "--password", "the-nsca-key"}), response));
  ASSERT_EQ(response.payload(0).result(), PB::Common::ResultCode::OK) << response.payload(0).message();
  // Warning the operator is as far as it goes: the listening key is shared with
  // different peers, so this command must not guess it from the client target.
  for (const auto &update : core().updated_settings()) {
    EXPECT_NE(update.path, "/settings/NSCA/server") << "wrote " << update.key << " into the NSCA server section";
  }
}

TEST_F(NscaModule, InstallHelpDoesNotWriteAnything) {
  ASSERT_TRUE(load());
  PB::Commands::ExecuteResponseMessage response;
  ASSERT_TRUE(module_.commandLineExec(NSCAPI::target_module, install_request({"--help"}), response));
  EXPECT_TRUE(core().updated_settings().empty());
}
