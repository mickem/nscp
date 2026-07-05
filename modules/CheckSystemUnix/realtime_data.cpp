// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "realtime_data.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <map>
#include <nscapi/macros.hpp>
#include <set>
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
      const std::shared_ptr<filter_obj> record(new filter_obj(c.alias, v.first, load.user, load.kernel, load.idle));
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
    const std::shared_ptr<filter_obj> record(new filter_obj(type, free_v, total));
    ret.append(filter.match(record));
  }
  return ret;
}
}  // namespace check_mem_filter
}  // namespace check_memory

namespace check_proc {
namespace check_proc_filter {
void runtime_data::add(const std::string &proc) {
  if (proc == "*") {
    check_all = true;
  } else {
    checks.push_back(boost::algorithm::to_lower_copy(proc));
  }
}

modern_filter::match_result runtime_data::process_item(filter_type &filter, transient_data_type) const {
  modern_filter::match_result ret;
  const bool all = check_all || checks.empty();
  const std::set<std::string> wanted(checks.begin(), checks.end());
  for (const filter_obj &info : enumerate_processes()) {
    if (all || wanted.count(boost::algorithm::to_lower_copy(info.exe)) > 0) {
      const std::shared_ptr<filter_obj> record(new filter_obj(info));
      ret.append(filter.match(record));
    }
  }
  return ret;
}
}  // namespace check_proc_filter
}  // namespace check_proc
