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

void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
}
}  // namespace process_checks
