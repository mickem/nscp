// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_kernel_memory.hpp"

#include <gtest/gtest.h>

using kernel_memory_check::kernel_memory_obj;
using kernel_memory_check::make_kernel_memory_obj;

namespace {

const long long GB = 1024LL * 1024 * 1024;
const long long MB = 1024LL * 1024;

std::string join_lines(const PB::Commands::QueryResponseMessage::Response &r) {
  std::string out;
  for (int i = 0; i < r.lines_size(); ++i) {
    if (!out.empty()) out += "\n";
    out += r.lines(i).message();
  }
  return out;
}

bool has_perf(const PB::Commands::QueryResponseMessage::Response &r, const std::string &alias_part) {
  for (int i = 0; i < r.lines_size(); ++i) {
    for (int j = 0; j < r.lines(i).perf_size(); ++j) {
      if (r.lines(i).perf(j).alias().find(alias_part) != std::string::npos) return true;
    }
  }
  return false;
}

kernel_memory_obj healthy() { return make_kernel_memory_obj(512 * MB, 256 * MB, 4 * GB, 25000.0, 20000.0, 2.5); }

PB::Common::ResultCode run_check(const kernel_memory_obj &data, const std::vector<std::string> &args, PB::Commands::QueryResponseMessage::Response &response) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_kernel_memory");
  for (const std::string &a : args) request.add_arguments(a);
  kernel_memory_check::check_from(request, &response, data);
  return response.result();
}

}  // namespace

TEST(CheckKernelMemory, NegativeFirstSampleRatesAreClamped) {
  const kernel_memory_obj o = make_kernel_memory_obj(-1, -1, -1, -1.0, -1.0, -1.0);
  EXPECT_EQ(o.pool_paged, 0);
  EXPECT_EQ(o.pool_nonpaged, 0);
  EXPECT_EQ(o.cache, 0);
  EXPECT_EQ(o.page_faults, 0.0);
  EXPECT_EQ(o.hard_faults, 0.0);
}

TEST(CheckKernelMemory, DefaultIsOkWithHumanSizesAndPerf) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check(healthy(), {}, response), PB::Common::ResultCode::OK) << join_lines(response);
  const std::string msg = join_lines(response);
  EXPECT_NE(msg.find("paged pool 512MB, nonpaged pool 256MB, cache 4GB"), std::string::npos) << msg;
  EXPECT_TRUE(has_perf(response, "pool_paged")) << msg;
  EXPECT_TRUE(has_perf(response, "pool_nonpaged"));
  EXPECT_TRUE(has_perf(response, "cache"));
  EXPECT_TRUE(has_perf(response, "page_faults_per_sec"));
  EXPECT_TRUE(has_perf(response, "transition_faults_per_sec"));
  EXPECT_TRUE(has_perf(response, "hard_faults_per_sec"));
}

TEST(CheckKernelMemory, PoolThresholdsUseSizeUnits) {
  kernel_memory_obj leaking = healthy();
  leaking.pool_nonpaged = 5 * GB;
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check(leaking, {"warn=pool_nonpaged > 1G", "crit=pool_nonpaged > 4G"}, response), PB::Common::ResultCode::CRITICAL) << join_lines(response);
  PB::Commands::QueryResponseMessage::Response ok_response;
  EXPECT_EQ(run_check(healthy(), {"warn=pool_nonpaged > 1G", "crit=pool_nonpaged > 4G"}, ok_response), PB::Common::ResultCode::OK) << join_lines(ok_response);
}

TEST(CheckKernelMemory, HardFaultStormTripsWhileSoftFaultsDoNot) {
  kernel_memory_obj storm = healthy();
  storm.hard_faults = 850.0;
  PB::Commands::QueryResponseMessage::Response response;
  // Soft faults are 25k/s on the healthy host too — only the hard-fault rate matters.
  EXPECT_EQ(run_check(storm, {"crit=hard_faults_per_sec > 500"}, response), PB::Common::ResultCode::CRITICAL) << join_lines(response);
  PB::Commands::QueryResponseMessage::Response ok_response;
  EXPECT_EQ(run_check(healthy(), {"crit=hard_faults_per_sec > 500"}, ok_response), PB::Common::ResultCode::OK) << join_lines(ok_response);
}

TEST(CheckKernelMemory, CacheAndTransitionKeywordsAreThresholdable) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check(healthy(), {"warn=cache > 1G and transition_faults_per_sec > 100"}, response), PB::Common::ResultCode::WARNING) << join_lines(response);
}
