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

#include "filter.hpp"

#include <boost/assign.hpp>
#include <cmath>
#include <parsers/where/helpers.hpp>
#include <str/utils.hpp>

using namespace parsers::where;

namespace check_cpu_filter {
parsers::where::node_type calculate_load(boost::shared_ptr<filter_obj> object, parsers::where::evaluation_context context, parsers::where::node_type subject) {
  parsers::where::helpers::read_arg_type value = parsers::where::helpers::read_arguments(context, subject, "%");
  double number = value.get<1>();
  std::string unit = value.get<2>();

  if (unit != "%") context->error("Invalid unit: " + unit);
  return parsers::where::factory::create_int(llround(number));
}

filter_obj_handler::filter_obj_handler() {
  static const parsers::where::value_type type_custom_pct = parsers::where::type_custom_int_1;

  registry_.add_string("time", &filter_obj::get_time, "The time frame to check")
      .add_string("core", &filter_obj::get_core_s, &filter_obj::get_core_i, "The core to check (total or core ##)")
      .add_string("core_id", &filter_obj::get_core_id, &filter_obj::get_core_i, "The core to check (total or core_##)");
  registry_.add_int_x("load", type_custom_pct, &filter_obj::get_total, "deprecated (use total instead)")
      .add_int_perf("%")
      .add_int_x("total", type_custom_pct, &filter_obj::get_total, "The current load used by user and system")
      .add_int_perf("%")
      .add_int_x("user", type_custom_pct, &filter_obj::get_user, "The current load used by user applications")
      .add_int_perf("%")
      .add_int_x("idle", &filter_obj::get_idle, "The current idle load for a given core")
      .add_int_x("system", &filter_obj::get_kernel, "The current load used by the system (kernel)")
      .add_int_x("kernel", &filter_obj::get_kernel, "deprecated (use system instead)");

  registry_.add_converter()(type_custom_pct, &calculate_load);
}
}  // namespace check_cpu_filter

namespace check_page_filter {
parsers::where::node_type calculate_free(boost::shared_ptr<filter_obj> object, parsers::where::evaluation_context context, parsers::where::node_type subject) {
  parsers::where::helpers::read_arg_type value = parsers::where::helpers::read_arguments(context, subject, "%");
  double number = value.get<1>();
  std::string unit = value.get<2>();

  if (unit == "%") {
    number = (static_cast<double>(object->get_total()) * number) / 100.0;
  } else {
    number = str::format::decode_byte_units(number, unit);
  }
  return parsers::where::factory::create_int(llround(number));
}

long long get_zero() { return 0; }

filter_obj_handler::filter_obj_handler() {
  static const parsers::where::value_type type_custom_used = parsers::where::type_custom_int_1;
  static const parsers::where::value_type type_custom_free = parsers::where::type_custom_int_2;

  registry_.add_string("name", &filter_obj::get_name, "The name of the page file (location)");
  // clang-format off
  registry_.add_int()
    ("size", [] (auto obj, auto context) { return obj->get_total(); }, "Total size of pagefile")
    ("free", type_custom_free, [] (auto obj, auto context) { return obj->get_free(); }, "Free memory in bytes (g,m,k,b) or percentages %")
    .add_scaled_byte([] (auto obj, auto context) { return get_zero(); }, [] (auto obj, auto context) { return obj->get_total(); })
    .add_percentage([] (auto obj, auto context) { return obj->get_total(); }, "", " %")

    ("used", type_custom_used, [] (auto obj, auto context) { return obj->get_used(); }, "Used memory in bytes (g,m,k,b) or percentages %")
    .add_scaled_byte([] (auto obj, auto context) { return get_zero(); }, [] (auto obj, auto context) { return obj->get_total(); })
    .add_percentage([] (auto obj, auto context) { return obj->get_total(); }, "", " %")
    ("free_pct", [] (auto obj, auto context) { return obj->get_free_pct(); }, "% free memory")
    ("used_pct", [] (auto obj, auto context) { return obj->get_used_pct(); }, "% used memory")
    ;
  // clang-format on
  registry_.add_human_string("size", &filter_obj::get_total_human, "")
      .add_human_string("free", &filter_obj::get_free_human, "")
      .add_human_string("used", &filter_obj::get_used_human, "");

  registry_.add_converter()(type_custom_free, &calculate_free)(type_custom_used, &calculate_free);
}
}  // namespace check_page_filter

namespace check_uptime_filter {
parsers::where::node_type parse_time(boost::shared_ptr<filter_obj> object, parsers::where::evaluation_context context, parsers::where::node_type subject) {
  parsers::where::helpers::read_arg_type value = parsers::where::helpers::read_arguments(context, subject, "d");
  std::string expr = str::xtos(value.get<0>()) + value.get<2>();
  return parsers::where::factory::create_int(str::format::stox_as_time_sec<long long>(expr, "s"));
}

static const parsers::where::value_type type_custom_uptime = parsers::where::type_custom_int_1;
filter_obj_handler::filter_obj_handler() {
  registry_.add_int_x("boot", parsers::where::type_date, &filter_obj::get_boot, "System boot time")
      .add_int_x("uptime", type_custom_uptime, &filter_obj::get_uptime, "Time since last boot")
      .add_int_perf("s", "", "");
  registry_.add_converter()(type_custom_uptime, &parse_time);
  registry_.add_human_string("boot", &filter_obj::get_boot_s, "The system boot time")
      .add_human_string("uptime", &filter_obj::get_uptime_s, "Time sine last boot");
}
}  // namespace check_uptime_filter

namespace os_version_filter {
filter_obj_handler::filter_obj_handler() {
  registry_.add_int_x("major", &filter_obj::get_major, "Major version number")
      .add_int_perf("")
      .add_int_x("version", &filter_obj::get_version_i, &filter_obj::get_version_s, "The system version")
      .add_int_perf("")
      .add_int_x("minor", &filter_obj::get_minor, "Minor version number")
      .add_int_perf("")
      .add_int_x("build", &filter_obj::get_build, "Build version number")
      .add_int_perf("");
  registry_.add_string("suite", &filter_obj::get_suite_string,
                       "Which suites are installed on the machine (Microsoft BackOffice, Web Edition, Compute Cluster Edition, Datacenter Edition, Enterprise "
                       "Edition, Embedded, Home Edition, Remote Desktop Support, Small Business Server, Storage Server, Terminal Services, Home Server)");
}
}  // namespace os_version_filter