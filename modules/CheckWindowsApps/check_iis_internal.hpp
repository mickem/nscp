// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <map>
#include <string>
#include <vector>
#include <win/pdh/pdh_object_gather.hpp>

namespace check_iis {
namespace check_iis_internal {

// All instances of one PDH object: instance name -> (counter name -> value).
typedef PDH::object_instance_values instance_values;

// Human-readable name for an APP_POOL_WAS "Current Application Pool State"
// value (per the WAS performance counter documentation).
inline std::string pool_state_name(const long long state) {
  switch (state) {
    case 1:
      return "uninitialized";
    case 2:
      return "initialized";
    case 3:
      return "running";
    case 4:
      return "disabling";
    case 5:
      return "disabled";
    case 6:
      return "shutdown_pending";
    case 7:
      return "delete_pending";
    default:
      return "unknown";
  }
}

// One application pool as reported by PDH, optionally enriched from WMI.
struct pool_record {
  std::string name;
  long long state = 0;      // Raw APP_POOL_WAS state (0 = not reported by PDH)
  long long uptime = 0;     // Seconds since the pool started
  long long recycles = 0;   // Total recycles since WAS started
  long long auto_start = -1;  // 1/0 from WMI; -1 when WMI is unavailable
};

// Merge the PDH pool instances with the WMI ApplicationPool rows: PDH is the
// primary source (state/uptime/recycles); WMI adds auto_start and contributes
// pools WAS has no counter instance for yet (never started since boot), which
// surface with state 0 = "unknown". Pure so it can be unit tested.
inline std::vector<pool_record> merge_pools(const instance_values &pdh_pools, const std::map<std::string, bool> &wmi_auto_start) {
  std::vector<pool_record> out;
  for (const auto &entry : pdh_pools) {
    pool_record rec;
    rec.name = entry.first;
    rec.state = static_cast<long long>(PDH::value_of(entry.second, "Current Application Pool State"));
    rec.uptime = static_cast<long long>(PDH::value_of(entry.second, "Current Application Pool Uptime"));
    rec.recycles = static_cast<long long>(PDH::value_of(entry.second, "Total Application Pool Recycles"));
    const auto auto_start = wmi_auto_start.find(rec.name);
    if (auto_start != wmi_auto_start.end()) rec.auto_start = auto_start->second ? 1 : 0;
    out.push_back(rec);
  }
  for (const auto &entry : wmi_auto_start) {
    if (pdh_pools.count(entry.first) > 0) continue;
    pool_record rec;
    rec.name = entry.first;
    rec.auto_start = entry.second ? 1 : 0;
    out.push_back(rec);
  }
  return out;
}

// One web site as reported by the "Web Service" PDH object, optionally
// enriched from WMI.
struct site_record {
  std::string name;
  long long connections = 0;
  long long uptime = 0;  // Service Uptime in seconds
  double requests_per_sec = 0.0;
  double bytes_per_sec = 0.0;
  long long auto_start = -1;  // 1/0 from WMI; -1 when WMI is unavailable
  bool in_pdh = false;        // False for WMI-only sites (no counter instance)
};

inline std::vector<site_record> merge_sites(const instance_values &pdh_sites, const std::map<std::string, bool> &wmi_auto_start) {
  std::vector<site_record> out;
  for (const auto &entry : pdh_sites) {
    site_record rec;
    rec.name = entry.first;
    rec.in_pdh = true;
    rec.connections = static_cast<long long>(PDH::value_of(entry.second, "Current Connections"));
    rec.uptime = static_cast<long long>(PDH::value_of(entry.second, "Service Uptime"));
    rec.requests_per_sec = PDH::value_of(entry.second, "Total Method Requests/sec");
    rec.bytes_per_sec = PDH::value_of(entry.second, "Bytes Total/sec");
    const auto auto_start = wmi_auto_start.find(rec.name);
    if (auto_start != wmi_auto_start.end()) rec.auto_start = auto_start->second ? 1 : 0;
    out.push_back(rec);
  }
  for (const auto &entry : wmi_auto_start) {
    if (pdh_sites.count(entry.first) > 0) continue;
    site_record rec;
    rec.name = entry.first;
    rec.auto_start = entry.second ? 1 : 0;
    out.push_back(rec);
  }
  return out;
}

// A W3SVC_W3WP instance is named "<pid>_<pool name>"; split it. Returns false
// (leaving pid 0 and pool = the raw instance) when the shape is unexpected.
inline bool parse_worker_instance(const std::string &instance, long long &pid, std::string &pool) {
  const auto sep = instance.find('_');
  if (sep == std::string::npos || sep == 0) {
    pid = 0;
    pool = instance;
    return false;
  }
  const std::string pid_part = instance.substr(0, sep);
  for (const char c : pid_part) {
    if (c < '0' || c > '9') {
      pid = 0;
      pool = instance;
      return false;
    }
  }
  pid = std::stoll(pid_part);
  pool = instance.substr(sep + 1);
  return true;
}

}  // namespace check_iis_internal
}  // namespace check_iis
