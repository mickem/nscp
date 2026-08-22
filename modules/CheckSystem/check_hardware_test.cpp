// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_hardware.hpp"

#include <gtest/gtest.h>

using hardware_check::chassis_type_to_string;
using hardware_check::hardware_info;
using hardware_check::memory_module;
using hardware_check::parse_first_array_int;

namespace {

// Minimal evaluation context carrying the default number format.
struct mock_evaluation_context final : parsers::where::evaluation_context_interface {
  bool has_error() const override { return false; }
  std::string get_error() const override { return ""; }
  void error(std::string) override {}
  bool has_warn() const override { return false; }
  std::string get_warn() const override { return ""; }
  void warn(std::string) override {}
  void clear() override {}
  void enable_debug(bool) override {}
  bool debug_enabled() override { return false; }
  std::string get_debug() const override { return ""; }
  void debug(parsers::where::object_match) override {}
};

parsers::where::evaluation_context make_context() { return std::make_shared<mock_evaluation_context>(); }

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

memory_module make_module(const std::string &locator, const long long capacity, const long long speed) {
  memory_module m;
  m.locator = locator;
  m.capacity = capacity;
  m.speed = speed;
  return m;
}

hardware_info sample_info() {
  hardware_info h;
  h.vendor = "Dell Inc.";
  h.model = "PowerEdge R750";
  h.uuid = "4C4C4544-0042-3510-8054-B9C04F515733";
  h.serial = "ABC1234";
  h.chassis_type = 23;
  h.chassis = chassis_type_to_string(23);
  h.slots = 8;
  h.module_details = {
      make_module("DIMM_A1", 32LL * 1024 * 1024 * 1024, 4800),
      make_module("DIMM_B1", 32LL * 1024 * 1024 * 1024, 4400),
  };
  h.recompute();
  return h;
}

PB::Common::ResultCode run_check(const hardware_info &info, const std::vector<std::string> &args, PB::Commands::QueryResponseMessage::Response &response) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_hardware");
  for (const std::string &a : args) request.add_arguments(a);
  hardware_check::check_from(request, &response, info);
  return response.result();
}

}  // namespace

// --- pure helpers ---------------------------------------------------------------

TEST(CheckHardware, ChassisTypeMapping) {
  EXPECT_EQ(chassis_type_to_string(3), "Desktop");
  EXPECT_EQ(chassis_type_to_string(9), "Laptop");
  EXPECT_EQ(chassis_type_to_string(23), "Rack Mount Chassis");
  EXPECT_EQ(chassis_type_to_string(35), "Mini PC");
  EXPECT_EQ(chassis_type_to_string(99), "Unknown (99)");
}

TEST(CheckHardware, ParsesWmiArrayRendering) {
  EXPECT_EQ(parse_first_array_int("[23]"), 23);
  EXPECT_EQ(parse_first_array_int("[3, 4]"), 3);
  EXPECT_EQ(parse_first_array_int("17"), 17);  // bare value tolerated
  EXPECT_EQ(parse_first_array_int(""), 0);
  EXPECT_EQ(parse_first_array_int("<NULL>"), 0);
  EXPECT_EQ(parse_first_array_int("[]"), 0);
}

TEST(CheckHardware, RecomputeAggregatesModules) {
  const hardware_info h = sample_info();
  EXPECT_EQ(h.modules, 2);
  EXPECT_EQ(h.memory, 64LL * 1024 * 1024 * 1024);
  EXPECT_EQ(h.memory_speed, 4400);  // slowest module
  EXPECT_EQ(h.module_list, "DIMM_A1: 32GB@4800MHz; DIMM_B1: 32GB@4400MHz");
  EXPECT_EQ(h.get_memory_human(make_context()), "64GB");
}

// --- rendering / thresholds ------------------------------------------------------

TEST(CheckHardware, DefaultIsOkInventoryWithPerf) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check(sample_info(), {}, response), PB::Common::ResultCode::OK) << join_lines(response);
  const std::string msg = join_lines(response);
  EXPECT_NE(msg.find("Dell Inc. PowerEdge R750 (Rack Mount Chassis)"), std::string::npos) << msg;
  EXPECT_NE(msg.find("serial=ABC1234"), std::string::npos) << msg;
  EXPECT_NE(msg.find("2 memory module(s), 64GB"), std::string::npos) << msg;
  EXPECT_TRUE(has_perf(response, "memory")) << msg;
  EXPECT_TRUE(has_perf(response, "modules"));
}

TEST(CheckHardware, PinnedSerialMismatchIsCritical) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check(sample_info(), {"crit=serial != 'XYZ9999'"}, response), PB::Common::ResultCode::CRITICAL) << join_lines(response);
  PB::Commands::QueryResponseMessage::Response ok_response;
  EXPECT_EQ(run_check(sample_info(), {"crit=serial != 'ABC1234'"}, ok_response), PB::Common::ResultCode::OK) << join_lines(ok_response);
}

TEST(CheckHardware, DroppedDimmTripsModuleAndMemoryThresholds) {
  hardware_info h = sample_info();
  h.module_details.pop_back();  // one DIMM died
  h.recompute();
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check(h, {"warn=modules < 2", "crit=memory < 48G"}, response), PB::Common::ResultCode::CRITICAL) << join_lines(response);
  PB::Commands::QueryResponseMessage::Response ok_response;
  EXPECT_EQ(run_check(sample_info(), {"warn=modules < 2", "crit=memory < 48G"}, ok_response), PB::Common::ResultCode::OK) << join_lines(ok_response);
}

TEST(CheckHardware, ChassisPolicyExpression) {
  hardware_info laptop = sample_info();
  laptop.chassis_type = 9;
  laptop.chassis = chassis_type_to_string(9);
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check(laptop, {"warn=chassis like 'Laptop'"}, response), PB::Common::ResultCode::WARNING) << join_lines(response);
  PB::Commands::QueryResponseMessage::Response ok_response;
  EXPECT_EQ(run_check(sample_info(), {"warn=chassis like 'Laptop'"}, ok_response), PB::Common::ResultCode::OK) << join_lines(ok_response);
}

TEST(CheckHardware, EmptySlotDetection) {
  PB::Commands::QueryResponseMessage::Response response;
  // 2 of 8 slots populated; alert policies can compare against the fixed count.
  EXPECT_EQ(run_check(sample_info(), {"warn=modules < 8"}, response), PB::Common::ResultCode::WARNING) << join_lines(response);
}

TEST(CheckHardware, BlankVmInventoryStaysOk) {
  // A minimal VM: no serial, no slots, chassis "Other", one virtual DIMM.
  hardware_info h;
  h.vendor = "Microsoft Corporation";
  h.model = "Virtual Machine";
  h.chassis_type = 1;
  h.chassis = chassis_type_to_string(1);
  h.module_details = {make_module("M0", 8LL * 1024 * 1024 * 1024, 0)};
  h.recompute();
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check(h, {}, response), PB::Common::ResultCode::OK) << join_lines(response);
  EXPECT_NE(join_lines(response).find("Virtual Machine (Other)"), std::string::npos) << join_lines(response);
}
