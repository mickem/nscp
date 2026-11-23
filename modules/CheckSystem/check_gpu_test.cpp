// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_gpu.hpp"

#include <gtest/gtest.h>

using gpu_check::adapter_info;
using gpu_check::build_adapters;
using gpu_check::counter_snapshot;
using gpu_check::engine_class;
using gpu_check::gpu_adapter;
using gpu_check::instance_id;
using gpu_check::parse_instance;

namespace {

const std::string kAdapter = "luid_0x00000000_0x0000C5F5_phys_0";
const std::string kLuid = "0x00000000_0x0000C5F5";
const long long kGiB = 1024ll * 1024ll * 1024ll;

std::string join_lines(const PB::Commands::QueryResponseMessage::Response &r) {
  std::string out;
  for (int i = 0; i < r.lines_size(); ++i) {
    if (!out.empty()) out += "\n";
    out += r.lines(i).message();
  }
  return out;
}

PB::Common::ResultCode run_check(const std::vector<gpu_adapter> &adapters, const std::vector<std::string> &args,
                                 PB::Commands::QueryResponseMessage::Response &response) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_gpu");
  for (const std::string &a : args) request.add_arguments(a);
  gpu_check::check_gpu_from(request, &response, adapters);
  return response.result();
}

adapter_info rtx() {
  adapter_info info;
  info.luid = kLuid;
  info.name = "NVIDIA RTX A4000";
  info.dedicated_total = 16 * kGiB;
  info.shared_total = 32 * kGiB;
  return info;
}

// An adapter with `dedicated_gib` of its 16 GiB in use and one busy 3D engine.
std::vector<gpu_adapter> one_adapter(const long long dedicated_gib, const double load) {
  counter_snapshot s;
  s.adapter_memory[kAdapter] = {{"Dedicated Usage", static_cast<double>(dedicated_gib * kGiB)}};
  s.engine["pid_100_" + kAdapter + "_eng_0_engtype_3D"] = {{"Utilization Percentage", load}};
  return build_adapters(s, {rtx()});
}

}  // namespace

TEST(CheckGpu, ParsesEngineInstance) {
  instance_id id;
  ASSERT_TRUE(parse_instance("pid_1234_luid_0x00000000_0x0000C5F5_phys_0_eng_3_engtype_Compute_0", id));
  EXPECT_EQ(id.pid, 1234);
  EXPECT_EQ(id.luid, kLuid);
  EXPECT_EQ(id.phys, 0);
  EXPECT_EQ(id.engine, 3);
  EXPECT_EQ(id.engine_type, "Compute_0");
  EXPECT_EQ(id.part, -1);
}

TEST(CheckGpu, ParsesAdapterAndPartitionInstances) {
  instance_id id;
  // Lower-case digits normalise to the form DXGI luids are rendered in.
  ASSERT_TRUE(parse_instance("luid_0x00000000_0x0000c5f5_phys_1_part_2", id));
  EXPECT_EQ(id.pid, -1);
  EXPECT_EQ(id.luid, kLuid);
  EXPECT_EQ(id.phys, 1);
  EXPECT_EQ(id.part, 2);
  EXPECT_EQ(id.engine, -1);
  EXPECT_TRUE(id.engine_type.empty());
}

TEST(CheckGpu, EngineTypeKeepsSpacesAndUnderscores) {
  instance_id id;
  ASSERT_TRUE(parse_instance("pid_4_" + kAdapter + "_eng_7_engtype_High Priority 3D", id));
  EXPECT_EQ(id.engine, 7);
  EXPECT_EQ(id.engine_type, "High Priority 3D");
}

TEST(CheckGpu, RejectsInstancesWithoutLuid) {
  instance_id id;
  EXPECT_FALSE(parse_instance("_Total", id));
  EXPECT_FALSE(parse_instance("", id));
  EXPECT_FALSE(parse_instance("luid_", id));
  EXPECT_FALSE(parse_instance("luid_0x1", id));
  EXPECT_FALSE(parse_instance("luid_zz_0x1_phys_0", id));
}

TEST(CheckGpu, EngineClassFoldsNumbersAndCuda) {
  EXPECT_EQ(engine_class("3D"), "3d");
  EXPECT_EQ(engine_class("Compute_0"), "compute");
  EXPECT_EQ(engine_class("Compute_12"), "compute");
  EXPECT_EQ(engine_class("Cuda"), "compute");
  EXPECT_EQ(engine_class("VideoDecode"), "videodecode");
  EXPECT_EQ(engine_class("Video Encode"), "videoencode");
  EXPECT_EQ(engine_class(""), "");
}

TEST(CheckGpu, UtilizationIsBusiestEngineSummedOverProcesses) {
  counter_snapshot s;
  s.adapter_memory[kAdapter] = {{"Dedicated Usage", 6.0 * kGiB}, {"Shared Usage", 1.0 * kGiB}, {"Total Committed", 7.5 * kGiB}};
  s.engine["pid_1_" + kAdapter + "_eng_0_engtype_3D"] = {{"Utilization Percentage", 30.2}};
  s.engine["pid_2_" + kAdapter + "_eng_0_engtype_3D"] = {{"Utilization Percentage", 12.4}};
  s.engine["pid_2_" + kAdapter + "_eng_5_engtype_Compute_0"] = {{"Utilization Percentage", 61.0}};
  s.engine["pid_3_" + kAdapter + "_eng_2_engtype_VideoDecode"] = {{"Utilization Percentage", 5.0}};

  const std::vector<gpu_adapter> adapters = build_adapters(s, {rtx()});
  ASSERT_EQ(adapters.size(), 1u);
  const gpu_adapter &g = adapters[0];
  EXPECT_EQ(g.name, "NVIDIA RTX A4000");
  EXPECT_EQ(g.luid, kLuid);
  EXPECT_EQ(g.adapter_type, "hardware");
  EXPECT_EQ(g.utilization, 61);
  EXPECT_EQ(g.busiest_engine, "Compute_0");
  EXPECT_EQ(g.utilization_3d, 43);  // 30.2 + 12.4: two processes on one engine
  EXPECT_EQ(g.utilization_compute, 61);
  EXPECT_EQ(g.utilization_video_decode, 5);
  EXPECT_EQ(g.utilization_video_encode, 0);
  EXPECT_EQ(g.dedicated_used, 6 * kGiB);
  EXPECT_EQ(g.shared_used, 1 * kGiB);
  EXPECT_EQ(g.get_dedicated_pct(), 38);
  EXPECT_EQ(g.get_shared_pct(), 3);
}

TEST(CheckGpu, UtilizationIsCappedAndNegativeSamplesIgnored) {
  counter_snapshot s;
  s.adapter_memory[kAdapter] = {};
  s.engine["pid_1_" + kAdapter + "_eng_0_engtype_3D"] = {{"Utilization Percentage", 80.0}};
  s.engine["pid_2_" + kAdapter + "_eng_0_engtype_3D"] = {{"Utilization Percentage", 45.0}};
  s.engine["pid_3_" + kAdapter + "_eng_1_engtype_Copy"] = {{"Utilization Percentage", -3.0}};
  const std::vector<gpu_adapter> adapters = build_adapters(s, {});
  ASSERT_EQ(adapters.size(), 1u);
  EXPECT_EQ(adapters[0].utilization, 100);
  EXPECT_EQ(adapters[0].utilization_copy, 0);
}

TEST(CheckGpu, UnidentifiedAdapterIsReportedUnderItsLuid) {
  counter_snapshot s;
  s.adapter_memory[kAdapter] = {{"Dedicated Usage", 1.0 * kGiB}};
  const std::vector<gpu_adapter> adapters = build_adapters(s, {});
  ASSERT_EQ(adapters.size(), 1u);
  EXPECT_EQ(adapters[0].name, kLuid);
  EXPECT_EQ(adapters[0].adapter_type, "unknown");
  EXPECT_EQ(adapters[0].dedicated_total, 0);
  EXPECT_EQ(adapters[0].get_dedicated_pct(), 0);  // no capacity, no percentage
  EXPECT_TRUE(adapters[0].busiest_engine.empty());
}

TEST(CheckGpu, LocalUsageSumsPartitionsAndProcessesCountMemoryHolders) {
  counter_snapshot s;
  s.adapter_memory[kAdapter] = {};
  s.local_memory[kAdapter + "_part_0"] = {{"Local Usage", 2.0 * kGiB}};
  s.local_memory[kAdapter + "_part_1"] = {{"Local Usage", 1.0 * kGiB}};
  s.process_memory["pid_1_" + kAdapter] = {{"Dedicated Usage", 1024.0}};
  s.process_memory["pid_2_" + kAdapter] = {{"Shared Usage", 1024.0}};
  s.process_memory["pid_3_" + kAdapter] = {{"Dedicated Usage", 0.0}, {"Shared Usage", 0.0}};
  const std::vector<gpu_adapter> adapters = build_adapters(s, {});
  ASSERT_EQ(adapters.size(), 1u);
  EXPECT_EQ(adapters[0].local_used, 3 * kGiB);
  EXPECT_EQ(adapters[0].processes, 2);
}

TEST(CheckGpu, LinkedAdaptersGetTheirPhysIndexInTheName) {
  counter_snapshot s;
  s.adapter_memory["luid_0x00000000_0x0000C5F5_phys_0"] = {};
  s.adapter_memory["luid_0x00000000_0x0000C5F5_phys_1"] = {};
  const std::vector<gpu_adapter> adapters = build_adapters(s, {rtx()});
  ASSERT_EQ(adapters.size(), 2u);
  EXPECT_EQ(adapters[0].name, "NVIDIA RTX A4000 #0");
  EXPECT_EQ(adapters[1].name, "NVIDIA RTX A4000 #1");
  EXPECT_EQ(adapters[1].phys, 1);
}

TEST(CheckGpu, DefaultThresholdsWatchDedicatedMemory) {
  PB::Commands::QueryResponseMessage::Response ok_response;
  // A pinned GPU is not a problem by default; a full one is.
  EXPECT_EQ(run_check(one_adapter(4, 100.0), {}, ok_response), PB::Common::ResultCode::OK) << join_lines(ok_response);
  EXPECT_NE(join_lines(ok_response).find("NVIDIA RTX A4000: 100% busy"), std::string::npos) << join_lines(ok_response);
  EXPECT_NE(join_lines(ok_response).find("(25%)"), std::string::npos) << join_lines(ok_response);

  PB::Commands::QueryResponseMessage::Response warn_response;
  EXPECT_EQ(run_check(one_adapter(14, 10.0), {}, warn_response), PB::Common::ResultCode::WARNING) << join_lines(warn_response);

  PB::Commands::QueryResponseMessage::Response crit_response;
  EXPECT_EQ(run_check(one_adapter(16, 10.0), {}, crit_response), PB::Common::ResultCode::CRITICAL) << join_lines(crit_response);
}

TEST(CheckGpu, UtilizationIsThresholdable) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check(one_adapter(1, 97.0), {"warn=utilization > 80", "crit=utilization > 95"}, response), PB::Common::ResultCode::CRITICAL)
      << join_lines(response);
}

TEST(CheckGpu, SizeUnitsWorkOnMemoryKeywords) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check(one_adapter(8, 0.0), {"warn=dedicated_used > 6G", "crit=dedicated_used > 12G"}, response), PB::Common::ResultCode::WARNING)
      << join_lines(response);
}

TEST(CheckGpu, SoftwareAdapterIsFilteredOutByDefault) {
  counter_snapshot s;
  s.adapter_memory[kAdapter] = {};
  adapter_info basic;
  basic.luid = kLuid;
  basic.name = "Microsoft Basic Render Driver";
  basic.software = true;
  const std::vector<gpu_adapter> adapters = build_adapters(s, {basic});

  // Only the software rasteriser: the host has no GPU.
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check(adapters, {}, response), PB::Common::ResultCode::UNKNOWN) << join_lines(response);
  EXPECT_NE(join_lines(response).find("No GPU adapters found"), std::string::npos) << join_lines(response);

  PB::Commands::QueryResponseMessage::Response ok_response;
  EXPECT_EQ(run_check(adapters, {"empty-state=ok"}, ok_response), PB::Common::ResultCode::OK) << join_lines(ok_response);

  PB::Commands::QueryResponseMessage::Response all_response;
  EXPECT_EQ(run_check(adapters, {"filter=none"}, all_response), PB::Common::ResultCode::OK) << join_lines(all_response);
  EXPECT_NE(join_lines(all_response).find("Microsoft Basic Render Driver"), std::string::npos) << join_lines(all_response);
}

TEST(CheckGpu, NoAdaptersIsUnknown) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check({}, {}, response), PB::Common::ResultCode::UNKNOWN) << join_lines(response);
  EXPECT_NE(join_lines(response).find("No GPU adapters found"), std::string::npos) << join_lines(response);
}
