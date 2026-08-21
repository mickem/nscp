// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The check bodies of the four Windows-oriented CheckDisk commands, exercised
// on Unix.
//
// Only their data acquisition is Windows-specific: check_share,
// check_storagepool and check_shadowcopy compile a query() that returns
// nothing here, and check_uncpath has a real Unix implementation over statvfs.
// Everything the command does around that - option parsing, the keyword
// registry, threshold evaluation, the empty-state contract and the rendering -
// is the same code the win32 build runs, and none of it was covered on this
// platform because the tests for these files live in a target that only exists
// under if(WIN32).
//
// So: drive the commands themselves. On a machine with no shares, no storage
// pools and no shadow copies (every Unix machine), what these three must do is
// report the documented empty state rather than fail - which is also exactly
// what a Windows box without Storage Spaces or VSS does.
//
// nscapi::plugin_singleton is defined once for this target in
// check_disk_unix_test.cpp.

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <string>
#include <vector>

#include "check_shadowcopy.hpp"
#include "check_share.hpp"
#include "check_storagepool.hpp"
#include "check_uncpath.hpp"

namespace {

std::string join_lines(const PB::Commands::QueryResponseMessage::Response &r) {
  std::string out;
  for (int i = 0; i < r.lines_size(); ++i) {
    if (!out.empty()) out += "\n";
    out += r.lines(i).message();
  }
  return out;
}

// Perf labels the check emitted, in order.
std::vector<std::string> perf_labels(const PB::Commands::QueryResponseMessage::Response &r) {
  std::vector<std::string> out;
  for (int i = 0; i < r.lines_size(); ++i) {
    for (const auto &p : r.lines(i).perf()) out.push_back(p.alias());
  }
  return out;
}

PB::Commands::QueryRequestMessage::Request request_for(const std::string &command, const std::vector<std::string> &args) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command(command);
  for (const std::string &a : args) request.add_arguments(a);
  return request;
}

}  // namespace

// ---------------------------------------------------------------------------
// check_share / check_storagepool / check_shadowcopy - the no-data contract.
// ---------------------------------------------------------------------------

TEST(ShareCommand, NoSharesIsOkAndSaysSo) {
  PB::Commands::QueryResponseMessage::Response response;
  share_check::check::check_share(request_for("check_share", {}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK) << join_lines(response);
  EXPECT_NE(join_lines(response).find("No shares"), std::string::npos) << join_lines(response);
}

TEST(ShareCommand, EmptyStateIsUserConfigurable) {
  // An operator who considers "no shares at all" a problem can say so, and the
  // empty-state has to be honoured rather than hard-coded to OK.
  PB::Commands::QueryResponseMessage::Response response;
  share_check::check::check_share(request_for("check_share", {"empty-state=critical"}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::CRITICAL) << join_lines(response);
}

TEST(ShareCommand, ARequiredShareThatDoesNotExistIsReported) {
  // Required mode emits a row per requested share whether or not it exists, so
  // a missing one can be alerted on. With no shares present the row is the
  // "missing" one, and the default critical expression (exists = 0) trips.
  PB::Commands::QueryResponseMessage::Response response;
  share_check::check::check_share(request_for("check_share", {"share=NoSuchShare"}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::CRITICAL) << join_lines(response);
  EXPECT_NE(join_lines(response).find("NoSuchShare"), std::string::npos) << join_lines(response);
}

TEST(ShareCommand, ABadFilterIsRejectedRatherThanIgnored) {
  PB::Commands::QueryResponseMessage::Response response;
  share_check::check::check_share(request_for("check_share", {"filter=no such keyword > 1"}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN) << join_lines(response);
}

TEST(StoragePoolCommand, NoPoolsIsOkAndSaysSo) {
  // A machine without Storage Spaces is not a fault: the check must not go
  // critical just because there are no pools.
  PB::Commands::QueryResponseMessage::Response response;
  storagepool_check::check::check_storagepool(request_for("check_storagepool", {}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK) << join_lines(response);
  EXPECT_NE(join_lines(response).find("No storage pools found"), std::string::npos) << join_lines(response);
}

TEST(StoragePoolCommand, EmptyStateIsUserConfigurable) {
  PB::Commands::QueryResponseMessage::Response response;
  storagepool_check::check::check_storagepool(request_for("check_storagepool", {"empty-state=warning"}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::WARNING) << join_lines(response);
}

TEST(StoragePoolCommand, ABadFilterIsRejectedRatherThanIgnored) {
  PB::Commands::QueryResponseMessage::Response response;
  storagepool_check::check::check_storagepool(request_for("check_storagepool", {"filter=no such keyword > 1"}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN) << join_lines(response);
}

TEST(ShadowCopyCommand, NoShadowCopiesIsOkAndSaysSo) {
  PB::Commands::QueryResponseMessage::Response response;
  shadowcopy_check::check::check_shadowcopy(request_for("check_shadowcopy", {}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK) << join_lines(response);
  EXPECT_NE(join_lines(response).find("No shadow copies found"), std::string::npos) << join_lines(response);
}

TEST(ShadowCopyCommand, EmptyStateIsUserConfigurable) {
  // Documented in the samples: a volume that is supposed to have snapshots can
  // be alerted on with empty-state=critical.
  PB::Commands::QueryResponseMessage::Response response;
  shadowcopy_check::check::check_shadowcopy(request_for("check_shadowcopy", {"empty-state=critical"}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::CRITICAL) << join_lines(response);
}

// ---------------------------------------------------------------------------
// check_uncpath - a real query on this platform (statvfs), so the whole
// command runs end to end.
// ---------------------------------------------------------------------------

TEST(UncPathCommand, WithoutAPathItSaysWhichArgumentIsMissing) {
  PB::Commands::QueryResponseMessage::Response response;
  uncpath_check::check::check_uncpath(request_for("check_uncpath", {}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN) << join_lines(response);
  EXPECT_NE(join_lines(response).find("No path specified"), std::string::npos) << join_lines(response);
}

TEST(UncPathCommand, AnExistingPathIsReportedWithItsSizes) {
  // On Unix the query is a statvfs of an already-mounted path; the root
  // filesystem is the one path every machine running this has.
  PB::Commands::QueryResponseMessage::Response response;
  uncpath_check::check::check_uncpath(request_for("check_uncpath", {"path=/", "warning=used_pct > 100", "critical=used_pct > 100"}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK) << join_lines(response);
  EXPECT_NE(join_lines(response).find('/'), std::string::npos) << join_lines(response);
}

TEST(UncPathCommand, EachMetricGetsItsOwnPerfSeries) {
  // Perf series follow the keywords the thresholds reference. The suffixes
  // exist so that two of them do not collapse onto the shared ${path} alias -
  // without them a graph shows one series for two different numbers.
  PB::Commands::QueryResponseMessage::Response response;
  uncpath_check::check::check_uncpath(request_for("check_uncpath", {"path=/", "warning=used_pct > 100 or free < 0", "critical=used_pct > 100"}), &response);

  const std::vector<std::string> labels = perf_labels(response);
  ASSERT_EQ(labels.size(), 2u) << join_lines(response);
  EXPECT_NE(labels[0], labels[1]) << "both metrics landed on one series";
  for (const std::string &suffix : {"_free", "_used_pct"}) {
    bool found = false;
    for (const std::string &l : labels) {
      if (l.size() >= suffix.size() && l.compare(l.size() - suffix.size(), suffix.size(), suffix) == 0) found = true;
    }
    EXPECT_TRUE(found) << "no perf series ending in " << suffix;
  }
}

TEST(UncPathCommand, AThresholdOnUsedSpaceTrips) {
  // used_pct is always >= 0, so this is the deterministic way to prove the
  // threshold reaches the data rather than testing the host's actual fullness.
  PB::Commands::QueryResponseMessage::Response response;
  uncpath_check::check::check_uncpath(request_for("check_uncpath", {"path=/", "critical=used_pct >= 0"}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::CRITICAL) << join_lines(response);
}

TEST(UncPathCommand, APathThatCannotBeQueriedIsAnError) {
  PB::Commands::QueryResponseMessage::Response response;
  const boost::filesystem::path missing = boost::filesystem::temp_directory_path() / "nscp-no-such-path-for-uncpath-test";
  ASSERT_FALSE(boost::filesystem::exists(missing));
  uncpath_check::check::check_uncpath(request_for("check_uncpath", {"path=" + missing.string()}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN) << join_lines(response);
  EXPECT_NE(join_lines(response).find(missing.string()), std::string::npos) << join_lines(response);
}

TEST(UncPathCommand, SeveralPathsAreEachReported) {
  PB::Commands::QueryResponseMessage::Response response;
  uncpath_check::check::check_uncpath(
      request_for("check_uncpath", {"path=/", "path=/tmp", "warning=used_pct > 100", "critical=used_pct > 100", "detail-syntax=${path}"}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK) << join_lines(response);
  const std::string message = join_lines(response);
  EXPECT_NE(message.find("/tmp"), std::string::npos) << message;
}
