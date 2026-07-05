// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "realtime_data.hpp"

#include <boost/filesystem.hpp>
#include <str/format.hpp>

namespace check_cpu_filter {
void runtime_data::add(const std::string &time) {
  container c;
  c.alias = time;
  c.time = str::format::decode_time<long>(time, 1);
  checks.push_back(c);
}

modern_filter::match_result runtime_data::process_item(filter_type &filter, transient_data_type thread) const {
  modern_filter::match_result ret;
  for (const container &c : checks) {
    std::map<std::string, windows::system_info::load_entry> vals = thread->get_cpu_load(c.time);
    typedef std::map<std::string, windows::system_info::load_entry>::value_type vt;
    for (vt v : vals) {
      const std::shared_ptr<filter_obj> record(new filter_obj(c.alias, v.first, v.second));
      ret.append(filter.match(record));
    }
  }
  return ret;
}
}  // namespace check_cpu_filter
