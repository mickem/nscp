/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <boost/shared_ptr.hpp>
#include <boost/unordered_set.hpp>
#include <list>
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
  void add_obj(boost::shared_ptr<filters::proc::filter_config_object> object);
  void boot();
  void check();
  std::set<std::string> check_shared();
};
}  // namespace realtime

namespace active {

void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
}
}  // namespace process_checks
