// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <map>
#include <string>
#include <vector>

// The platform-neutral half of check_gpu: turning the instances of the Windows
// "GPU ..." performance counter sets into one record per adapter. Nothing in
// here touches PDH or DXGI (gpu_source_win.cpp does), and it depends on the
// standard library only, so the aggregation is unit-testable with canned
// instance names.
namespace gpu_check {

// What a counter instance name says about itself. The four counter sets spell
// their instances from the same building blocks:
//   GPU Engine                pid_<pid>_luid_<hi>_<lo>_phys_<n>_eng_<n>_engtype_<type>
//   GPU Process Memory        pid_<pid>_luid_<hi>_<lo>_phys_<n>
//   GPU Adapter Memory        luid_<hi>_<lo>_phys_<n>
//   GPU Local Adapter Memory  luid_<hi>_<lo>_phys_<n>_part_<n>
// A part an instance does not carry stays at -1 / empty.
struct instance_id {
  long long pid = -1;
  // Normalised to "0xHHHHHHHH_0xLLLLLLLL" (upper-case hex), the form
  // format_luid() produces, so a counter instance and a DXGI adapter compare
  // equal however the counter set happened to spell its digits.
  std::string luid;
  int phys = -1;
  int engine = -1;
  std::string engine_type;
  int part = -1;
};

// False when `name` carries no parsable luid - then it names no adapter and
// the instance is skipped.
bool parse_instance(const std::string &name, instance_id &out);

std::string format_luid(unsigned long high, unsigned long low);

// The engine type reduced to a class: lower case, without spaces, underscores
// or a trailing engine number ("Compute_0" -> "compute", "VideoDecode" ->
// "videodecode", "3D" -> "3d"). CUDA engines fold into "compute".
std::string engine_class(const std::string &engine_type);

// What the adapter says about itself (DXGI), keyed on its luid. The counter
// sets carry usage only: no name and no capacity.
struct adapter_info {
  std::string luid;
  std::string name;
  bool software = false;
  long long dedicated_total = 0;
  long long shared_total = 0;
};

// instance name -> (counter name -> value); the shape PDH::gather_object_instances returns.
typedef std::map<std::string, std::map<std::string, double> > instance_values;

struct counter_snapshot {
  instance_values engine;          // GPU Engine: "Utilization Percentage"
  instance_values adapter_memory;  // GPU Adapter Memory: "Dedicated Usage", "Shared Usage", "Total Committed"
  instance_values local_memory;    // GPU Local Adapter Memory: "Local Usage"
  instance_values process_memory;  // GPU Process Memory: "Dedicated Usage", "Shared Usage"
};

// One adapter, as the check reports it. Owns every value it exposes.
struct gpu_adapter {
  std::string name;
  std::string luid;
  long long phys = 0;
  std::string adapter_type;  // hardware, software or unknown (the adapter was not found through DXGI)
  long long utilization = 0;
  long long utilization_3d = 0;
  long long utilization_copy = 0;
  long long utilization_video_decode = 0;
  long long utilization_video_encode = 0;
  long long utilization_compute = 0;
  std::string busiest_engine;
  long long dedicated_used = 0;
  long long dedicated_total = 0;
  long long shared_used = 0;
  long long shared_total = 0;
  long long committed = 0;
  long long local_used = 0;
  long long processes = 0;

  // 0 when the capacity is unknown.
  long long get_dedicated_pct() const;
  long long get_shared_pct() const;

  std::string show() const;
};

// One record per (luid, phys) the counter sets mention, ordered by luid and
// then phys. `adapters` supplies names and capacities; an adapter it does not
// list is reported under its luid, as type "unknown", with no capacity.
std::vector<gpu_adapter> build_adapters(const counter_snapshot &snapshot, const std::vector<adapter_info> &adapters);

}  // namespace gpu_check
