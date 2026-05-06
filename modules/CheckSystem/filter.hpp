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

#include <boost/date_time.hpp>
#include <check/uptime/filter_obj.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <str/format.hpp>
#include <string>
#include <win/processes.hpp>
#include <win/services.hpp>
#include <win/sysinfo/win_sysinfo.hpp>

namespace check_cpu_filter {
struct filter_obj {
  std::string time;
  std::string core;
  const windows::system_info::load_entry &value;

  filter_obj(std::string time, std::string core, const windows::system_info::load_entry &value) : time(time), core(core), value(value) {}

  std::string show() const {
    return core + " user: " + str::xtos(value.user) + "% kernel: " + str::xtos(value.kernel) + "% idle: " + str::xtos(value.idle) +
           "% total: " + str::xtos(get_total()) + "%";
  }

  long long get_total() const { return static_cast<long long>(value.user + value.kernel); }
  long long get_user() const { return static_cast<long long>(value.user); }
  long long get_idle() const { return static_cast<long long>(value.idle); }
  long long get_kernel() const { return static_cast<long long>(value.kernel); }
  std::string get_time() const { return time; }
  std::string get_core_s() const { return core; }
  std::string get_core_id() const { return boost::replace_all_copy(core, " ", "_"); }
  long long get_core_i() const { return value.core; }
};
typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj> > native_context;

struct filter_obj_handler : public native_context {
  filter_obj_handler();
};

typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;
}  // namespace check_cpu_filter

namespace check_page_filter {
struct filter_obj {
  const windows::system_info::pagefile_info &info;

  filter_obj(const windows::system_info::pagefile_info &info) : info(info) {}

  std::string show() const { return info.name; }

  long long get_peak() const { return info.peak_usage; }
  long long get_total() const { return info.size; }
  long long get_used() const { return info.usage; }
  long long get_free() const { return info.size - info.usage; }
  long long get_used_pct() const { return str::format::calc_pct_round(get_used(), get_total()); }
  long long get_free_pct() const { return str::format::calc_pct_round(get_free(), get_total()); }
  std::string get_used_pct_human() const { return str::format::format_pct(get_used(), get_total()); }
  std::string get_free_pct_human() const { return str::format::format_pct(get_free(), get_total()); }
  std::string get_name() const { return info.name; }

  std::string get_total_human() const { return str::format::format_byte_units(get_total()); }
  std::string get_used_human() const { return str::format::format_byte_units(get_used()); }
  std::string get_free_human() const { return str::format::format_byte_units(get_free()); }
};

typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;
}  // namespace check_page_filter

namespace check_uptime_filter {
typedef check_uptime_filter_common::filter_obj filter_obj;

typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;
}  // namespace check_uptime_filter

namespace os_version_filter {
struct filter_obj {
  long long major_version;
  long long minor_version;
  long long build;
  long long plattform;
  std::string version_s;
  long long version_i;
  std::string suite;

  filter_obj() : major_version(0), minor_version(0), build(0), plattform(0), version_i(0) {}

  std::string show() const { return version_s; }
  std::string to_string() const { return version_s; }
  long long get_major() const { return major_version; }
  long long get_minor() const { return minor_version; }
  long long get_build() const { return build; }
  long long get_plattform() const { return plattform; }
  std::string get_version_s() const { return version_s; }
  long long get_version_i() const { return version_i; }
  std::string get_suite_string() const { return suite; }
};
typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj> > native_context;

struct filter_obj_handler : public native_context {
  filter_obj_handler();
};

typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;
}  // namespace os_version_filter
