// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// check_drivesize's used_change_1h / used_change_24h keywords, run through the
// real check against the system drive with a hand-made change map standing in
// for the collector's history.

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "check_drive.hpp"
#include "test_support.hpp"

namespace {

#ifdef WIN32
// The collector keys a drive without its trailing backslash.
const char *const kKey = "C:";
const char *const kDrive = "C:";
#else
const char *const kKey = "/";
const char *const kDrive = "/";
#endif

PB::Commands::QueryResponseMessage::Response run(const std::vector<std::string> &args, const check_drive::change_map &changes) {
  PB::Commands::QueryRequestMessage::Request request;
  PB::Commands::QueryResponseMessage::Response response;
  request.set_command("check_drivesize");
  request.add_arguments(std::string("drive=") + kDrive);
  for (const std::string &a : args) request.add_arguments(a);
  check_drive::check(request, &response, check_drive::trend_map(), changes);
  return response;
}

const PB::Common::PerformanceData *perf_with_unit(const PB::Commands::QueryResponseMessage::Response &r, const std::string &unit) {
  for (int l = 0; l < r.lines_size(); ++l) {
    for (int p = 0; p < r.lines(l).perf_size(); ++p) {
      const PB::Common::PerformanceData &pd = r.lines(l).perf(p);
      if (pd.has_float_value() && pd.float_value().unit() == unit) return &pd;
    }
  }
  return nullptr;
}

}  // namespace

TEST(CheckDriveUsedChange, WithoutHistoryItIsUnknownAndNoThresholdFires) {
  const auto r = run({"warning=used_change_1h > 0 or used_change_24h < 0", "critical=used_change_1h > 1G", "detail-syntax=%(drive) ${used_change_1h} ${used_change_24h}",
                      "top-syntax=${list}"},
                     check_drive::change_map());
  EXPECT_EQ(r.result(), PB::Common::ResultCode::OK) << check_disk_test_support::join_lines(r);
  EXPECT_NE(check_disk_test_support::join_lines(r).find("unknown unknown"), std::string::npos) << check_disk_test_support::join_lines(r);
  // ...and no perfdata for a value that does not exist.
  EXPECT_EQ(perf_with_unit(r, "B"), nullptr);
}

TEST(CheckDriveUsedChange, AJumpFiresAndRendersSigned) {
  check_drive::change_map changes;
  changes[kKey].change_1h = 3LL * 1024 * 1024 * 1024;
  changes[kKey].change_24h = -1024LL * 1024;
  const auto r = run({"warning=used_change_1h > 1G", "critical=used_change_1h > 10G", "detail-syntax=${used_change_1h} ${used_change_24h}", "top-syntax=${list}"},
                     changes);
  EXPECT_EQ(r.result(), PB::Common::ResultCode::WARNING) << check_disk_test_support::join_lines(r);
  EXPECT_NE(check_disk_test_support::join_lines(r).find("+3GB -1MB"), std::string::npos) << check_disk_test_support::join_lines(r);
  const PB::Common::PerformanceData *pd = perf_with_unit(r, "B");
  ASSERT_NE(pd, nullptr);
  EXPECT_EQ(pd->float_value().value(), 3.0 * 1024 * 1024 * 1024);

  // Freeing space is a negative change, below any growth threshold.
  check_drive::change_map freed;
  freed[kKey].change_1h = -5LL * 1024 * 1024 * 1024;
  EXPECT_EQ(run({"warning=used_change_1h > 1G"}, freed).result(), PB::Common::ResultCode::OK);
  EXPECT_EQ(run({"warning=used_change_1h < -1G"}, freed).result(), PB::Common::ResultCode::WARNING);
}

TEST(CheckDriveUsedChange, TheTotalRowAddsUpTheDrivesThatHaveOne) {
  check_drive::change_map changes;
  changes[kKey].change_1h = 2048;
  const auto r = run({"total", "filter=drive = 'total'", "detail-syntax=%(drive) ${used_change_1h} ${used_change_24h}", "top-syntax=${list}"}, changes);
  EXPECT_NE(check_disk_test_support::join_lines(r).find("total +2KB unknown"), std::string::npos) << check_disk_test_support::join_lines(r);
}
