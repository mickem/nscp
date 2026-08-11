// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_kernel_memory.h"

#include <gtest/gtest.h>

using kernel_memory_check::compute_kernel_memory;
using kernel_memory_check::kernel_memory_obj;
using kernel_memory_check::meminfo_kernel;
using kernel_memory_check::parse_meminfo_kernel;
using kernel_memory_check::parse_vmstat_faults;
using kernel_memory_check::vmstat_faults;

namespace {

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

const std::string MEMINFO =
    "MemTotal:       16384000 kB\n"
    "MemFree:         2048000 kB\n"
    "Cached:          4194304 kB\n"
    "SwapCached:            0 kB\n"
    "Slab:             524288 kB\n"
    "SReclaimable:     393216 kB\n"
    "SUnreclaim:       131072 kB\n";

vmstat_faults faults(const unsigned long long pgfault, const unsigned long long pgmajfault) {
  vmstat_faults f;
  f.pgfault = pgfault;
  f.pgmajfault = pgmajfault;
  f.valid = true;
  return f;
}

kernel_memory_obj healthy() {
  // 25k faults/s of which 2/s major over a 1s window.
  return compute_kernel_memory(parse_meminfo_kernel(MEMINFO), faults(1000000, 5000), faults(1025000, 5002), 1.0);
}

PB::Common::ResultCode run_check(const kernel_memory_obj &data, const std::vector<std::string> &args, PB::Commands::QueryResponseMessage::Response &response) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_kernel_memory");
  for (const std::string &a : args) request.add_arguments(a);
  kernel_memory_check::check_from(request, &response, data);
  return response.result();
}

}  // namespace

// --- parsers -----------------------------------------------------------------

TEST(CheckKernelMemory, ParsesMeminfoInBytes) {
  const meminfo_kernel m = parse_meminfo_kernel(MEMINFO);
  EXPECT_TRUE(m.valid);
  EXPECT_EQ(m.slab, 524288LL * 1024);
  EXPECT_EQ(m.slab_reclaimable, 393216LL * 1024);
  EXPECT_EQ(m.slab_unreclaimable, 131072LL * 1024);
  EXPECT_EQ(m.cache, 4194304LL * 1024);
}

TEST(CheckKernelMemory, MissingMeminfoKeysAreInvalid) {
  EXPECT_FALSE(parse_meminfo_kernel("MemTotal: 1 kB\n").valid);
  EXPECT_FALSE(parse_meminfo_kernel("").valid);
}

TEST(CheckKernelMemory, ParsesVmstatFaults) {
  const vmstat_faults f = parse_vmstat_faults("nr_free_pages 100\npgfault 123456\npgmajfault 789\n");
  EXPECT_TRUE(f.valid);
  EXPECT_EQ(f.pgfault, 123456u);
  EXPECT_EQ(f.pgmajfault, 789u);
  EXPECT_FALSE(parse_vmstat_faults("pgfault 1\n").valid);
}

TEST(CheckKernelMemory, ComputesRatesAndClampsBackwardCounters) {
  const kernel_memory_obj o = healthy();
  EXPECT_DOUBLE_EQ(o.page_faults, 25000.0);
  EXPECT_DOUBLE_EQ(o.major_faults, 2.0);
  // A counter going backwards (e.g. after a wrap) yields 0, not a negative rate.
  const kernel_memory_obj wrapped = compute_kernel_memory(parse_meminfo_kernel(MEMINFO), faults(1000, 100), faults(500, 50), 1.0);
  EXPECT_DOUBLE_EQ(wrapped.page_faults, 0.0);
  EXPECT_DOUBLE_EQ(wrapped.major_faults, 0.0);
}

// --- rendering / thresholds ------------------------------------------------------

TEST(CheckKernelMemory, DefaultIsOkWithHumanSizesAndPerf) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check(healthy(), {}, response), PB::Common::ResultCode::OK) << join_lines(response);
  const std::string msg = join_lines(response);
  EXPECT_NE(msg.find("slab 512MB (128MB unreclaimable), cache 4GB"), std::string::npos) << msg;
  EXPECT_TRUE(has_perf(response, "slab_unreclaimable")) << msg;
  EXPECT_TRUE(has_perf(response, "cache"));
  EXPECT_TRUE(has_perf(response, "major_faults_per_sec"));
}

TEST(CheckKernelMemory, SlabLeakThresholdUsesSizeUnits) {
  kernel_memory_obj leaking = healthy();
  leaking.slab_unreclaimable = 3LL * 1024 * 1024 * 1024;
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check(leaking, {"crit=slab_unreclaimable > 2G"}, response), PB::Common::ResultCode::CRITICAL) << join_lines(response);
  PB::Commands::QueryResponseMessage::Response ok_response;
  EXPECT_EQ(run_check(healthy(), {"crit=slab_unreclaimable > 2G"}, ok_response), PB::Common::ResultCode::OK) << join_lines(ok_response);
}

TEST(CheckKernelMemory, MajorFaultStormTripsWhileSoftFaultsDoNot) {
  const kernel_memory_obj storm = compute_kernel_memory(parse_meminfo_kernel(MEMINFO), faults(1000000, 5000), faults(1025000, 5900), 1.0);
  PB::Commands::QueryResponseMessage::Response response;
  // Soft faults are 25k/s on the healthy host too — only the major-fault rate matters.
  EXPECT_EQ(run_check(storm, {"crit=major_faults_per_sec > 500"}, response), PB::Common::ResultCode::CRITICAL) << join_lines(response);
  PB::Commands::QueryResponseMessage::Response ok_response;
  EXPECT_EQ(run_check(healthy(), {"crit=major_faults_per_sec > 500"}, ok_response), PB::Common::ResultCode::OK) << join_lines(ok_response);
}
