// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Unit tests for the NCPAServer module class: the settings it registers and
// reads in loadModuleEx(), the configurations the listener refuses to start
// on, and the `nscp ncpa token` subcommand, which only writes settings.
//
// The wire contract (path and argument splitting, the token check, the JSON
// bodies) has its own test in ncpa_protocol_test.cpp; this covers the plugin
// shell around it.
//
// No test here opens a socket. The settings-only loads use dontStart. The
// refusing loads use reloadStart - which, unlike dontStart, goes on to start
// the listener - and every one of them stops before the server binds: a
// `plugins` value that does not parse returns before anything is built, and
// an empty or broken certificate is refused before start(). An empty
// certificate is also what keeps a default one from being generated.

#include "NCPAServer.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <boost/filesystem.hpp>
#include <fstream>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/test_helpers.hpp>
#include <string>
#include <vector>

// Test binaries have no generated module glue, so the plugin singleton
// (normally provided by NSC_WRAP_DLL()) must be defined here.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

namespace {

const char *const kRoot = "/settings/NCPA/server";
const char *const kNotStarted = "The NCPA listener has NOT been started";
const char *const kHashed = "pbkdf2-sha256$100000$00112233445566778899aabbccddeeff$0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

class NcpaModule : public ::testing::Test {
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

  bool load(const NSCAPI::moduleLoadMode mode = NSCAPI::dontStart) { return module_.loadModuleEx("", mode); }

  // NSC_LOG_ERROR goes to stdout in a unit-test build, the rest to the core.
  bool logged(const std::string &needle) const { return out_.contains(needle) || nscapi::test_helpers::stub_core::instance().has_logged(needle); }

  // Run one `nscp ncpa <argv...>` invocation.
  PB::Commands::ExecuteResponseMessage::Response cli(const std::vector<std::string> &args, bool *handled = nullptr, const int target = NSCAPI::target_module,
                                                     const std::string &command = "ncpa") {
    PB::Commands::ExecuteRequestMessage request_message;
    PB::Commands::ExecuteRequestMessage::Request request;
    request.set_command(command);
    for (const std::string &a : args) request.add_arguments(a);
    PB::Commands::ExecuteResponseMessage::Response response;
    const bool ret = module_.commandLineExec(target, request, &response, request_message);
    if (handled != nullptr) *handled = ret;
    return response;
  }

  nscapi::test_helpers::stdout_capture out_;
  NCPAServer module_;
};

}  // namespace

// ============================================================================
// The settings contract
// ============================================================================

TEST_F(NcpaModule, LoadRegistersTheServerKeysWithTheDocumentedDefaults) {
  ASSERT_TRUE(load());

  EXPECT_EQ(core().default_for(kRoot, "port"), "5693");
  EXPECT_EQ(core().default_for(kRoot, "token"), "");
  EXPECT_EQ(core().default_for(kRoot, "backup token"), "");
  EXPECT_EQ(core().default_for(kRoot, "allow arguments"), "false");
  EXPECT_EQ(core().default_for(kRoot, "plugins"), "any");
  EXPECT_EQ(core().default_for(kRoot, "auth rate limit max failures"), "10");
  EXPECT_EQ(core().default_for(kRoot, "auth rate limit block seconds"), "60");
  EXPECT_EQ(core().default_for(kRoot, "threads"), "10");
  EXPECT_EQ(core().default_for(kRoot, "allow insecure"), "false");
}

TEST_F(NcpaModule, TlsDefaultsShareTheWebServerCertificate) {
  ASSERT_TRUE(load());

  EXPECT_EQ(core().default_for(kRoot, "certificate"), "${certificate-path}/certificate.pem");
  EXPECT_EQ(core().default_for(kRoot, "certificate key"), "");
  EXPECT_EQ(core().default_for(kRoot, "tls version"), "1.2+");
  EXPECT_EQ(core().default_for(kRoot, "allowed ciphers"), "");
}

TEST_F(NcpaModule, LogKeysLiveInTheirOwnSubsection) {
  ASSERT_TRUE(load());

  const std::string log = std::string(kRoot) + "/log";
  EXPECT_EQ(core().default_for(log, "error"), "true");
  EXPECT_EQ(core().default_for(log, "info"), "false");
  EXPECT_EQ(core().default_for(log, "debug"), "false");
  EXPECT_FALSE(core().has_key(kRoot, "error"));
}

// The listener shares bind/allowed-hosts with the other socket servers: the
// keys are published under /settings/default, and again under the module's
// own section (where an override goes) with the same default.
TEST_F(NcpaModule, SharedServerKeysAreRegisteredUnderTheDefaultSectionToo) {
  ASSERT_TRUE(load());

  EXPECT_EQ(core().default_for("/settings/default", "allowed hosts"), "127.0.0.1");
  EXPECT_EQ(core().default_for("/settings/default", "cache allowed hosts"), "true");
  EXPECT_TRUE(core().has_key("/settings/default", "bind to"));
  EXPECT_EQ(core().default_for(kRoot, "allowed hosts"), "127.0.0.1");
  EXPECT_TRUE(core().has_key(kRoot, "bind to"));
}

TEST_F(NcpaModule, BothTokensAreRegisteredAsSensitive) {
  ASSERT_TRUE(load());

  EXPECT_TRUE(core().is_sensitive("token"));
  EXPECT_TRUE(core().is_sensitive("backup token"));
  EXPECT_FALSE(core().is_sensitive("plugins"));
}

TEST_F(NcpaModule, LoadRegistersTheSectionPaths) {
  ASSERT_TRUE(load());

  const std::vector<std::string> &paths = core().registered_paths();
  EXPECT_NE(std::find(paths.begin(), paths.end(), kRoot), paths.end());
  EXPECT_NE(std::find(paths.begin(), paths.end(), std::string(kRoot) + "/log"), paths.end());
}

// ============================================================================
// What a dontStart load does not do
// ============================================================================

// dontStart is `nscp settings` / the docs extractor: it must load whatever
// the configuration, and must not complain about what only matters to a
// running listener.
TEST_F(NcpaModule, DontStartLoadsABrokenConfigurationSilently) {
  core().set_setting("plugins", " , ");
  core().set_setting("certificate", "");

  ASSERT_TRUE(load());
  EXPECT_FALSE(logged("NCPA:")) << out_.str();
}

// ============================================================================
// Refusing to start
// ============================================================================

TEST_F(NcpaModule, AnEmptyPluginListIsRefusedBeforeAnythingIsBuilt) {
  core().set_setting("plugins", " , ");

  ASSERT_TRUE(load(NSCAPI::reloadStart)) << "a bad value must not unload the module";
  EXPECT_TRUE(logged("'plugins' is empty"));
  EXPECT_TRUE(logged(kNotStarted));
  EXPECT_FALSE(logged("no token is configured")) << "the policy is checked first";
}

TEST_F(NcpaModule, NoCertificateIsRefusedRatherThanServedInClear) {
  core().set_setting("certificate", "");
  core().set_setting("token", "s3cret");

  ASSERT_TRUE(load(NSCAPI::reloadStart));
  EXPECT_TRUE(logged("no certificate is configured: refusing to start"));
  EXPECT_TRUE(logged(kNotStarted));
  EXPECT_FALSE(logged("no token is configured"));
}

TEST_F(NcpaModule, AMissingTokenIsReported) {
  core().set_setting("certificate", "");

  ASSERT_TRUE(load(NSCAPI::reloadStart));
  EXPECT_TRUE(logged("no token is configured under /settings/NCPA/server"));
}

// `allow insecure` covers a missing certificate only: one that is there but
// does not load must never turn into a cleartext listener.
TEST_F(NcpaModule, ACertificateThatDoesNotLoadIsRefusedEvenWhenInsecureIsAllowed) {
  const boost::filesystem::path dir = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("ncpa-test-%%%%-%%%%");
  boost::filesystem::create_directories(dir);
  const boost::filesystem::path cert = dir / "certificate.pem";
  {
    std::ofstream out(cert.string().c_str());
    out << "this is not a certificate\n";
  }
  core().set_setting("certificate", cert.string());
  core().set_setting("allow insecure", "true");
  core().set_setting("token", "s3cret");

  ASSERT_TRUE(load(NSCAPI::reloadStart));
  EXPECT_TRUE(logged("could not be loaded: refusing to start the NCPA listener in cleartext HTTP"));
  EXPECT_TRUE(logged(kNotStarted));
  EXPECT_FALSE(logged("serving UNENCRYPTED HTTP"));

  boost::system::error_code ec;
  boost::filesystem::remove_all(dir, ec);
}

// A reload re-enters loadModuleEx on the live module: stopping the (absent)
// listener first must be harmless, and the keys are registered again.
TEST_F(NcpaModule, ReloadingTwiceIsSafe) {
  core().set_setting("certificate", "");

  ASSERT_TRUE(load(NSCAPI::reloadStart));
  ASSERT_TRUE(load(NSCAPI::reloadStart));
  EXPECT_TRUE(module_.unloadModule());
  EXPECT_TRUE(module_.unloadModule()) << "unload is idempotent";
}

TEST_F(NcpaModule, PrepareShutdownWithoutAListenerIsSafe) {
  ASSERT_TRUE(load());
  module_.prepareShutdown();
  EXPECT_TRUE(module_.unloadModule());
}

// ============================================================================
// `nscp ncpa token`
// ============================================================================

TEST_F(NcpaModule, TokenSetStoresAHashNotTheToken) {
  ASSERT_TRUE(load());

  bool handled = false;
  const PB::Commands::ExecuteResponseMessage::Response r = cli({"token", "--set", "s3cret"}, &handled);

  EXPECT_TRUE(handled);
  EXPECT_EQ(r.result(), PB::Common::ResultCode::OK) << r.message();
  ASSERT_TRUE(core().was_updated("token"));
  const std::string stored = core().updated_value("token");
  EXPECT_EQ(stored.rfind("pbkdf2-sha256$", 0), 0u) << stored;
  EXPECT_EQ(stored.find("s3cret"), std::string::npos);
  EXPECT_FALSE(core().was_updated("backup token"));
  EXPECT_NE(r.message().find("stored hashed"), std::string::npos) << r.message();
  EXPECT_EQ(core().updated_settings().front().path, kRoot);
}

TEST_F(NcpaModule, TokenSetBackupWritesTheBackupToken) {
  ASSERT_TRUE(load());

  const PB::Commands::ExecuteResponseMessage::Response r = cli({"token", "--set", "s3cret", "--backup"});

  EXPECT_EQ(r.result(), PB::Common::ResultCode::OK) << r.message();
  EXPECT_TRUE(core().was_updated("backup token"));
  EXPECT_FALSE(core().was_updated("token"));
  EXPECT_NE(r.message().find("The backup token under /settings/NCPA/server has been updated"), std::string::npos) << r.message();
}

// A hash copied from another agent is stored as it is, not hashed again.
TEST_F(NcpaModule, TokenSetKeepsAnAlreadyHashedValue) {
  ASSERT_TRUE(load());

  cli({"token", "--set", kHashed});
  EXPECT_EQ(core().updated_value("token"), kHashed);
}

TEST_F(NcpaModule, TokenWithoutSetPrintsUsageAndWritesNothing) {
  ASSERT_TRUE(load());

  bool handled = false;
  const PB::Commands::ExecuteResponseMessage::Response r = cli({"token"}, &handled);

  EXPECT_TRUE(handled);
  EXPECT_NE(r.result(), PB::Common::ResultCode::OK);
  EXPECT_NE(r.message().find("Set the token"), std::string::npos) << r.message();
  EXPECT_TRUE(core().updated_settings().empty());
}

TEST_F(NcpaModule, TokenHelpIsAGoodResponse) {
  ASSERT_TRUE(load());

  const PB::Commands::ExecuteResponseMessage::Response r = cli({"token", "--help"});
  EXPECT_EQ(r.result(), PB::Common::ResultCode::OK);
  EXPECT_NE(r.message().find("Set `backup token` instead of `token`"), std::string::npos) << r.message();
  EXPECT_TRUE(core().updated_settings().empty());
}

// `nscp --module NCPAServer token ...` names the subcommand as the command.
TEST_F(NcpaModule, TheSubcommandMayArriveAsTheCommand) {
  ASSERT_TRUE(load());

  cli({"token", "--set", "s3cret"}, nullptr, NSCAPI::target_module, "token");
  EXPECT_TRUE(core().was_updated("token"));
}

TEST_F(NcpaModule, AnUnknownSubcommandAnswersWithUsage) {
  ASSERT_TRUE(load());

  bool handled = false;
  const PB::Commands::ExecuteResponseMessage::Response r = cli({"rotate"}, &handled);
  EXPECT_TRUE(handled);
  EXPECT_NE(r.result(), PB::Common::ResultCode::OK);
  EXPECT_EQ(r.message(), "Usage: nscp ncpa token --help");
}

// Outside module mode a command that is not ours is left to other modules.
TEST_F(NcpaModule, AForeignCommandIsNotClaimed) {
  ASSERT_TRUE(load());

  bool handled = true;
  cli({}, &handled, NSCAPI::target_any, "something-else");
  EXPECT_FALSE(handled);
}
