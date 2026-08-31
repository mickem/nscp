// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Unit tests for the CheckDisk module class itself - the settings it parses in
// loadModuleEx() and the dispatch wrappers it puts in front of the individual
// checks. The checks themselves are covered by their own *_test.cpp files;
// what is tested here is only what the plugin shell adds on top of them:
//
//   * the collector settings: parsing, the 1s floor, the int ceiling, and the
//     fallback-to-default on anything unparseable;
//   * the "Collector not started" guard every collector-backed check opens
//     with, and the catch-all each of them wraps its check in;
//   * the deprecation responses the legacy shims return off Windows.
//
// None of those branches is reachable from the integration suite (a running
// daemon has a collector and valid settings by construction), which is exactly
// why they belong here.

#include "CheckDisk.h"

#include <gtest/gtest.h>

#include <nscapi/test_helpers.hpp>
#include <string>
#include <vector>

#include "test_support.hpp"

namespace {

using check_disk_test_support::join_lines;
using ScratchDir = check_disk_test_support::ScratchDir;

// The stub core (include/nscapi/test_helpers.hpp) answers the settings queries
// loadModuleEx makes; its state is process-wide, so it is reset per test.
class CheckDiskModule : public ::testing::Test {
 protected:
  nscapi::test_helpers::stub_core &core() { return nscapi::test_helpers::stub_core::instance(); }

  void SetUp() override {
    core().reset();
    module_.set_id(42);
  }
  void TearDown() override { core().reset(); }

  // dontStart, never normalStart: the collector settings are parsed either
  // way, but no collector thread is spawned and no host tag is published.
  bool load() { return module_.loadModuleEx("test_disk", NSCAPI::dontStart); }

  const collector_thread &collector() const { return *module_.get_collector(); }

  CheckDisk module_;
};

PB::Commands::QueryRequestMessage::Request make_request(const std::string &command, const std::vector<std::string> &args = {}) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command(command);
  for (const std::string &a : args) request.add_arguments(a);
  return request;
}

}  // namespace

// ============================================================================
// Collector settings: the defaults
// ============================================================================

TEST_F(CheckDiskModule, LoadModuleCreatesCollectorWithDefaults) {
  ASSERT_TRUE(load());
  ASSERT_NE(module_.get_collector(), nullptr);
  EXPECT_EQ(collector().collection_interval, 10);
  EXPECT_EQ(collector().trend_interval, 300);
  EXPECT_EQ(collector().trend_retention, 7 * 24 * 3600);
  EXPECT_EQ(collector().max_collection_errors, 10);
  EXPECT_TRUE(collector().disable_.empty());
}

TEST_F(CheckDiskModule, NoCollectorBeforeLoadModule) { EXPECT_EQ(module_.get_collector(), nullptr); }

// ============================================================================
// Collector settings: values that parse
// ============================================================================

TEST_F(CheckDiskModule, ConfiguredIntervalsAreApplied) {
  core().set_setting("collection interval", "30s");
  core().set_setting("trend interval", "10m");
  core().set_setting("trend retention", "1d");
  core().set_setting("max collection errors", "3");
  core().set_setting("disable", "disk_io,trend");

  ASSERT_TRUE(load());
  EXPECT_EQ(collector().collection_interval, 30);
  EXPECT_EQ(collector().trend_interval, 600);
  EXPECT_EQ(collector().trend_retention, 24 * 3600);
  EXPECT_EQ(collector().max_collection_errors, 3);
  EXPECT_EQ(collector().disable_, "disk_io,trend");
}

TEST_F(CheckDiskModule, BareNumberIsSeconds) {
  core().set_setting("collection interval", "45");

  ASSERT_TRUE(load());
  EXPECT_EQ(collector().collection_interval, 45);
}

// ============================================================================
// Collector settings: values that do not - each must fall back, not propagate
// ============================================================================

TEST_F(CheckDiskModule, CollectionIntervalBelowOneSecondFallsBackToDefault) {
  core().set_setting("collection interval", "0s");

  ASSERT_TRUE(load());
  EXPECT_EQ(collector().collection_interval, 10);
}

TEST_F(CheckDiskModule, CollectionIntervalPastIntMaxFallsBackToDefault) {
  // Would wrap to a negative wait in the collector, which then spins.
  core().set_setting("collection interval", "999999999999s");

  ASSERT_TRUE(load());
  EXPECT_EQ(collector().collection_interval, 10);
}

TEST_F(CheckDiskModule, UnparseableCollectionIntervalFallsBackToDefault) {
  core().set_setting("collection interval", "banana");

  ASSERT_TRUE(load());
  EXPECT_EQ(collector().collection_interval, 10);
}

TEST_F(CheckDiskModule, NonPositiveTrendIntervalFallsBackToDefaults) {
  core().set_setting("trend interval", "0");
  core().set_setting("trend retention", "1d");

  ASSERT_TRUE(load());
  // Interval and retention are validated together, so both go back to their
  // defaults - a 1d retention with a bad interval is not carried over.
  EXPECT_EQ(collector().trend_interval, 300);
  EXPECT_EQ(collector().trend_retention, 7 * 24 * 3600);
}

TEST_F(CheckDiskModule, UnparseableTrendRetentionFallsBackToDefaults) {
  core().set_setting("trend retention", "banana");

  ASSERT_TRUE(load());
  EXPECT_EQ(collector().trend_interval, 300);
  EXPECT_EQ(collector().trend_retention, 7 * 24 * 3600);
}

TEST_F(CheckDiskModule, NegativeMaxCollectionErrorsIsClampedToNeverGiveUp) {
  core().set_setting("max collection errors", "-1");

  ASSERT_TRUE(load());
  EXPECT_EQ(collector().max_collection_errors, 0);
}

// A bad collection interval must not take the trend settings down with it, nor
// the other way around: they are parsed in separate try blocks.
TEST_F(CheckDiskModule, BadCollectionIntervalLeavesTrendSettingsIntact) {
  core().set_setting("collection interval", "banana");
  core().set_setting("trend interval", "10m");
  core().set_setting("trend retention", "2d");

  ASSERT_TRUE(load());
  EXPECT_EQ(collector().collection_interval, 10);
  EXPECT_EQ(collector().trend_interval, 600);
  EXPECT_EQ(collector().trend_retention, 2 * 24 * 3600);
}

// ============================================================================
// unloadModule
// ============================================================================

TEST_F(CheckDiskModule, UnloadWithoutLoadIsSafe) { EXPECT_TRUE(module_.unloadModule()); }

TEST_F(CheckDiskModule, UnloadStopsANeverStartedCollector) {
  ASSERT_TRUE(load());
  EXPECT_TRUE(module_.unloadModule());
  // Idempotent: the module is unloaded twice on a reload race.
  EXPECT_TRUE(module_.unloadModule());
}

// ============================================================================
// The collector-backed checks refuse to run without a collector
// ============================================================================

TEST_F(CheckDiskModule, CheckDiskIoWithoutCollectorIsUnknown) {
  PB::Commands::QueryResponseMessage::Response response;
  module_.check_disk_io(make_request("check_disk_io"), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_NE(join_lines(response).find("Collector not started"), std::string::npos) << join_lines(response);
}

TEST_F(CheckDiskModule, CheckDiskHealthWithoutCollectorIsUnknown) {
  PB::Commands::QueryResponseMessage::Response response;
  module_.check_disk_health(make_request("check_disk_health"), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_NE(join_lines(response).find("Collector not started"), std::string::npos) << join_lines(response);
}

// With a collector present (but never started, so its data is empty) the same
// checks go down the real path instead.
TEST_F(CheckDiskModule, CheckDiskIoWithIdleCollectorRuns) {
  ASSERT_TRUE(load());
  PB::Commands::QueryResponseMessage::Response response;
  module_.check_disk_io(make_request("check_disk_io", {"empty-state=ok"}), &response);

  EXPECT_EQ(join_lines(response).find("Collector not started"), std::string::npos);
  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK) << join_lines(response);
}

TEST_F(CheckDiskModule, CheckDiskHealthWithIdleCollectorRuns) {
  ASSERT_TRUE(load());
  PB::Commands::QueryResponseMessage::Response response;
  module_.check_disk_health(make_request("check_disk_health", {"empty-state=ok"}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK) << join_lines(response);
}

// ============================================================================
// Dispatch wrappers that only delegate: the response must come back from the
// check, not from the wrapper's catch block.
// ============================================================================

TEST_F(CheckDiskModule, CheckSingleFileDelegates) {
  const ScratchDir dir;
  const std::string file = dir.make_file("hello.txt", "hello world");

  PB::Commands::QueryResponseMessage::Response response;
  module_.check_single_file(make_request("check_single_file", {"file=" + file}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK) << join_lines(response);
}

TEST_F(CheckDiskModule, CheckFilesDelegates) {
  const ScratchDir dir;
  dir.touch("a.txt");

  PB::Commands::QueryResponseMessage::Response response;
  // top-syntax=${list} so the scanned file names reach the rendered message -
  // the default top syntax only reports a count.
  module_.check_files(make_request("check_files", {"path=" + dir.string(), "pattern=*.txt", "detail-syntax=${filename}", "top-syntax=${list}"}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK) << join_lines(response);
  EXPECT_NE(join_lines(response).find("a.txt"), std::string::npos) << join_lines(response);
}

TEST_F(CheckDiskModule, CheckDiskWriteDelegates) {
  // The only check here that needs no collector and writes for real; a 2k
  // round trip inside the scratch directory keeps it hermetic.
  const ScratchDir dir;

  PB::Commands::QueryResponseMessage::Response response;
  module_.check_disk_write(make_request("check_disk_write", {"file=" + dir.string() + "/probe.dat", "size=2k"}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK) << join_lines(response);
}

TEST_F(CheckDiskModule, CheckMountDelegates) {
  PB::Commands::QueryResponseMessage::Response response;
  module_.check_mount(make_request("check_mount", {"mount=/"}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK) << join_lines(response);
}

TEST_F(CheckDiskModule, CheckDrivesizeDelegatesWithoutTrends) {
  // No collector, so the trend map handed to the check is the empty static
  // one - the check must still run and render.
  PB::Commands::QueryResponseMessage::Response response;
  module_.check_drivesize(make_request("check_drivesize", {"drive=/", "warning=none", "critical=none", "empty-state=ok"}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK) << join_lines(response);
}

// ============================================================================
// The Windows-only checks: off Windows their data acquisition is compiled out,
// so what the module contributes is the wrapper. Each must return a response
// rather than propagating an exception out of the plugin.
// ============================================================================

TEST_F(CheckDiskModule, WindowsOnlyChecksReturnAResponse) {
  {
    PB::Commands::QueryResponseMessage::Response response;
    module_.check_uncpath(make_request("check_uncpath", {"path=\\\\host\\share"}), &response);
    EXPECT_FALSE(join_lines(response).empty());
  }
  {
    PB::Commands::QueryResponseMessage::Response response;
    module_.check_storagepool(make_request("check_storagepool", {"empty-state=ok"}), &response);
    EXPECT_EQ(response.result(), PB::Common::ResultCode::OK) << join_lines(response);
  }
  {
    PB::Commands::QueryResponseMessage::Response response;
    module_.check_shadowcopy(make_request("check_shadowcopy", {"empty-state=ok"}), &response);
    EXPECT_EQ(response.result(), PB::Common::ResultCode::OK) << join_lines(response);
  }
  {
    PB::Commands::QueryResponseMessage::Response response;
    module_.check_share(make_request("check_share", {"empty-state=ok"}), &response);
    EXPECT_EQ(response.result(), PB::Common::ResultCode::OK) << join_lines(response);
  }
}

// ============================================================================
// Legacy shims - deprecated and unsupported off Windows
// ============================================================================

TEST_F(CheckDiskModule, LegacyCheckDriveSizeIsUnsupportedHere) {
  PB::Commands::QueryRequestMessage::Request request = make_request("CheckDriveSize", {"CheckAll=true"});
  PB::Commands::QueryResponseMessage::Response response;
  module_.checkDriveSize(request, &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN);
  const std::string out = join_lines(response);
  EXPECT_NE(out.find("deprecated legacy command"), std::string::npos) << out;
  EXPECT_NE(out.find("check_drivesize"), std::string::npos) << out;
}

TEST_F(CheckDiskModule, LegacyCheckFilesIsUnsupportedHere) {
  PB::Commands::QueryRequestMessage::Request request = make_request("CheckFiles", {"path=/tmp"});
  PB::Commands::QueryResponseMessage::Response response;
  module_.checkFiles(request, &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN);
  const std::string out = join_lines(response);
  EXPECT_NE(out.find("deprecated legacy command"), std::string::npos) << out;
  EXPECT_NE(out.find("check_files"), std::string::npos) << out;
}

// ============================================================================
// Metrics
// ============================================================================

TEST_F(CheckDiskModule, FetchMetricsWithoutCollectorProducesNothing) {
  PB::Metrics::MetricsMessage::Response response;
  module_.fetchMetrics(&response);

  EXPECT_EQ(response.bundles_size(), 0);
}

TEST_F(CheckDiskModule, FetchMetricsWithIdleCollectorProducesTheDiskBundle) {
  ASSERT_TRUE(load());
  PB::Metrics::MetricsMessage::Response response;
  module_.fetchMetrics(&response);

  ASSERT_EQ(response.bundles_size(), 1);
  EXPECT_EQ(response.bundles(0).key(), "disk");
  // Nothing has been collected, so the bundle carries no io/free sections.
  EXPECT_EQ(response.bundles(0).children_size(), 0);
}
