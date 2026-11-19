// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Unit tests for the CheckMKClient module class - the settings it registers
// and reads in loadModuleEx(), and the target/handler sections it turns into
// client targets and relay commands. The check_mk wire format and the option
// parsing live in check_mk_handler.hpp and have their own tests.

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/test_helpers.hpp>
#include <string>
#include <vector>

#include "CheckMKClient.h"
#include "check_mk_install.hpp"

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

// ============================================================================
// `nscp mk install` / `nscp mk show`
// ============================================================================

namespace {

PB::Commands::ExecuteResponseMessage::Response run_cli(CheckMKClient &module, const std::vector<std::string> &args) {
  PB::Commands::ExecuteRequestMessage request;
  PB::Commands::ExecuteRequestMessage::Request *payload = request.add_payload();
  payload->set_command("mk");
  for (const std::string &a : args) payload->add_arguments(a);
  PB::Commands::ExecuteResponseMessage response;
  EXPECT_TRUE(module.commandLineExec(NSCAPI::target_module, request, response));
  EXPECT_EQ(response.payload_size(), 1);
  return response.payload_size() > 0 ? response.payload(0) : PB::Commands::ExecuteResponseMessage::Response();
}

const nscapi::test_helpers::registered_key *update_of(const std::string &path, const std::string &key) {
  for (const auto &k : nscapi::test_helpers::stub_core::instance().updated_settings()) {
    if (k.path == path && k.key == key) return &k;
  }
  return nullptr;
}

}  // namespace

TEST_F(CheckMkModule, InstallEnablesTheAgentAndTheModulesItsSectionsNeed) {
  const auto r = run_cli(module_, {"install", "--allowed-hosts", "10.0.0.5"});

  EXPECT_EQ(r.result(), PB::Common::ResultCode::OK) << r.message();
  for (const char *module : {"CheckMKServer", "CheckSystem", "CheckDisk"}) {
    const auto *u = update_of("/modules", module);
    ASSERT_NE(u, nullptr) << module;
    EXPECT_EQ(u->default_value, "enabled");
  }
  ASSERT_NE(update_of("/settings/check_mk/server", "allowed hosts"), nullptr);
  EXPECT_EQ(update_of("/settings/check_mk/server", "allowed hosts")->default_value, "10.0.0.5");
  EXPECT_EQ(update_of("/settings/check_mk/server", "port")->default_value, "6556");
  // Only the agent's own list: NRPE and the others keep /settings/default.
  EXPECT_EQ(update_of("/settings/default", "allowed hosts"), nullptr);
  EXPECT_EQ(r.message().find("WARNING"), std::string::npos) << r.message();
}

TEST_F(CheckMkModule, InstallKeepsTheConfiguredPortAndHostsWhenNotGiven) {
  core().set_setting("/settings/check_mk/server", "port", "16556");
  core().set_setting("/settings/check_mk/server", "allowed hosts", "192.168.1.10");

  const auto r = run_cli(module_, {"install"});

  EXPECT_EQ(r.result(), PB::Common::ResultCode::OK) << r.message();
  EXPECT_EQ(update_of("/settings/check_mk/server", "port")->default_value, "16556");
  EXPECT_EQ(update_of("/settings/check_mk/server", "allowed hosts")->default_value, "192.168.1.10");
}

TEST_F(CheckMkModule, InstallWarnsWhenOnlyThisMachineMayConnect) {
  const auto r = run_cli(module_, {"install"});

  EXPECT_EQ(r.result(), PB::Common::ResultCode::OK) << r.message();
  EXPECT_EQ(update_of("/settings/check_mk/server", "allowed hosts")->default_value, "127.0.0.1") << "inherits /settings/default";
  EXPECT_NE(r.message().find("WARNING: only this machine may connect"), std::string::npos) << r.message();
}

TEST_F(CheckMkModule, InstallAddsAndRemovesLocalChecksAndMrpeEntries) {
  const auto r =
      run_cli(module_, {"install", "--allowed-hosts", "10.0.0.5", "--local", "CPU Load=check_cpu warn=load>80", "--local", "Memory=command=check_memory",
                        "--mrpe", "Uptime_Check=check_uptime warn=uptime<2d", "--remove-local", "Old check", "--remove-mrpe", "Gone"});

  EXPECT_EQ(r.result(), PB::Common::ResultCode::OK) << r.message();
  ASSERT_NE(update_of("/settings/check_mk/server/local", "CPU Load"), nullptr);
  EXPECT_EQ(update_of("/settings/check_mk/server/local", "CPU Load")->default_value, "command=check_cpu warn=load>80");
  EXPECT_EQ(update_of("/settings/check_mk/server/local", "Memory")->default_value, "command=check_memory") << "no doubled prefix";
  EXPECT_EQ(update_of("/settings/check_mk/server/mrpe", "Uptime_Check")->default_value, "command=check_uptime warn=uptime<2d");
  // erase(): an update without store_empty.
  ASSERT_NE(update_of("/settings/check_mk/server/local", "Old check"), nullptr);
  EXPECT_FALSE(update_of("/settings/check_mk/server/local", "Old check")->store_empty);
  ASSERT_NE(update_of("/settings/check_mk/server/mrpe", "Gone"), nullptr);
  EXPECT_FALSE(update_of("/settings/check_mk/server/mrpe", "Gone")->store_empty);
}

TEST_F(CheckMkModule, InstallRejectsBadInputWithoutWritingAnything) {
  for (const std::vector<std::string> &args : std::vector<std::vector<std::string>>{
           {"install", "--allowed-hosts", " "},
           {"install", "--port", "65x"},
           {"install", "--local", "no equals sign"},
           {"install", "--local", "=check_cpu"},
           {"install", "--mrpe", "Name="},
           {"install", "--mrpe", "Uptime Check=check_uptime"},
       }) {
    core().reset();
    const auto r = run_cli(module_, args);
    EXPECT_EQ(r.result(), PB::Common::ResultCode::UNKNOWN) << args.back() << ": " << r.message();
    EXPECT_TRUE(core().updated_settings().empty()) << args.back();
  }
}

TEST_F(CheckMkModule, InstallHelpDescribesTheOptions) {
  const auto r = run_cli(module_, {"install", "--help"});

  EXPECT_EQ(r.result(), PB::Common::ResultCode::OK);
  EXPECT_NE(r.message().find("Usage: nscp mk install"), std::string::npos) << r.message();
  for (const char *option : {"allowed-hosts=ARG", "port=ARG", "local=ARG", "mrpe=ARG", "remove-local=ARG", "remove-mrpe=ARG"})
    EXPECT_NE(r.message().find(option), std::string::npos) << option;
  EXPECT_TRUE(core().updated_settings().empty());
}

TEST_F(CheckMkModule, ShowDescribesTheAgentConfiguration) {
  core().set_setting("/settings/check_mk/server", "port", "16556");
  core().set_setting("/settings/check_mk/server", "allowed hosts", "10.0.0.5");

  const auto r = run_cli(module_, {"show"});

  EXPECT_EQ(r.result(), PB::Common::ResultCode::OK) << r.message();
  EXPECT_NE(r.message().find("Port: 16556"), std::string::npos) << r.message();
  EXPECT_NE(r.message().find("Allowed hosts: 10.0.0.5"), std::string::npos) << r.message();
  EXPECT_TRUE(core().updated_settings().empty()) << "show must not write";
}

TEST(CheckMkInstall, EntryNameEndsAtTheFirstEquals) {
  const check_mk_install::entry e = check_mk_install::parse_entry(" CPU Load = check_cpu warn=load>80 ", "local");
  EXPECT_EQ(e.name, "CPU Load");
  EXPECT_EQ(e.command, "check_cpu warn=load>80");
  EXPECT_EQ(e.value(), "command=check_cpu warn=load>80");
}

TEST(CheckMkInstall, MrpeNamesCannotContainSpacesButLocalNamesCan) {
  EXPECT_EQ(check_mk_install::parse_entry("Disk C=check_drivesize drive=C:", "local").name, "Disk C");
  EXPECT_EQ(check_mk_install::parse_entry("Uptime_Check=check_uptime", "mrpe").name, "Uptime_Check");
  try {
    check_mk_install::parse_entry("Uptime Check=check_uptime", "mrpe");
    FAIL() << "a spaced MRPE name was accepted";
  } catch (const std::invalid_argument &e) {
    EXPECT_NE(std::string(e.what()).find("\"Uptime_Check\""), std::string::npos) << e.what();
  }
}

TEST(CheckMkInstall, CommandOfStripsThePrefix) {
  EXPECT_EQ(check_mk_install::command_of("command=check_cpu warn=load>80"), "check_cpu warn=load>80");
  EXPECT_EQ(check_mk_install::command_of("check_cpu"), "check_cpu");
}

TEST(CheckMkInstall, LoopbackOnlyRecognisesTheDefault) {
  EXPECT_TRUE(check_mk_install::loopback_only("127.0.0.1"));
  EXPECT_TRUE(check_mk_install::loopback_only("127.0.0.1, ::1"));
  EXPECT_FALSE(check_mk_install::loopback_only("127.0.0.1,10.0.0.5"));
  EXPECT_FALSE(check_mk_install::loopback_only("10.0.0.0/24"));
  EXPECT_FALSE(check_mk_install::loopback_only(""));
}
