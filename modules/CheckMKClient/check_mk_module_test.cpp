// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Unit tests for the CheckMKClient module class - the settings it registers
// and reads in loadModuleEx(), and the target/handler sections it turns into
// client targets and relay commands. The check_mk wire format and the option
// parsing live in check_mk_handler.hpp and have their own tests.

#include "CheckMKClient.h"

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/test_helpers.hpp>
#include <string>

// Test binaries have no generated module glue, so the plugin singleton
// (normally provided by NSC_WRAP_DLL()) must be defined here.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

namespace {

class CheckMkModule : public ::testing::Test {
 protected:
  nscapi::test_helpers::stub_core &core() { return nscapi::test_helpers::stub_core::instance(); }

  void SetUp() override {
    core().reset();
    module_.set_id(42);
  }
  void TearDown() override { core().reset(); }

  bool load(const std::string &alias = "") { return module_.loadModuleEx(alias, NSCAPI::dontStart); }

  // The settings root differs per module, so a section is matched by its tail
  // rather than its full path.
  bool has_section(const std::string &name) const {
    for (const std::string &path : nscapi::test_helpers::stub_core::instance().registered_paths()) {
      if (path.size() > name.size() && path.compare(path.size() - name.size(), name.size(), name) == 0) return true;
    }
    return false;
  }

  CheckMKClient module_;
};

}  // namespace

TEST_F(CheckMkModule, LoadRegistersItsKeysWithTheDocumentedDefaults) {
  ASSERT_TRUE(load());
  EXPECT_EQ(core().default_for("channel"), "CheckMK");
}

TEST_F(CheckMkModule, LoadRegistersItsSettingsSections) {
  ASSERT_TRUE(load());
  EXPECT_TRUE(has_section("targets")) << "no targets section registered";
  EXPECT_TRUE(has_section("handlers")) << "no handlers section registered";
}

TEST_F(CheckMkModule, LoadRegistersTheDefaultChannel) {
  ASSERT_TRUE(load());
  EXPECT_TRUE(core().has_channel("CheckMK"));
}

TEST_F(CheckMkModule, ConfiguredChannelIsRegisteredInsteadOfTheDefault) {
  core().set_setting("channel", "MY_CHANNEL");

  ASSERT_TRUE(load());
  EXPECT_TRUE(core().has_channel("MY_CHANNEL"));
  EXPECT_FALSE(core().has_channel("CheckMK"));
}

TEST_F(CheckMkModule, HandlerKeysAreRegisteredAsCommands) {
  core().set_keys("handlers", {{"submit_check_mk", "host=127.0.0.1"}});

  ASSERT_TRUE(load());
  EXPECT_TRUE(core().has_command("submit_check_mk")) << "handler key was not registered as a command";
}

TEST_F(CheckMkModule, EmptyHandlerKeyRegistersNoCommand) {
  core().set_keys("handlers", {{"", ""}});

  ASSERT_TRUE(load());
  EXPECT_FALSE(core().has_command(""));
}

TEST_F(CheckMkModule, TargetKeysAreAccepted) {
  core().set_keys("targets", {{"default", "host=127.0.0.1,port=6556"}});

  EXPECT_TRUE(load());
}

// One unparseable target in nsclient.ini must be reported and skipped, not
// take the whole module offline.
TEST_F(CheckMkModule, UnparseableTargetDoesNotFailTheLoad) {
  core().set_keys("targets", {{"broken", "this is not a target definition"}});

  EXPECT_TRUE(load());
}

TEST_F(CheckMkModule, UnloadWithoutLoadIsSafe) { EXPECT_TRUE(module_.unloadModule()); }

TEST_F(CheckMkModule, UnloadAfterLoadIsIdempotent) {
  ASSERT_TRUE(load());
  EXPECT_TRUE(module_.unloadModule());
  EXPECT_TRUE(module_.unloadModule());
}

TEST_F(CheckMkModule, CommandLineExecOnlyHandlesItsOwnTargetMode) {
  ASSERT_TRUE(load());
  PB::Commands::ExecuteRequestMessage request;
  PB::Commands::ExecuteResponseMessage response;

  EXPECT_FALSE(module_.commandLineExec(NSCAPI::target_any, request, response));
}

// ============================================================================
// connection_data — what a target's settings turn into on the wire
// ============================================================================

namespace {

client::destination_container check_mk_target_with(const std::map<std::string, std::string> &options) {
  client::destination_container d;
  for (const auto &o : options) d.set_string_data(o.first, o.second);
  return d;
}

// Stands in for the module's own handler, whose expand_path goes through the
// core. The prefix makes an expansion visible in the assertions.
struct expanding_handler : socket_helpers::client::client_handler {
  void log_debug(std::string, int, std::string) const override {}
  void log_error(std::string, int, std::string) const override {}
  std::string expand_path(std::string path) override { return "/expanded" + path; }
};

check_mk_client::connection_data check_mk_connection_for(const std::map<std::string, std::string> &options) {
  return check_mk_client::connection_data(check_mk_target_with(options), client::destination_container(), std::make_shared<expanding_handler>());
}

}  // namespace

TEST(CheckMkConnectionData, TheAgentIsVerifiedUnlessTheTargetSaysOtherwise) {
  // An empty verify mode parses to verify_none, so "leave it unset" used to
  // mean a TLS target encrypted the agent section without ever authenticating
  // the agent it came from.
  const check_mk_client::connection_data con = check_mk_connection_for({{"address", "agent.example.com"}});

  EXPECT_EQ(con.ssl.verify_mode, "peer");
  EXPECT_EQ(con.ssl.ca_path, "/expanded${ca-path}") << "the default CA is the agent's bundle, expanded through the handler";
}

TEST(CheckMkConnectionData, AnExplicitVerifyModeAndCaAreNotOverridden) {
  EXPECT_EQ(check_mk_connection_for({{"address", "h"}, {"verify mode", "none"}}).ssl.verify_mode, "none")
      << "an operator opting out of verification must still be honoured";
  EXPECT_EQ(check_mk_connection_for({{"address", "h"}, {"ca", "/etc/ca.pem"}}).ssl.ca_path, "/expanded/etc/ca.pem");
}

TEST(CheckMkConnectionData, ABlankVerifyModeFallsBackToTheDefault) {
  // `verify mode =` with nothing after it parses to verify_none, so a blank
  // key must not be a quiet way to switch verification off - `none` is.
  const check_mk_client::connection_data con = check_mk_connection_for({{"address", "h"}, {"verify mode", ""}, {"ca", ""}});

  EXPECT_EQ(con.ssl.verify_mode, "peer");
  EXPECT_EQ(con.ssl.ca_path, "/expanded${ca-path}");
}

TEST(CheckMkConnectionData, AMissingHandlerLeavesThePathsVerbatim) {
  const check_mk_client::connection_data con(check_mk_target_with({{"address", "h"}}), client::destination_container());

  EXPECT_EQ(con.ssl.ca_path, "${ca-path}");
}
