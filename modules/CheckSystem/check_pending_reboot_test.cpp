// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_pending_reboot.hpp"

#include <gtest/gtest.h>

using pending_reboot_check::reboot_obj;

namespace {
std::string join_lines(const PB::Commands::QueryResponseMessage::Response &r) {
  std::string out;
  for (int i = 0; i < r.lines_size(); ++i) {
    if (!out.empty()) out += "\n";
    out += r.lines(i).message();
  }
  return out;
}

PB::Common::ResultCode run_reboot(const reboot_obj &o, const std::vector<std::string> &args, PB::Commands::QueryResponseMessage::Response &response) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_pending_reboot");
  for (const std::string &a : args) request.add_arguments(a);
  pending_reboot_check::check_pending_reboot_from(request, &response, o);
  return response.result();
}
}  // namespace

TEST(CheckPendingReboot, CleanObjectReportsNoReboot) {
  const reboot_obj o;  // nothing pending
  EXPECT_EQ(o.get_pending(), 0);
  EXPECT_EQ(o.get_count(), 0);
  EXPECT_EQ(o.get_reasons(), "none");
  EXPECT_EQ(o.get_message(), "No reboot pending");
}

TEST(CheckPendingReboot, ReasonsListMultipleCauses) {
  reboot_obj o;
  o.servicing = true;
  o.windows_update = true;
  EXPECT_EQ(o.get_pending(), 1);
  EXPECT_EQ(o.get_count(), 2);
  EXPECT_EQ(o.get_reasons(), "Component Based Servicing, Windows Update");
  EXPECT_EQ(o.get_message(), "Reboot required: Component Based Servicing, Windows Update");
}

TEST(CheckPendingReboot, DefaultOkWhenNothingPending) {
  PB::Commands::QueryResponseMessage::Response response;
  const reboot_obj o;
  EXPECT_EQ(run_reboot(o, {}, response), PB::Common::ResultCode::OK) << join_lines(response);
}

TEST(CheckPendingReboot, DefaultWarnsWhenPending) {
  PB::Commands::QueryResponseMessage::Response response;
  reboot_obj o;
  o.file_rename = true;
  EXPECT_EQ(run_reboot(o, {}, response), PB::Common::ResultCode::WARNING) << join_lines(response);
}

TEST(CheckPendingReboot, PerCauseCriticalExpression) {
  PB::Commands::QueryResponseMessage::Response response;
  reboot_obj o;
  o.servicing = true;
  // Only a servicing-driven reboot should escalate to critical here.
  EXPECT_EQ(run_reboot(o, {"critical=servicing = 1"}, response), PB::Common::ResultCode::CRITICAL) << join_lines(response);
}

TEST(CheckPendingReboot, SignalsKeywordAndDeprecatedCountAlias) {
  reboot_obj o;
  o.servicing = true;
  o.windows_update = true;  // two distinct signals

  PB::Commands::QueryResponseMessage::Response renamed;
  EXPECT_EQ(run_reboot(o, {"warning=none", "critical=signals > 1"}, renamed), PB::Common::ResultCode::CRITICAL) << join_lines(renamed);

  // The old name keeps working as a deprecated alias.
  PB::Commands::QueryResponseMessage::Response alias;
  EXPECT_EQ(run_reboot(o, {"warning=none", "critical=count > 1"}, alias), PB::Common::ResultCode::CRITICAL) << join_lines(alias);
}

TEST(CheckPendingReboot, PerCauseCriticalDoesNotTripOnOtherCause) {
  PB::Commands::QueryResponseMessage::Response response;
  reboot_obj o;
  o.windows_update = true;  // pending, but not servicing
  // critical is scoped to servicing; the default warn (pending=1) still applies.
  EXPECT_EQ(run_reboot(o, {"critical=servicing = 1"}, response), PB::Common::ResultCode::WARNING) << join_lines(response);
}

TEST(CheckPendingReboot, WrittenAndAgeAreUnknownWithoutTimestampedSignal) {
  reboot_obj o;
  o.file_rename = true;  // pending, but PendingFileRenameOperations carries no timestamp
  o.now = 1755900000;
  EXPECT_FALSE(o.get_written());
  EXPECT_FALSE(o.get_age());
  EXPECT_EQ(o.get_written_s(), "unknown");
  // ...and the message does not claim a since-time it does not have.
  EXPECT_EQ(o.get_message().find("pending since"), std::string::npos);
}

TEST(CheckPendingReboot, MessageIncludesPendingSinceWhenKnown) {
  reboot_obj o;
  o.windows_update = true;
  o.pending_since = 1755900000;
  o.now = 1755900600;
  ASSERT_TRUE(o.get_written());
  EXPECT_EQ(*o.get_written(), 1755900000);
  ASSERT_TRUE(o.get_age());
  EXPECT_EQ(*o.get_age(), 600);
  EXPECT_NE(o.get_message().find("(pending since "), std::string::npos) << o.get_message();
}

TEST(CheckPendingReboot, AgeThresholdTakesDurations) {
  reboot_obj o;
  o.servicing = true;
  o.now = 1755900000;

  // Pending for eight days: a week-stale threshold fires...
  o.pending_since = o.now - 8 * 86400;
  PB::Commands::QueryResponseMessage::Response stale;
  EXPECT_EQ(run_reboot(o, {"warning=none", "critical=pending = 1 and age > 7d"}, stale), PB::Common::ResultCode::CRITICAL) << join_lines(stale);

  // ...but a freshly queued reboot does not (7d must mean seven days, not 7 seconds).
  o.pending_since = o.now - 3600;
  PB::Commands::QueryResponseMessage::Response fresh;
  EXPECT_EQ(run_reboot(o, {"warning=none", "critical=pending = 1 and age > 7d"}, fresh), PB::Common::ResultCode::OK) << join_lines(fresh);

  // Plain integers keep meaning seconds.
  o.pending_since = o.now - 600;
  PB::Commands::QueryResponseMessage::Response seconds;
  EXPECT_EQ(run_reboot(o, {"warning=none", "critical=age > 599"}, seconds), PB::Common::ResultCode::CRITICAL) << join_lines(seconds);
}

TEST(CheckPendingReboot, UnknownAgeNeverTripsNumericThresholds) {
  reboot_obj o;
  o.file_rename = true;  // pending, no timestamped signal
  o.now = 1755900000;

  // A numeric comparison on the empty optional is false, so only the staleness
  // clause is silenced - not the whole alert (default warn still fires above).
  PB::Commands::QueryResponseMessage::Response quiet;
  EXPECT_EQ(run_reboot(o, {"warning=none", "critical=age > 7d"}, quiet), PB::Common::ResultCode::OK) << join_lines(quiet);

  // The documented way to test for the missing value is the string compare.
  PB::Commands::QueryResponseMessage::Response probe;
  EXPECT_EQ(run_reboot(o, {"warning=written = 'unknown'", "critical=none"}, probe), PB::Common::ResultCode::WARNING) << join_lines(probe);
}
