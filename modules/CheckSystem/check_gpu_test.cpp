// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_gpu.hpp"

#include <gtest/gtest.h>

#include <set>

using gpu_check::adapter_enumeration;
using gpu_check::adapter_info;
using gpu_check::build_adapters;
using gpu_check::classify_engine;
using gpu_check::counter_snapshot;
using gpu_check::driver_key;
using gpu_check::driver_memory_size;
using gpu_check::engine_class;
using gpu_check::engine_kinds;
using gpu_check::gpu_adapter;
using gpu_check::instance_id;
using gpu_check::memory_total;
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

// --- engine classification ---------------------------------------------------

namespace {
std::string kinds_of(const std::string &engine_type) {
  const engine_kinds k = classify_engine(engine_type);
  std::string out;
  if (k.is_3d) out += "3d ";
  if (k.copy) out += "copy ";
  if (k.video_decode) out += "decode ";
  if (k.video_encode) out += "encode ";
  if (k.compute) out += "compute ";
  return out;
}
}  // namespace

TEST(CheckGpu, ExactEngineNamesClassifyAsBefore) {
  EXPECT_EQ(kinds_of("3D"), "3d ");
  EXPECT_EQ(kinds_of("Copy"), "copy ");
  EXPECT_EQ(kinds_of("VideoDecode"), "decode ");
  EXPECT_EQ(kinds_of("Video Decode"), "decode ");
  EXPECT_EQ(kinds_of("VideoEncode"), "encode ");
  EXPECT_EQ(kinds_of("Compute_0"), "compute ");
  EXPECT_EQ(kinds_of("Compute_12"), "compute ");
  EXPECT_EQ(kinds_of("Cuda"), "compute ");
  EXPECT_EQ(kinds_of(""), "");
}

TEST(CheckGpu, EngineVariantsClassifyBySubstring) {
  EXPECT_EQ(kinds_of("High Priority 3D"), "3d ");
  EXPECT_EQ(kinds_of("High Priority Compute"), "compute ");
  EXPECT_EQ(kinds_of("High Priority Copy"), "copy ");
  EXPECT_EQ(kinds_of("Video Codec 0"), "decode encode ");
  EXPECT_EQ(kinds_of("VideoCodec_1"), "decode encode ");
  EXPECT_EQ(kinds_of("VideoDecode_1"), "decode ");
  EXPECT_EQ(kinds_of("VideoEncode 2"), "encode ");
  // Scaling and colour conversion: neither decode nor encode.
  EXPECT_EQ(kinds_of("VideoProcessing"), "");
  EXPECT_EQ(kinds_of("Security"), "");
  EXPECT_EQ(kinds_of("LegacyOverlay"), "");
}

TEST(CheckGpu, PerTypeLoadCountsEngineVariants) {
  counter_snapshot s;
  s.adapter_memory[kAdapter] = {};
  s.engine["pid_1_" + kAdapter + "_eng_0_engtype_High Priority 3D"] = {{"Utilization Percentage", 20.0}};
  s.engine["pid_1_" + kAdapter + "_eng_1_engtype_High Priority Compute"] = {{"Utilization Percentage", 30.0}};
  s.engine["pid_1_" + kAdapter + "_eng_2_engtype_Video Codec 0"] = {{"Utilization Percentage", 70.0}};
  s.engine["pid_1_" + kAdapter + "_eng_3_engtype_VideoProcessing"] = {{"Utilization Percentage", 90.0}};
  const std::vector<gpu_adapter> adapters = build_adapters(s, {rtx()});
  ASSERT_EQ(adapters.size(), 1u);
  const gpu_adapter &g = adapters[0];
  EXPECT_EQ(g.utilization, 90);
  EXPECT_EQ(g.busiest_engine, "VideoProcessing");
  EXPECT_EQ(g.utilization_3d, 20);
  EXPECT_EQ(g.utilization_compute, 30);
  EXPECT_EQ(g.utilization_video_decode, 70);
  EXPECT_EQ(g.utilization_video_encode, 70);
  EXPECT_EQ(g.utilization_copy, 0);
}

// --- unique names ------------------------------------------------------------

namespace {
adapter_info card(const std::string &luid, const std::string &name) {
  adapter_info info;
  info.luid = luid;
  info.name = name;
  info.dedicated_total = 16 * kGiB;
  return info;
}
}  // namespace

TEST(CheckGpu, SingleCardKeepsItsName) {
  counter_snapshot s;
  s.adapter_memory[kAdapter] = {};
  adapter_info basic = card("0x00000000_0x00010000", "Microsoft Basic Render Driver");
  basic.software = true;
  s.adapter_memory["luid_0x00000000_0x00010000_phys_0"] = {};
  const std::vector<gpu_adapter> adapters = build_adapters(s, {rtx(), basic});
  ASSERT_EQ(adapters.size(), 2u);
  EXPECT_EQ(adapters[0].name, "NVIDIA RTX A4000");
  EXPECT_EQ(adapters[1].name, "Microsoft Basic Render Driver");
}

TEST(CheckGpu, IdenticalCardsAreNumberedInDxgiOrder) {
  counter_snapshot s;
  // The luids sort the other way round from the DXGI order: the number has to
  // follow DXGI (slot order, stable across reboots), not the luid.
  s.adapter_memory["luid_0x00000000_0x0000A000_phys_0"] = {};
  s.adapter_memory["luid_0x00000000_0x0000B000_phys_0"] = {};
  s.adapter_memory["luid_0x00000000_0x0000C000_phys_0"] = {};
  const std::vector<adapter_info> dxgi = {card("0x00000000_0x0000B000", "NVIDIA A16"), card("0x00000000_0x0000C000", "Intel UHD Graphics"),
                                          card("0x00000000_0x0000A000", "NVIDIA A16")};
  const std::vector<gpu_adapter> adapters = build_adapters(s, dxgi);
  ASSERT_EQ(adapters.size(), 3u);
  // Records stay ordered by luid.
  EXPECT_EQ(adapters[0].luid, "0x00000000_0x0000A000");
  EXPECT_EQ(adapters[0].name, "NVIDIA A16 #1");
  EXPECT_EQ(adapters[1].name, "NVIDIA A16 #0");
  EXPECT_EQ(adapters[2].name, "Intel UHD Graphics");
}

TEST(CheckGpu, IdenticalCardsComposeWithLinkedAdapters) {
  counter_snapshot s;
  // One linked pair (luid B, phys 0 and 1) and a separate card of the same model.
  s.adapter_memory["luid_0x00000000_0x0000A000_phys_0"] = {};
  s.adapter_memory["luid_0x00000000_0x0000B000_phys_0"] = {};
  s.adapter_memory["luid_0x00000000_0x0000B000_phys_1"] = {};
  const std::vector<adapter_info> dxgi = {card("0x00000000_0x0000B000", "NVIDIA A16"), card("0x00000000_0x0000A000", "NVIDIA A16")};
  const std::vector<gpu_adapter> adapters = build_adapters(s, dxgi);
  ASSERT_EQ(adapters.size(), 3u);
  EXPECT_EQ(adapters[0].name, "NVIDIA A16 #2");  // luid A, listed second by DXGI
  EXPECT_EQ(adapters[1].name, "NVIDIA A16 #0");  // luid B, phys 0
  EXPECT_EQ(adapters[2].name, "NVIDIA A16 #1");  // luid B, phys 1
  EXPECT_EQ(adapters[2].phys, 1);
}

TEST(CheckGpu, IdenticalCardsGetDistinctPerfLabels) {
  counter_snapshot s;
  s.adapter_memory["luid_0x00000000_0x0000A000_phys_0"] = {{"Dedicated Usage", 1.0 * kGiB}};
  s.adapter_memory["luid_0x00000000_0x0000B000_phys_0"] = {{"Dedicated Usage", 2.0 * kGiB}};
  const std::vector<gpu_adapter> adapters = build_adapters(s, {card("0x00000000_0x0000A000", "NVIDIA A16"), card("0x00000000_0x0000B000", "NVIDIA A16")});

  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check(adapters, {}, response), PB::Common::ResultCode::OK) << join_lines(response);
  ASSERT_EQ(response.lines_size(), 1);
  std::set<std::string> aliases;
  for (const auto &perf : response.lines(0).perf()) aliases.insert(perf.alias());
  EXPECT_EQ(aliases.count("NVIDIA A16 #0_utilization"), 1u) << join_lines(response);
  EXPECT_EQ(aliases.count("NVIDIA A16 #1_utilization"), 1u) << join_lines(response);

  PB::Commands::QueryResponseMessage::Response one;
  EXPECT_EQ(run_check(adapters, {"filter=name = 'NVIDIA A16 #1'", "top-syntax=${list}"}, one), PB::Common::ResultCode::OK) << join_lines(one);
  EXPECT_EQ(join_lines(one), "NVIDIA A16 #1: 0% busy, 2GB dedicated memory used (13%)");
}

// --- memory totals -----------------------------------------------------------

TEST(CheckGpu, SixtyFourBitTotalsAreTakenAsReported) {
  EXPECT_EQ(memory_total(16ull * kGiB, 64), 16 * kGiB);
  EXPECT_EQ(memory_total(0xFFFFFFFFull, 64), 0xFFFFFFFFll);
  // The driver figure is only for builds that cannot express the size.
  EXPECT_EQ(memory_total(16ull * kGiB, 64, 8ull * kGiB), 16 * kGiB);
}

TEST(CheckGpu, ClampedThirtyTwoBitTotalsAreUnknown) {
  EXPECT_EQ(memory_total(0xFFFFFFFFull, 32), 0);
  EXPECT_EQ(memory_total(0xFFF00000ull, 32), 0);
  EXPECT_EQ(memory_total(gpu_check::clamped_size_floor_32, 32), 0);
  // Below the ceiling the 32-bit figure is a real size.
  EXPECT_EQ(memory_total(2ull * kGiB, 32), 2 * kGiB);
  EXPECT_EQ(memory_total(gpu_check::clamped_size_floor_32 - 1, 32), static_cast<long long>(gpu_check::clamped_size_floor_32 - 1));
  EXPECT_EQ(memory_total(0, 32), 0);
}

TEST(CheckGpu, ThirtyTwoBitTotalsUseTheDriverFigure) {
  EXPECT_EQ(memory_total(0xFFFFFFFFull, 32, 16ull * kGiB), 16 * kGiB);
  EXPECT_EQ(memory_total(2ull * kGiB, 32, 2ull * kGiB), 2 * kGiB);
  // A driver figure below what DXGI said contradicts it and is ignored.
  EXPECT_EQ(memory_total(0xFFFFFFFFull, 32, 2ull * kGiB), 0);
  EXPECT_EQ(memory_total(3ull * kGiB, 32, 2ull * kGiB), 3 * kGiB);
  // A size DXGI could express is kept even when the driver key says more.
  EXPECT_EQ(memory_total(kGiB / 8, 32, 2ull * kGiB), kGiB / 8);
}

TEST(CheckGpu, ClampedTotalGivesNoPercentage) {
  // What the x86 agent saw before: a 16 GiB card described as 4 GiB, 6 GiB in use.
  counter_snapshot s;
  s.adapter_memory[kAdapter] = {{"Dedicated Usage", 6.0 * kGiB}};
  adapter_info info = rtx();
  info.dedicated_total = memory_total(0xFFFFFFFFull, 32);
  const std::vector<gpu_adapter> adapters = build_adapters(s, {info});
  ASSERT_EQ(adapters.size(), 1u);
  EXPECT_EQ(adapters[0].dedicated_total, 0);
  EXPECT_EQ(adapters[0].get_dedicated_pct(), 0);
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check(adapters, {}, response), PB::Common::ResultCode::OK) << join_lines(response);
}

TEST(CheckGpu, DriverMemorySizeMatchesThePciDevice) {
  const unsigned long long size16 = 16ull * kGiB;
  const std::vector<driver_key> keys = {
      {"PCI\\VEN_10DE&DEV_24B0&SUBSYS_14AD10DE", size16},
      {"pci\\ven_8086&dev_9bc5", 0},  // no size recorded
      {"ROOT\\BasicRender", 0},
  };
  EXPECT_EQ(driver_memory_size(keys, 0x10DE, 0x24B0, 0x14AD10DE), size16);
  EXPECT_EQ(driver_memory_size(keys, 0x10DE, 0x24B0, 0x12345678), 0u);  // other subsystem
  EXPECT_EQ(driver_memory_size(keys, 0x8086, 0x9BC5, 0), 0u);
  EXPECT_EQ(driver_memory_size(keys, 0x1002, 0x73BF, 0), 0u);
  EXPECT_EQ(driver_memory_size({}, 0x10DE, 0x24B0, 0x14AD10DE), 0u);

  // A key without a subsystem matches every card of the chip.
  const std::vector<driver_key> generic = {{"pci\\ven_10de&dev_24b0", size16}};
  EXPECT_EQ(driver_memory_size(generic, 0x10DE, 0x24B0, 0x14AD10DE), size16);
}

TEST(CheckGpu, AmbiguousDriverMemorySizeIsUnknown) {
  const std::vector<driver_key> keys = {
      {"pci\\ven_10de&dev_2803", 8ull * kGiB},
      {"pci\\ven_10de&dev_2803", 16ull * kGiB},
  };
  EXPECT_EQ(driver_memory_size(keys, 0x10DE, 0x2803, 0), 0u);
  const std::vector<driver_key> agreeing = {
      {"pci\\ven_10de&dev_2803", 8ull * kGiB},
      {"pci\\ven_10de&dev_2803&subsys_00000000", 8ull * kGiB},
  };
  EXPECT_EQ(driver_memory_size(agreeing, 0x10DE, 0x2803, 0), 8ull * kGiB);
}

// --- DXGI enumeration failure ------------------------------------------------

namespace {
PB::Common::ResultCode run_full_check(const counter_snapshot &snapshot, const adapter_enumeration &enumeration, const std::vector<std::string> &args,
                                      PB::Commands::QueryResponseMessage::Response &response) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_gpu");
  for (const std::string &a : args) request.add_arguments(a);
  gpu_check::check_gpu_from(request, &response, snapshot, enumeration);
  return response.result();
}
}  // namespace

TEST(CheckGpu, FailedEnumerationIsUnknownWithTheReason) {
  // The Basic Render Driver is all the counters see; without DXGI it could not
  // be told apart from a GPU and would pass the default filter as healthy.
  counter_snapshot s;
  s.adapter_memory[kAdapter] = {{"Dedicated Usage", 1.0 * kGiB}};
  adapter_enumeration failed;
  failed.error = "CreateDXGIFactory1 failed: 0x887A0001";

  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_full_check(s, failed, {}, response), PB::Common::ResultCode::UNKNOWN) << join_lines(response);
  EXPECT_EQ(join_lines(response),
            "GPU adapters could not be identified through DXGI, so their names, types and memory sizes are unknown (CreateDXGIFactory1 failed: "
            "0x887A0001)");

  // empty-state=ok does not hide it: it is not an empty result.
  PB::Commands::QueryResponseMessage::Response lenient;
  EXPECT_EQ(run_full_check(s, failed, {"empty-state=ok", "filter=none"}, lenient), PB::Common::ResultCode::UNKNOWN) << join_lines(lenient);
}

TEST(CheckGpu, SuccessfulEmptyEnumerationRunsTheCheck) {
  // DXGI answered and listed nothing: the counters' adapters are reported as unknown.
  counter_snapshot s;
  s.adapter_memory[kAdapter] = {};
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_full_check(s, adapter_enumeration(), {"top-syntax=${list}", "detail-syntax=${name} ${adapter_type}"}, response), PB::Common::ResultCode::OK)
      << join_lines(response);
  EXPECT_EQ(join_lines(response), kLuid + " unknown");

  PB::Commands::QueryResponseMessage::Response none;
  EXPECT_EQ(run_full_check(counter_snapshot(), adapter_enumeration(), {}, none), PB::Common::ResultCode::UNKNOWN) << join_lines(none);
  EXPECT_NE(join_lines(none).find("No GPU adapters found"), std::string::npos) << join_lines(none);
}

TEST(CheckGpu, SuccessfulEnumerationNamesTheAdapters) {
  counter_snapshot s;
  s.adapter_memory[kAdapter] = {{"Dedicated Usage", 4.0 * kGiB}};
  adapter_enumeration ok;
  ok.adapters.push_back(rtx());
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_full_check(s, ok, {}, response), PB::Common::ResultCode::OK) << join_lines(response);
  EXPECT_NE(join_lines(response).find("NVIDIA RTX A4000: 0% busy"), std::string::npos) << join_lines(response);
}
