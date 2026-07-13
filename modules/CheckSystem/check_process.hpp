// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/unordered_set.hpp>
#include <list>
#include <map>
#include <memory>
#include <nscapi/protobuf/command.hpp>
#include <set>
#include <string>

#include "filter_config_object.hpp"

namespace process_checks {

// Per-process CPU usage sampled by the CheckSystem background collector (see
// pdh_thread). The collector diffs two consecutive 1 Hz snapshots of the
// system-wide process table and publishes a whole-percent CPU reading per PID,
// so the active check_process path can report CPU% without doing its own
// sample / sleep / sample. creation_time (unix seconds, matching
// process_info::get_creation_time) disambiguates a recycled PID: the active
// check only trusts an entry whose creation time matches the process it found.
struct cpu_delta {
  long long kernel_pct{};
  long long user_pct{};
  long long total_pct{};
  unsigned long long creation_time{};
};
// Keyed by pid (as long long to keep this header free of <windows.h>).
typedef std::map<long long, cpu_delta> cpu_delta_map;

namespace realtime {

// Case-insensitive any-of match used by the realtime check_process path.
// Windows process / executable names are case-insensitive (NTFS is
// case-preserving but case-insensitive), so a configured `process=notepad.exe`
// must match a running process whose on-disk image is `NOTEPAD.EXE`. The
// active check path already does this via CaseBlindCompare; this helper
// keeps the realtime path in sync (issues #587, #552). Exposed at namespace
// scope so it is unit-testable without dragging in runtime_data.
bool process_name_matches_any(const std::list<std::string> &names, const std::string &candidate);

struct proc_filter_helper_wrapper;
struct helper {
  typedef boost::unordered_set<std::string> known_type;
  known_type known_processes_;
  proc_filter_helper_wrapper *proc_helper;

  helper(nscapi::core_wrapper *core, int plugin_id);
  void add_obj(std::shared_ptr<filters::proc::filter_config_object> object);
  void boot();
  void check();
  std::set<std::string> check_shared();
  std::map<std::string, long long> get_counts() const;
};
}  // namespace realtime

namespace active {

// cpu_deltas is the collector's latest per-PID CPU% snapshot and
// cpu_collector_enabled reflects whether the background sampler is turned on.
// When delta=true is requested the check overlays cpu_deltas onto the freshly
// enumerated processes; if the collector is disabled it returns UNKNOWN asking
// the user to enable it.
void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response, const cpu_delta_map &cpu_deltas,
           bool cpu_collector_enabled);
}
}  // namespace process_checks
