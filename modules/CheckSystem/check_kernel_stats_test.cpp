// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_kernel_stats.hpp"

#include <gtest/gtest.h>

using kernel_stats_check::build_rows;
using kernel_stats_check::kstat_row;
using kernel_stats_check::rows_type;

namespace {

std::string join_lines(const PB::Commands::QueryResponseMessage::Response &r) {
  std::string out;
  for (int i = 0; i < r.lines_size(); ++i) {
    if (!out.empty()) out += "\n";
    out += r.lines(i).message();
  }
  return out;
}

PB::Common::ResultCode run_check(const double ctxt, const double syscalls, const long long processes, const long long threads,
                                 const std::vector<std::string> &args, PB::Commands::QueryResponseMessage::Response &response) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_kernel_stats");
  for (const std::string &a : args) request.add_arguments(a);
  kernel_stats_check::check_kernel_stats_from(request, &response, ctxt, syscalls, processes, threads);
  return response.result();
}

}  // namespace

TEST(CheckKernelStats, BuildsAllFourRowsByDefault) {
  const rows_type rows = build_rows(12345.6, 98765.4, 250, 3200, {});
  ASSERT_EQ(rows.size(), 4u);
  auto it = rows.begin();
  EXPECT_EQ(it->name, "ctxt");
  EXPECT_EQ(it->label, "Context Switches");
  EXPECT_DOUBLE_EQ(it->rate, 12345.6);
  EXPECT_EQ(it->current, 12346);  // rounded rate: no cumulative counter on Windows
  EXPECT_EQ(it->human, "12345.6/s");
  ++it;
  EXPECT_EQ(it->name, "syscalls");
  ++it;
  EXPECT_EQ(it->name, "processes");
  EXPECT_DOUBLE_EQ(it->rate, 0.0);
  EXPECT_EQ(it->current, 250);
  EXPECT_EQ(it->human, "250");
  ++it;
  EXPECT_EQ(it->name, "threads");
  EXPECT_EQ(it->current, 3200);
}

TEST(CheckKernelStats, TypeSelectionFiltersRows) {
  const rows_type rows = build_rows(1.0, 2.0, 3, 4, {"threads"});
  ASSERT_EQ(rows.size(), 1u);
  EXPECT_EQ(rows.begin()->name, "threads");
}

TEST(CheckKernelStats, NegativeFirstSampleRatesAreClamped) {
  const rows_type rows = build_rows(-5.0, -5.0, -1, -1, {});
  for (const kstat_row &r : rows) {
    EXPECT_GE(r.rate, 0.0) << r.name;
    EXPECT_GE(r.current, 0) << r.name;
  }
}

TEST(CheckKernelStats, DefaultThreadGuardrailsMatchUnix) {
  PB::Commands::QueryResponseMessage::Response ok_response;
  EXPECT_EQ(run_check(10000.0, 50000.0, 250, 3200, {}, ok_response), PB::Common::ResultCode::OK) << join_lines(ok_response);

  PB::Commands::QueryResponseMessage::Response warn_response;
  EXPECT_EQ(run_check(10000.0, 50000.0, 250, 9000, {}, warn_response), PB::Common::ResultCode::WARNING) << join_lines(warn_response);

  PB::Commands::QueryResponseMessage::Response crit_response;
  EXPECT_EQ(run_check(10000.0, 50000.0, 250, 11000, {}, crit_response), PB::Common::ResultCode::CRITICAL) << join_lines(crit_response);
}

TEST(CheckKernelStats, ContextSwitchStormIsThresholdable) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check(250000.0, 50000.0, 250, 3200, {"warn=none", "crit=name = 'ctxt' and rate > 100000"}, response), PB::Common::ResultCode::CRITICAL)
      << join_lines(response);
  EXPECT_NE(join_lines(response).find("Context Switches 250000.0/s"), std::string::npos) << join_lines(response);
}

TEST(CheckKernelStats, TypeOptionRendersSingleRow) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check(1000.0, 2000.0, 250, 3200, {"type=threads", "detail-syntax=${name}=${current}"}, response), PB::Common::ResultCode::OK)
      << join_lines(response);
  const std::string msg = join_lines(response);
  EXPECT_NE(msg.find("threads=3200"), std::string::npos) << msg;
  EXPECT_EQ(msg.find("ctxt"), std::string::npos) << msg;
}
