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

#include "realtime_data.hpp"

#include <boost/filesystem.hpp>
#include <map>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <str/format.hpp>

namespace checks {
namespace check_cpu_filter {
void runtime_data::add(const std::string &time) {
  container c;
  c.alias = time;
  c.time = str::format::decode_time<long>(time, 1);
  checks.push_back(c);
}

modern_filter::match_result runtime_data::process_item(filter_type &filter, transient_data_type thread) const {
  modern_filter::match_result ret;
  if (!thread || !thread->has_cpu_data()) {
    return ret;
  }
  for (const container &c : checks) {
    std::map<std::string, load_entry> vals = thread->get_cpu_load(c.time);
    for (const auto &v : vals) {
      const load_entry &load = v.second;
      const boost::shared_ptr<filter_obj> record(new filter_obj(c.alias, v.first, load.user, load.kernel, load.idle));
      ret.append(filter.match(record));
    }
  }
  return ret;
}
}  // namespace check_cpu_filter
}  // namespace checks

namespace check_memory {
namespace check_mem_filter {
void runtime_data::add(const std::string &type) { checks.push_back(type); }

modern_filter::match_result runtime_data::process_item(filter_type &filter, transient_data_type thread) const {
  modern_filter::match_result ret;
  if (!thread || !thread->has_memory_data()) {
    return ret;
  }
  const memory_info mem_data = thread->get_memory(1);
  for (const std::string &type : checks) {
    unsigned long long total = 0, free_v = 0;
    if (type == "physical") {
      total = mem_data.physical.total;
      free_v = mem_data.physical.free;
    } else if (type == "cached") {
      total = mem_data.cached.total;
      free_v = mem_data.cached.free;
    } else if (type == "swap") {
      total = mem_data.swap.total;
      free_v = mem_data.swap.free;
    } else {
      continue;
    }
    const boost::shared_ptr<filter_obj> record(new filter_obj(type, free_v, total));
    ret.append(filter.match(record));
  }
  return ret;
}
}  // namespace check_mem_filter
}  // namespace check_memory
