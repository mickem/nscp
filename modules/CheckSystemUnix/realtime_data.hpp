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
#include <boost/filesystem/path.hpp>
#include <list>
#include <parsers/filter/modern_filter.hpp>

#include "check_cpu.h"
#include "check_memory.h"
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
