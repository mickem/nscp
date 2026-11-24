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

// Which per-type load keywords an engine counts toward. Drivers spell the
// engine types freely, so this goes by what the class contains rather than by
// exact names:
//   contains "3d"           -> 3D       ("3D", "High Priority 3D")
//   contains "copy"         -> copy     ("Copy")
//   contains "videodecode"  -> decode
//   contains "videoencode"  -> encode
//   contains "videocodec"   -> decode and encode (one engine does both; "Video Codec 0")
//   contains "compute"/"cuda" -> compute ("Compute_0", "High Priority Compute", "Cuda")
// Anything else - "VideoProcessing" (scaling, deinterlacing and colour
// conversion, neither decode nor encode), "Security", "Overlay", ... - counts
// toward none of them; it still counts toward utilization and busiest_engine.
struct engine_kinds {
  bool is_3d = false;
  bool copy = false;
  bool video_decode = false;
  bool video_encode = false;
  bool compute = false;
};
engine_kinds classify_engine(const std::string &engine_type);

// DXGI reports memory sizes as SIZE_T, which is 32 bits wide in the x86
// agent: a card with 4 GB or more cannot be described there and the size
// comes back clamped near the 32-bit ceiling, while the usage counters are
// 64-bit. A 32-bit size at or above this floor is taken to be clamped. It
// costs a genuine ~4 GB card its percentages on the x86 agent, which beats a
// dedicated_pct far above 100.
const unsigned long long clamped_size_floor_32 = 0xF0000000ull;  // 3.75 GiB

// The capacity to report for a DXGI memory size. `size_bits` is the width of
// SIZE_T in the build that read it (sizeof(SIZE_T) * 8). `driver_size` is the
// adapter's memory size as its driver key records it (see
// driver_memory_size()), 0 when unknown; only a 32-bit build consults it, only
// for a clamped size, and only when it is consistent with what DXGI said (not
// below it). 0 means
// unknown: no percentage is computed from it.
long long memory_total(unsigned long long reported, unsigned size_bits, unsigned long long driver_size = 0);

// One driver key of the display adapter class
// (HKLM\SYSTEM\CurrentControlSet\Control\Class\{4d36e968-...}\NNNN).
struct driver_key {
  std::string matching_device_id;      // "PCI\VEN_10DE&DEV_24B0&SUBSYS_14AD10DE" or "pci\ven_10de&dev_24b0"
  unsigned long long memory_size = 0;  // HardwareInformation.qwMemorySize; 0 when absent
};

// The memory size the driver keys record for the PCI device DXGI describes
// (DXGI_ADAPTER_DESC1 VendorId/DeviceId/SubSysId). A key whose
// MatchingDeviceId names a subsystem must match that too. Every matching key
// that records a size has to agree; 0 when none does or they disagree (two
// variants of one chip with different memory sizes, or a stale key), so an
// ambiguous match never yields a wrong total.
unsigned long long driver_memory_size(const std::vector<driver_key> &keys, unsigned vendor_id, unsigned device_id, unsigned subsys_id);

// What the adapter says about itself (DXGI), keyed on its luid. The counter
// sets carry usage only: no name and no capacity.
struct adapter_info {
  std::string luid;
  std::string name;
  bool software = false;
  long long dedicated_total = 0;
  long long shared_total = 0;
};

// The result of asking DXGI for the adapters. `error` says why the
// enumeration failed and is empty when it succeeded - an empty `adapters` with
// no error means DXGI listed nothing. Without DXGI every adapter would be
// reported as "unknown" with no capacity, which the default filter and
// thresholds would pass as healthy, so the check reports a failure instead.
struct adapter_enumeration {
  std::vector<adapter_info> adapters;  // in DXGI enumeration order
  std::string error;
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
// then phys. `adapters` supplies names and capacities, in DXGI enumeration
// order; an adapter it does not list is reported under its luid, as type
// "unknown", with no capacity.
//
// Names are unique: when several records would share a name - linked adapters
// (one luid, several phys) or several cards of one model - each gets " #<n>",
// n counting from 0 in DXGI enumeration order and then phys. That order
// follows the PCI slots and survives a reboot, unlike the luid. For a single
// linked adapter n is its phys index; a name nothing else shares is left as
// it is.
std::vector<gpu_adapter> build_adapters(const counter_snapshot &snapshot, const std::vector<adapter_info> &adapters);

}  // namespace gpu_check
