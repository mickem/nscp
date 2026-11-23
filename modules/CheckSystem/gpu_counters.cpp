// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "gpu_counters.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <utility>

namespace gpu_check {

namespace {

// The run of characters from `pos` up to the next '_' (or the end).
std::string token_at(const std::string &name, const std::string::size_type pos) {
  const std::string::size_type end = name.find('_', pos);
  return name.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
}

bool parse_number(const std::string &token, const int base, unsigned long long &out) {
  if (token.empty()) return false;
  char *end = nullptr;
  out = std::strtoull(token.c_str(), &end, base);
  return end != nullptr && *end == '\0';
}

// The decimal number following `marker`, or -1 when the marker is absent or
// is not followed by a number.
long long number_after(const std::string &name, const std::string &marker) {
  const std::string::size_type pos = name.find(marker);
  if (pos == std::string::npos) return -1;
  unsigned long long value = 0;
  if (!parse_number(token_at(name, pos + marker.size()), 10, value)) return -1;
  return static_cast<long long>(value);
}

double counter_value(const std::map<std::string, double> &values, const std::string &counter) {
  const auto it = values.find(counter);
  return it == values.end() ? 0.0 : it->second;
}

long long to_count(const double value) {
  // A counter that could not be formatted reads as 0 and a rate can dip
  // negative on a wrapped first sample; neither is a usage figure.
  if (!(value > 0.0)) return 0;
  return std::llround(value);
}

long long to_percent(const double value) { return std::min<long long>(to_count(value), 100); }

long long percent_of(const long long used, const long long total) {
  if (total <= 0 || used <= 0) return 0;
  return std::llround(static_cast<double>(used) * 100.0 / static_cast<double>(total));
}

typedef std::pair<std::string, int> adapter_key;  // luid, phys

struct engine_load {
  double utilization = 0.0;
  std::string type;
};

struct adapter_work {
  gpu_adapter record;
  std::map<int, engine_load> engines;
  std::set<long long> pids;
};

bool adapter_of(const std::string &instance, instance_id &id, adapter_key &key) {
  if (!parse_instance(instance, id)) return false;
  key = adapter_key(id.luid, id.phys < 0 ? 0 : id.phys);
  return true;
}

}  // namespace

std::string format_luid(const unsigned long high, const unsigned long low) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "0x%08lX_0x%08lX", high & 0xFFFFFFFFul, low & 0xFFFFFFFFul);
  return buf;
}

bool parse_instance(const std::string &name, instance_id &out) {
  out = instance_id();

  static const std::string luid_marker = "luid_";
  const std::string::size_type luid_pos = name.find(luid_marker);
  if (luid_pos == std::string::npos) return false;
  const std::string high_token = token_at(name, luid_pos + luid_marker.size());
  const std::string::size_type low_pos = luid_pos + luid_marker.size() + high_token.size() + 1;
  if (low_pos > name.size()) return false;
  unsigned long long high = 0;
  unsigned long long low = 0;
  if (!parse_number(high_token, 16, high) || !parse_number(token_at(name, low_pos), 16, low)) return false;
  out.luid = format_luid(static_cast<unsigned long>(high), static_cast<unsigned long>(low));

  if (name.compare(0, 4, "pid_") == 0) {
    unsigned long long pid = 0;
    if (parse_number(token_at(name, 4), 10, pid)) out.pid = static_cast<long long>(pid);
  }
  out.phys = static_cast<int>(number_after(name, "_phys_"));
  out.part = static_cast<int>(number_after(name, "_part_"));
  // "_eng_" cannot match inside "_engtype_": the latter has no underscore
  // after "eng".
  out.engine = static_cast<int>(number_after(name, "_eng_"));
  static const std::string type_marker = "_engtype_";
  const std::string::size_type type_pos = name.find(type_marker);
  // The type is the rest of the name: it can hold spaces and underscores of
  // its own ("Compute_0", "High Priority 3D").
  if (type_pos != std::string::npos) out.engine_type = name.substr(type_pos + type_marker.size());
  return true;
}

std::string engine_class(const std::string &engine_type) {
  std::string out;
  for (const char c : engine_type) {
    if (c == ' ' || c == '_') continue;
    out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  // "Compute_0", "Compute_1": the trailing number tells engines of one class
  // apart. "3D" ends in a letter and is left alone.
  while (out.size() > 1 && std::isdigit(static_cast<unsigned char>(out[out.size() - 1])) != 0) out.erase(out.size() - 1);
  if (out == "cuda") return "compute";
  return out;
}

long long gpu_adapter::get_dedicated_pct() const { return percent_of(dedicated_used, dedicated_total); }
long long gpu_adapter::get_shared_pct() const { return percent_of(shared_used, shared_total); }

std::string gpu_adapter::show() const { return name + " (" + std::to_string(utilization) + "%)"; }

std::vector<gpu_adapter> build_adapters(const counter_snapshot &snapshot, const std::vector<adapter_info> &adapters) {
  std::map<adapter_key, adapter_work> work;
  instance_id id;
  adapter_key key;

  for (const auto &entry : snapshot.adapter_memory) {
    if (!adapter_of(entry.first, id, key)) continue;
    gpu_adapter &record = work[key].record;
    record.dedicated_used += to_count(counter_value(entry.second, "Dedicated Usage"));
    record.shared_used += to_count(counter_value(entry.second, "Shared Usage"));
    record.committed += to_count(counter_value(entry.second, "Total Committed"));
  }

  // One instance per memory partition of the adapter.
  for (const auto &entry : snapshot.local_memory) {
    if (!adapter_of(entry.first, id, key)) continue;
    work[key].record.local_used += to_count(counter_value(entry.second, "Local Usage"));
  }

  // One instance per process per engine. An engine's load is what all the
  // processes on it add up to.
  for (const auto &entry : snapshot.engine) {
    if (!adapter_of(entry.first, id, key)) continue;
    engine_load &engine = work[key].engines[id.engine];
    const double value = counter_value(entry.second, "Utilization Percentage");
    if (value > 0.0) engine.utilization += value;
    if (engine.type.empty()) engine.type = id.engine_type;
  }

  for (const auto &entry : snapshot.process_memory) {
    if (!adapter_of(entry.first, id, key)) continue;
    // Every process that has opened the adapter keeps an instance; count the
    // ones that actually hold memory on it.
    if (id.pid < 0) continue;
    if (to_count(counter_value(entry.second, "Dedicated Usage")) + to_count(counter_value(entry.second, "Shared Usage")) <= 0) continue;
    work[key].pids.insert(id.pid);
  }

  // Linked adapters share one luid and differ in phys; only then does the
  // index need to show in the name.
  std::map<std::string, int> per_luid;
  for (const auto &entry : work) ++per_luid[entry.first.first];

  std::vector<gpu_adapter> out;
  for (auto &entry : work) {
    gpu_adapter &record = entry.second.record;
    record.luid = entry.first.first;
    record.phys = entry.first.second;
    record.name = record.luid;
    record.adapter_type = "unknown";
    for (const adapter_info &info : adapters) {
      if (info.luid != record.luid) continue;
      if (!info.name.empty()) record.name = info.name;
      record.adapter_type = info.software ? "software" : "hardware";
      record.dedicated_total = info.dedicated_total;
      record.shared_total = info.shared_total;
      break;
    }
    if (per_luid[record.luid] > 1) record.name += " #" + std::to_string(record.phys);

    // The adapter is as busy as its busiest engine - the figure Task Manager
    // shows. Engines run side by side, so adding them up would pass 100%.
    double busiest = 0.0;
    for (const auto &engine : entry.second.engines) {
      const long long load = to_percent(engine.second.utilization);
      const std::string cls = engine_class(engine.second.type);
      if (cls == "3d") record.utilization_3d = std::max(record.utilization_3d, load);
      if (cls == "copy") record.utilization_copy = std::max(record.utilization_copy, load);
      if (cls == "videodecode") record.utilization_video_decode = std::max(record.utilization_video_decode, load);
      if (cls == "videoencode") record.utilization_video_encode = std::max(record.utilization_video_encode, load);
      if (cls == "compute") record.utilization_compute = std::max(record.utilization_compute, load);
      if (engine.second.utilization > busiest) {
        busiest = engine.second.utilization;
        record.busiest_engine = engine.second.type;
      }
    }
    record.utilization = to_percent(busiest);
    record.processes = static_cast<long long>(entry.second.pids.size());
    out.push_back(record);
  }
  return out;
}

}  // namespace gpu_check
