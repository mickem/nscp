// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once
#include <boost/filesystem/path.hpp>
#include <list>
#include <parsers/filter/modern_filter.hpp>

#include "check_cpu.h"
#include "check_memory.h"
#include "check_process.h"
#include "realtime_thread.hpp"

namespace checks {
namespace check_cpu_filter {
struct runtime_data {
  typedef checks::check_cpu_filter::filter filter_type;
  typedef pdh_thread *transient_data_type;

  struct container {
    std::string alias;
    long time;
  };

  std::list<container> checks;

  void boot() {}
  void touch(boost::posix_time::ptime /*now*/) {}
  bool has_changed(transient_data_type) const { return true; }
  modern_filter::match_result process_item(filter_type &filter, transient_data_type) const;
  void add(const std::string &time);
};
}  // namespace check_cpu_filter
}  // namespace checks

namespace check_memory {
namespace check_mem_filter {
struct runtime_data {
  typedef check_memory::check_mem_filter::filter filter_type;
  typedef pdh_thread *transient_data_type;

  std::list<std::string> checks;

  void boot() {}
  void touch(boost::posix_time::ptime /*now*/) {}
  bool has_changed(transient_data_type) const { return true; }
  modern_filter::match_result process_item(filter_type &filter, transient_data_type) const;
  void add(const std::string &type);
};
}  // namespace check_mem_filter
}  // namespace check_memory

namespace check_proc {
namespace check_proc_filter {
struct runtime_data {
  typedef check_proc::check_proc_filter::filter filter_type;
  typedef pdh_thread *transient_data_type;

  // Executable names to watch (lowercase); empty or "*" means all processes.
  std::list<std::string> checks;
  bool check_all = false;

  void boot() {}
  void touch(boost::posix_time::ptime /*now*/) {}
  bool has_changed(transient_data_type) const { return true; }
  modern_filter::match_result process_item(filter_type &filter, transient_data_type) const;
  void add(const std::string &proc);
};
}  // namespace check_proc_filter
}  // namespace check_proc
