// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Unit tests for the CheckNSCP module class: the update-check settings it
// registers in loadModuleEx(), the error counter fed by the log stream, and
// check_nscp - the agent's own health check, whose crash and error inputs are
// both reachable from here.
//
// check_nscp_update talks to the GitHub releases API, so only its
// no-network paths are exercised: an unusable running version, and the option
// parsing that runs before any request is made.

#include "CheckNSCP.h"

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <fstream>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/test_helpers.hpp>
#include <string>
#include <vector>

// Test binaries have no generated module glue, so the plugin singleton
// (normally provided by NSC_WRAP_DLL()) must be defined here.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

namespace fs = boost::filesystem;

namespace {

std::string join_lines(const PB::Commands::QueryResponseMessage::Response &r) {
  std::string out;
  for (int i = 0; i < r.lines_size(); ++i) {
    if (!out.empty()) out += "\n";
    out += r.lines(i).message();
  }
  return out;
}

class CheckNscpModule : public ::testing::Test {
 protected:
  nscapi::test_helpers::stub_core &core() { return nscapi::test_helpers::stub_core::instance(); }

  void SetUp() override {
    core().reset();
    module_.set_id(42);
    // The crash folder is read from the core's own /settings/crash section, so
    // it is pointed at an empty scratch directory: a crash report left on the
    // developer's machine must not decide the outcome of a test.
    crash_dir_ = fs::temp_directory_path() / fs::unique_path("nscp-crash-%%%%%%%%");
    fs::create_directories(crash_dir_);
    core().set_setting("/settings/crash", "archive folder", crash_dir_.string());
  }
  void TearDown() override {
    core().reset();
    boost::system::error_code ec;
    fs::remove_all(crash_dir_, ec);
  }

  bool load() { return module_.loadModuleEx("", NSCAPI::dontStart); }

  PB::Commands::QueryRequestMessage::Request request(const std::string &command, const std::vector<std::string> &args = {}) const {
    PB::Commands::QueryRequestMessage::Request r;
    r.set_command(command);
    for (const std::string &a : args) r.add_arguments(a);
    return r;
  }

  void log_error(const std::string &message) {
    PB::Log::LogEntry::Entry entry;
    entry.set_level(PB::Log::LogEntry_Entry_Level_LOG_ERROR);
    entry.set_message(message);
    module_.handleLogMessage(entry);
  }

  fs::path crash_dir_;
  CheckNSCP module_;
};

}  // namespace

// ============================================================================
// The settings contract
// ============================================================================

TEST_F(CheckNscpModule, LoadRegistersTheUpdateKeysWithTheDocumentedDefaults) {
  ASSERT_TRUE(load());

  EXPECT_EQ(core().default_for("cache hours"), "24");
  EXPECT_EQ(core().default_for("check experimental"), "false");
  EXPECT_EQ(core().default_for("url"), "https://api.github.com/repos/mickem/nscp/releases");
}

// The update check reaches out over TLS, so its defaults are security
// settings: verification on, TLS 1.2 as the floor, system CA store.
TEST_F(CheckNscpModule, UpdateTlsDefaultsAreTheHardenedOnes) {
  ASSERT_TRUE(load());

  EXPECT_EQ(core().default_for("tls version"), "tlsv1.2+");
  EXPECT_EQ(core().default_for("verify mode"), "peer");
  EXPECT_EQ(core().default_for("ca"), "${ca-path}");
}

// The crash archive folder belongs to the core, so the module reads it rather
// than registering it - registering it again would document a core setting on
// this module's reference page.
TEST_F(CheckNscpModule, CrashArchiveFolderIsReadNotRegistered) {
  ASSERT_TRUE(load());

  for (const nscapi::test_helpers::registered_key &key : core().registered_keys()) {
    EXPECT_NE(key.key, "archive folder") << "the core's crash setting is registered by this module";
  }
}

// ============================================================================
// The error counter, fed by the log stream
// ============================================================================

TEST_F(CheckNscpModule, OnlyErrorAndCriticalLogEntriesAreCounted) {
  ASSERT_TRUE(load());

  PB::Log::LogEntry::Entry info;
  info.set_level(PB::Log::LogEntry_Entry_Level_LOG_INFO);
  info.set_message("just info");
  module_.handleLogMessage(info);

  std::string last;
  EXPECT_EQ(module_.get_errors(last), 0u);

  log_error("something broke");
  EXPECT_EQ(module_.get_errors(last), 1u);
  EXPECT_EQ(last, "something broke");
}

TEST_F(CheckNscpModule, CriticalLogEntriesAreCountedToo) {
  ASSERT_TRUE(load());

  PB::Log::LogEntry::Entry critical;
  critical.set_level(PB::Log::LogEntry_Entry_Level_LOG_CRITICAL);
  critical.set_message("fatal");
  module_.handleLogMessage(critical);

  std::string last;
  EXPECT_EQ(module_.get_errors(last), 1u);
  EXPECT_EQ(last, "fatal");
}

TEST_F(CheckNscpModule, LastErrorIsTheMostRecentOne) {
  ASSERT_TRUE(load());

  log_error("first");
  log_error("second");

  std::string last;
  EXPECT_EQ(module_.get_errors(last), 2u);
  EXPECT_EQ(last, "second");
}

// ============================================================================
// check_nscp: the agent's own health
// ============================================================================

TEST_F(CheckNscpModule, HealthyAgentIsOk) {
  ASSERT_TRUE(load());

  PB::Commands::QueryResponseMessage::Response response;
  module_.check_nscp(request("check_nscp"), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK) << join_lines(response);
  const std::string out = join_lines(response);
  EXPECT_NE(out.find("0 crash(es)"), std::string::npos) << out;
  EXPECT_NE(out.find("0 error(s)"), std::string::npos) << out;
  EXPECT_NE(out.find("uptime"), std::string::npos) << out;
}

// The default thresholds preserve the historical verdict: any logged error
// makes the agent CRITICAL.
TEST_F(CheckNscpModule, ALoggedErrorMakesTheAgentCritical) {
  ASSERT_TRUE(load());
  log_error("something broke");

  PB::Commands::QueryResponseMessage::Response response;
  module_.check_nscp(request("check_nscp"), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::CRITICAL) << join_lines(response);
  EXPECT_NE(join_lines(response).find("1 error(s)"), std::string::npos) << join_lines(response);
}

TEST_F(CheckNscpModule, ACrashReportMakesTheAgentCritical) {
  ASSERT_TRUE(load());
  std::ofstream((crash_dir_ / "crash-1.txt").string()) << "boom";

  PB::Commands::QueryResponseMessage::Response response;
  module_.check_nscp(request("check_nscp"), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::CRITICAL) << join_lines(response);
  EXPECT_NE(join_lines(response).find("1 crash(es)"), std::string::npos) << join_lines(response);
}

// Thresholds are ordinary filter options, so an administrator can decide that
// errors alone are not critical.
TEST_F(CheckNscpModule, ThresholdsCanBeOverridden) {
  ASSERT_TRUE(load());
  log_error("something broke");

  PB::Commands::QueryResponseMessage::Response response;
  module_.check_nscp(request("check_nscp", {"crit=crashes > 0", "warn=none"}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK) << join_lines(response);
}

TEST_F(CheckNscpModule, InvalidMaxUnitIsRejected) {
  ASSERT_TRUE(load());

  PB::Commands::QueryResponseMessage::Response response;
  module_.check_nscp(request("check_nscp", {"max-unit=banana"}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN) << join_lines(response);
}

// ============================================================================
// check_nscp_version
// ============================================================================

TEST_F(CheckNscpModule, VersionCheckRendersTheRunningVersion) {
  core().set_application_version("0.10.5.2020 2026-01-01");
  ASSERT_TRUE(load());

  PB::Commands::QueryResponseMessage::Response response;
  module_.check_nscp_version(request("check_nscp_version"), &response);

  EXPECT_NE(join_lines(response).find("0.10.5.2020"), std::string::npos) << join_lines(response);
}

// ============================================================================
// check_nscp_update: only what happens before the network is touched
// ============================================================================

// Without a running version there is nothing to compare against, so the check
// must bail before querying GitHub.
TEST_F(CheckNscpModule, UpdateCheckWithoutARunningVersionFailsWithoutQuerying) {
  ASSERT_TRUE(load());

  PB::Commands::QueryResponseMessage::Response response;
  module_.check_nscp_update(request("check_nscp_update"), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN) << join_lines(response);
  EXPECT_NE(join_lines(response).find("Failed to parse running version"), std::string::npos) << join_lines(response);
}

TEST_F(CheckNscpModule, UpdateCheckRejectsBadOptionsBeforeQuerying) {
  ASSERT_TRUE(load());

  PB::Commands::QueryResponseMessage::Response response;
  module_.check_nscp_update(request("check_nscp_update", {"not-an-option=1"}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN) << join_lines(response);
}

TEST_F(CheckNscpModule, UnloadIsSafe) { EXPECT_TRUE(module_.unloadModule()); }
