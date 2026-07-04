/*
 * Copyright (C) 2004-2026 Michael Medin
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

#include <algorithm>
#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <string>

namespace load_check {

// One row of load-average data. When percpu is requested the three averages
// are pre-divided by the CPU count and `type` becomes "scaled".
struct load_obj {
  std::string type;  // "total" or "scaled"
  double load1;
  double load5;
  double load15;
  long long procs_running;
  long long procs_total;

  load_obj() : type("total"), load1(0), load5(0), load15(0), procs_running(0), procs_total(0) {}

  std::string get_type() const { return type; }
  double get_load1() const { return load1; }
  double get_load5() const { return load5; }
  double get_load15() const { return load15; }
  double get_load() const { return std::max(load1, std::max(load5, load15)); }
  long long get_procs_running() const { return procs_running; }
  long long get_procs_total() const { return procs_total; }

  std::string show() const {
    return type + " load average: " + std::to_string(load1) + ", " + std::to_string(load5) + ", " + std::to_string(load15);
  }
};

// Parse the contents of /proc/loadavg. `ncpu` (>=1) and `percpu` control
// whether the averages are divided by the CPU count. Returns false on a
// malformed first field.
bool parse_loadavg(const std::string &content, int ncpu, bool percpu, load_obj &out);

typedef parsers::where::filter_handler_impl<std::shared_ptr<load_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<load_obj, filter_obj_handler> filter_type;

// Testable variant: reads the load average from an explicit file path.
void check_load_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                     const std::string &loadavg_path);

void check_load(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

}  // namespace load_check
