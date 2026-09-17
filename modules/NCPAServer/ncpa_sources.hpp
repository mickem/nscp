// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/thread/mutex.hpp>
#include <map>
#include <nscapi/protobuf/metrics.hpp>
#include <string>
#include <vector>

#include "ncpa_tree.hpp"

// Where the NCPA tree gets its numbers. The module measures nothing itself:
// everything below `cpu`, `memory`, `disk` and `interface` comes from the 1 Hz
// metrics snapshot the agent already publishes, and the rest comes from a small
// per-platform shim and from the agent's own queries.
namespace ncpa {

// The latest metrics snapshot, flattened to the dotted keys every other metrics
// consumer reads ("system.mem.physical.total", "disk.free.C:.used"). Written
// once a second by the metrics thread and read by any number of HTTP workers,
// so every access takes the lock.
class metrics_snapshot {
 public:
  void set(const PB::Metrics::MetricsMessage &message);

  bool get_number(const std::string &key, double &out) const;
  bool get_string(const std::string &key, std::string &out) const;
  // The distinct names one level below `prefix`: instances("system.cpu.")
  // answers core_0, core_1, ... and total.
  std::vector<std::string> instances(const std::string &prefix) const;
  bool empty() const;

 private:
  std::map<std::string, double> numbers_;
  std::map<std::string, std::string> strings_;
  mutable boost::mutex mutex_;
};

// Facts about the machine that no metric carries. Filled by the platform shim
// (ncpa_sysinfo_unix.cpp / ncpa_sysinfo_win.cpp).
struct system_info {
  std::string system;     // "Linux", "Windows"
  std::string node;       // host name
  std::string release;    // kernel / OS release
  std::string version;    // build version string
  std::string machine;    // "x86_64"
  std::string processor;  // CPU model, when the platform names one
  std::string timezone;
};

namespace sysinfo {
// Everything `system/*` reports beyond the agent version.
system_info gather();
// The interactive logon sessions `user/*` reports.
std::vector<std::string> logged_on_users();
}  // namespace sysinfo

struct tree_options {
  // The version string `system/agent_version` reports. Empty when
  // `expose version` is off, in which case NCPA's own node still exists (the
  // XI wizard reads it) but answers a redacted string.
  std::string agent_version;
  bool expose_version = true;
};

// Assemble the whole `/api` tree for one request. Cheap: it copies the values it
// needs out of the snapshot and builds a few dozen nodes, which is what lets the
// tree itself stay free of any locking or dispatch.
node_ptr build_root(const metrics_snapshot &snapshot, const tree_options &options, query_dispatcher *dispatcher);

}  // namespace ncpa
