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

TEST(CheckPendingReboot, PerCauseCriticalDoesNotTripOnOtherCause) {
  PB::Commands::QueryResponseMessage::Response response;
  reboot_obj o;
  o.windows_update = true;  // pending, but not servicing
  // critical is scoped to servicing; the default warn (pending=1) still applies.
  EXPECT_EQ(run_reboot(o, {"critical=servicing = 1"}, response), PB::Common::ResultCode::WARNING) << join_lines(response);
}
