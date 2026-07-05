// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_cpu_utilization.h"

#include <gtest/gtest.h>

using cpu_utilization_check::compute_utilization;
using cpu_utilization_check::cpu_jiffies;
using cpu_utilization_check::parse_proc_stat_cpu;

namespace {
std::string join_lines(const PB::Commands::QueryResponseMessage::Response &r) {
  std::string out;
  for (int i = 0; i < r.lines_size(); ++i) {
    if (!out.empty()) out += "\n";
    out += r.lines(i).message();
  }
  return out;
}

PB::Common::ResultCode run_util(const cpu_jiffies &prev, const cpu_jiffies &cur, const std::vector<std::string> &args,
                                PB::Commands::QueryResponseMessage::Response &response) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_cpu_utilization");
  for (const std::string &a : args) request.add_arguments(a);
  cpu_utilization_check::check_cpu_utilization_from(request, &response, prev, cur);
  return response.result();
}
}  // namespace

TEST(CheckCpuUtilization, ParsesAggregateLine) {
  const cpu_jiffies j = parse_proc_stat_cpu("cpu  100 5 30 800 10 1 2 3 0 0\ncpu0 50 2 15 400 5 0 1 1 0 0\n");
  ASSERT_TRUE(j.valid);
  EXPECT_EQ(j.user, 100u);
  EXPECT_EQ(j.nice, 5u);
  EXPECT_EQ(j.system, 30u);
  EXPECT_EQ(j.idle, 800u);
  EXPECT_EQ(j.iowait, 10u);
  EXPECT_EQ(j.steal, 3u);
}

TEST(CheckCpuUtilization, MissingLineIsInvalid) {
  EXPECT_FALSE(parse_proc_stat_cpu("intr 12345\nctxt 999\n").valid);
}

TEST(CheckCpuUtilization, ComputesBreakdownOverDelta) {
  // Δ over the interval: user+nice=10, system=10, idle=70, iowait=10 -> total delta 100.
  cpu_jiffies a;
  a.user = 0;
  a.nice = 0;
  a.system = 0;
  a.idle = 0;
  a.iowait = 0;
  a.valid = true;
  cpu_jiffies b = a;
  b.user = 8;
  b.nice = 2;
  b.system = 10;
  b.idle = 70;
  b.iowait = 10;

  const auto u = compute_utilization(a, b);
  EXPECT_DOUBLE_EQ(u.user, 10.0);    // (8+2)/100
  EXPECT_DOUBLE_EQ(u.system, 10.0);  // 10/100
  EXPECT_DOUBLE_EQ(u.iowait, 10.0);
  EXPECT_DOUBLE_EQ(u.idle, 70.0);
  EXPECT_DOUBLE_EQ(u.total, 20.0);  // 100 - idle(70) - iowait(10)
}

TEST(CheckCpuUtilization, ZeroDeltaYieldsZero) {
  cpu_jiffies a;
  a.user = 5;
  a.idle = 5;
  a.valid = true;
  const auto u = compute_utilization(a, a);
  EXPECT_DOUBLE_EQ(u.total, 0.0);
  EXPECT_DOUBLE_EQ(u.idle, 0.0);
}

TEST(CheckCpuUtilization, CounterResetIsClamped) {
  cpu_jiffies a;
  a.user = 100;
  a.idle = 100;
  a.valid = true;
  cpu_jiffies b;  // all zero -> looks like a reset
  b.valid = true;
  const auto u = compute_utilization(a, b);
  EXPECT_DOUBLE_EQ(u.total, 0.0);  // no forward progress -> zeroed, not negative
}

TEST(CheckCpuUtilization, DefaultThresholdTripsAboveNinety) {
  // 95% busy (idle 5) -> above the default warn total>90.
  cpu_jiffies a;
  a.valid = true;
  cpu_jiffies b;
  b.user = 95;
  b.idle = 5;
  b.valid = true;
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_util(a, b, {}, response), PB::Common::ResultCode::WARNING) << join_lines(response);
}

TEST(CheckCpuUtilization, StealIsVisible) {
  cpu_jiffies a;
  a.valid = true;
  cpu_jiffies b;
  b.steal = 40;
  b.idle = 60;
  b.valid = true;
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_util(a, b, {"critical=steal > 30", "detail-syntax=steal=${steal}"}, response), PB::Common::ResultCode::CRITICAL) << join_lines(response);
  EXPECT_NE(join_lines(response).find("steal=40"), std::string::npos) << join_lines(response);
}
