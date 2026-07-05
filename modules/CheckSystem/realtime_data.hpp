// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once
#include <boost/filesystem/path.hpp>
#include <list>

#include "filter.hpp"
#include "pdh_thread.hpp"
#include "win/CheckMemory.h"

namespace check_cpu_filter {
struct runtime_data {
  typedef check_cpu_filter::filter filter_type;
  typedef pdh_thread *transient_data_type;

  struct container {
    std::string alias;
    long time;
  };

  std::list<container> checks;

  void boot() {}
  void touch(boost::posix_time::ptime _now) {}
  bool has_changed(transient_data_type) const { return true; }
  modern_filter::match_result process_item(filter_type &filter, transient_data_type) const;
  void add(const std::string &time);
};
}  // namespace check_cpu_filter
