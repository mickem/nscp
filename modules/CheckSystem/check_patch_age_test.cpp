// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_patch_age.hpp"

#include <gtest/gtest.h>

using patch_age_check::build_patch_obj;
using patch_age_check::hotfix_entry;
using patch_age_check::parse_installed_on;
using patch_age_check::patch_obj;

namespace {
std::string join_lines(const PB::Commands::QueryResponseMessage::Response &r) {
  std::string out;
  for (int i = 0; i < r.lines_size(); ++i) {
    if (!out.empty()) out += "\n";
    out += r.lines(i).message();
  }
  return out;
}

hotfix_entry make_hotfix(const std::string &id, const std::string &installed) {
  hotfix_entry e;
  e.id = id;
  e.installed_str = installed;
  e.installed_epoch = parse_installed_on(installed);
  return e;
}

std::vector<hotfix_entry> sample_hotfixes() {
  return {make_hotfix("KB111", "1/1/2024"), make_hotfix("KB222", "2/20/2024")};
}

PB::Common::ResultCode run_patch(const std::vector<hotfix_entry> &entries, long long now_epoch, const std::vector<std::string> &args,
                                 PB::Commands::QueryResponseMessage::Response &response) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_patch_age");
  for (const std::string &a : args) request.add_arguments(a);
  patch_age_check::check_patch_age_from(request, &response, entries, now_epoch);
  return response.result();
}
}  // namespace

TEST(CheckPatchAge, ParsesUsSlashDate) {
  EXPECT_GT(parse_installed_on("3/15/2023"), 0);
  // The compact 8-digit form maps to the same day.
  EXPECT_EQ(parse_installed_on("20230315"), parse_installed_on("3/15/2023"));
}

TEST(CheckPatchAge, UnparseableDatesYieldZero) {
  EXPECT_EQ(parse_installed_on(""), 0);
  EXPECT_EQ(parse_installed_on("N/A"), 0);
  EXPECT_EQ(parse_installed_on("garbage"), 0);
  EXPECT_EQ(parse_installed_on("13/40/2020"), 0);  // impossible month/day
}

TEST(CheckPatchAge, AggregatesNewestAndAge) {
  const long long now = parse_installed_on("3/1/2024");  // 10 days after 2/20/2024 (leap year)
  const patch_obj o = build_patch_obj(sample_hotfixes(), {}, now);
  EXPECT_EQ(o.count, 2);
  EXPECT_EQ(o.newest_id, "KB222");
  EXPECT_EQ(o.newest_installed, "2/20/2024");
  EXPECT_EQ(o.age_days, 10);
  EXPECT_EQ(o.ids, "KB111;KB222");
  EXPECT_EQ(o.missing, 0);
}

TEST(CheckPatchAge, MissingRequiredHotfix) {
  const long long now = parse_installed_on("3/1/2024");
  // KB999 is absent; a bare number is matched with an implicit KB prefix.
  const patch_obj o = build_patch_obj(sample_hotfixes(), {"KB999", "111"}, now);
  EXPECT_EQ(o.required, 2);
  EXPECT_EQ(o.missing, 1);
  EXPECT_EQ(o.missing_ids, "KB999");
}

TEST(CheckPatchAge, UnknownAgeWhenNoDatesParse) {
  std::vector<hotfix_entry> entries = {make_hotfix("KB111", ""), make_hotfix("KB222", "N/A")};
  const patch_obj o = build_patch_obj(entries, {}, parse_installed_on("3/1/2024"));
  EXPECT_EQ(o.count, 2);
  EXPECT_EQ(o.age_days, -1);
}

TEST(CheckPatchAge, DefaultOkWithoutThresholds) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_patch(sample_hotfixes(), parse_installed_on("3/1/2024"), {}, response), PB::Common::ResultCode::OK) << join_lines(response);
}

TEST(CheckPatchAge, MissingHotfixIsCritical) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_patch(sample_hotfixes(), parse_installed_on("3/1/2024"), {"hotfix=KB999"}, response), PB::Common::ResultCode::CRITICAL)
      << join_lines(response);
}

TEST(CheckPatchAge, PresentHotfixIsOk) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_patch(sample_hotfixes(), parse_installed_on("3/1/2024"), {"hotfix=KB111"}, response), PB::Common::ResultCode::OK) << join_lines(response);
}

TEST(CheckPatchAge, AgeWarningTrips) {
  PB::Commands::QueryResponseMessage::Response response;
  // Newest hotfix is 10 days old; warn above 5 days.
  EXPECT_EQ(run_patch(sample_hotfixes(), parse_installed_on("3/1/2024"), {"warning=age > 5"}, response), PB::Common::ResultCode::WARNING)
      << join_lines(response);
}
