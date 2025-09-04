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

  // clang-format off
  registry_.add_string()
    ("time", [] (auto obj, auto context) { return obj->get_time(); }, "The time frame to check")
    ("core", [] (auto obj, auto context) { return obj->get_core_s(); }, [] (auto obj, auto context) { return obj->get_core_i(); }, "The core to check (total or core ##)")
    ("core_id", [] (auto obj, auto context) { return obj->get_core_id(); }, [] (auto obj, auto context) { return obj->get_core_i(); }, "The core to check (total or core_##)")
    ;
  registry_.add_int()
    ("load", type_custom_pct, [] (auto obj, auto context) { return obj->get_total(); }, "deprecated (use total instead)").add_perf("%")
    ("total", type_custom_pct, [] (auto obj, auto context) { return obj->get_total(); }, "The current load used by user and system").add_perf("%")
    ("user", type_custom_pct, [] (auto obj, auto context) { return obj->get_user(); }, "The current load used by user applications").add_perf("%")
    ("idle", [] (auto obj, auto context) { return obj->get_idle(); }, "The current idle load for a given core")
    ("system", [] (auto obj, auto context) { return obj->get_kernel(); }, "The current load used by the system (kernel)")
    ("kernel", [] (auto obj, auto context) { return obj->get_kernel(); }, "deprecated (use system instead)")
    ;

  registry_.add_converter()
    (type_custom_pct, &calculate_load)
    ;
  // clang-format on
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

  // clang-format off
  registry_.add_string()
    ("name", [] (auto obj, auto context) { return obj->get_name(); }, "The name of the page file (location)")
    ;
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
  registry_.add_human_string()
    ("size", [] (auto obj, auto context) { return obj->get_total_human(); }, "")
    ("free", [] (auto obj, auto context) { return obj->get_free_human(); }, "")
    ("used", [] (auto obj, auto context) { return obj->get_used_human(); }, "")
    ;

  registry_.add_converter()
    (type_custom_free, &calculate_free)
    (type_custom_used, &calculate_free)
    ;
  // clang-format on
}
}  // namespace check_page_filter

namespace check_svc_filter {
bool check_state_is_perfect(DWORD state, DWORD start_type, bool trigger) {
  if (start_type == SERVICE_BOOT_START) return state == SERVICE_RUNNING;
  if (start_type == SERVICE_SYSTEM_START) return state == SERVICE_RUNNING;
  if (start_type == SERVICE_AUTO_START) return state == SERVICE_RUNNING;
  if (start_type == SERVICE_DEMAND_START) return true;
  if (start_type == SERVICE_DISABLED) return state == SERVICE_STOPPED;
  return false;
}

bool check_state_is_ok(DWORD state, DWORD start_type, bool delayed, bool trigger) {
  if ((state == SERVICE_START_PENDING) && (start_type == SERVICE_BOOT_START || start_type == SERVICE_SYSTEM_START || start_type == SERVICE_AUTO_START))
    return true;
  if (delayed) {
    if (start_type == SERVICE_BOOT_START || start_type == SERVICE_SYSTEM_START || start_type == SERVICE_AUTO_START) return true;
  }
  return check_state_is_perfect(state, start_type, trigger);
}

node_type state_is_ok(const value_type, evaluation_context context, const node_type subject) {
  native_context* n_context = reinterpret_cast<native_context*>(context.get());
  DWORD state = n_context->get_object()->state;
  DWORD start_type = n_context->get_object()->start_type;
  bool delayed = n_context->get_object()->get_delayed() == 1;
  bool trigger = n_context->get_object()->get_is_trigger() == 1;
  if (check_state_is_ok(state, start_type, delayed, trigger))
    return factory::create_true();
  else
    return factory::create_false();
}

node_type state_is_perfect(const value_type, evaluation_context context, const node_type subject) {
  native_context* n_context = reinterpret_cast<native_context*>(context.get());
  DWORD state = n_context->get_object()->state;
  DWORD start_type = n_context->get_object()->start_type;
  bool trigger = n_context->get_object()->get_is_trigger() == 1;
  if (check_state_is_perfect(state, start_type, trigger))
    return factory::create_true();
  else
    return factory::create_false();
}

parsers::where::node_type parse_state(boost::shared_ptr<filter_obj> object, parsers::where::evaluation_context context, parsers::where::node_type subject) {
  try {
    return parsers::where::factory::create_int(filter_obj::parse_state(subject->get_string_value(context)));
  } catch (const std::string& e) {
    context->error(e);
    return factory::create_false();
  }
}
parsers::where::node_type parse_start_type(boost::shared_ptr<filter_obj> object, parsers::where::evaluation_context context,
                                           parsers::where::node_type subject) {
  try {
    return parsers::where::factory::create_int(filter_obj::parse_start_type(subject->get_string_value(context)));
  } catch (const std::string& e) {
    context->error(e);
    return factory::create_false();
  }
}

filter_obj_handler::filter_obj_handler() {
  static const parsers::where::value_type type_custom_state = parsers::where::type_custom_int_1;
  static const parsers::where::value_type type_custom_start_type = parsers::where::type_custom_int_2;

  // clang-format off
  registry_.add_string()
    ("name", [] (auto obj, auto context) { return obj->get_name(); }, "Service name")
    ("desc", [] (auto obj, auto context) { return obj->get_desc(); }, "Service description")
    ("legacy_state", [] (auto obj, auto context) { return obj->get_legacy_state_s(); }, "Get legacy state (deprecated and only used by check_nt)")
    ("classification", [] (auto obj, auto context) { return obj->get_classification(); }, "Get classification")
    ;
  registry_.add_int()
    ("pid", [] (auto obj, auto context) { return obj->get_pid(); }, "Process id")
    ("state", type_custom_state, [] (auto obj, auto context) { return obj->get_state_i(); }, "The current state ()").add_perf("", "")
    ("start_type", type_custom_start_type, [] (auto obj, auto context) { return obj->get_start_type_i(); }, "The configured start type ()")
    ("delayed", parsers::where::type_bool, [] (auto obj, auto context) { return obj->get_delayed(); }, "If the service is delayed")
    ("is_trigger", parsers::where::type_bool, [] (auto obj, auto context) { return obj->get_is_trigger(); }, "If the service is has associated triggers")
    ("triggers", parsers::where::type_int, [] (auto obj, auto context) { return obj->get_triggers(); }, "The number of associated triggers for this service")
    ;

  registry_.add_int_fun()
    ("state_is_perfect", parsers::where::type_bool, &state_is_perfect, "Check if the state is ok, i.e. all running services are running")
    ("state_is_ok", parsers::where::type_bool, &state_is_ok, "Check if the state is ok, i.e. all running services are runningelayed services are allowed to be stopped)")
    ;

  registry_.add_human_string()
    ("state", [] (auto obj, auto context) { return obj->get_state_s(); }, "The current state ()")
    ("start_type", [] (auto obj, auto context) { return obj->get_start_type_s(); }, "The configured start type ()")
    ;

  registry_.add_converter()
    (type_custom_state, &parse_state)
    (type_custom_start_type, &parse_start_type)
    ;
  // clang-format on
}
}  // namespace check_svc_filter

namespace check_uptime_filter {
parsers::where::node_type parse_time(boost::shared_ptr<filter_obj> object, parsers::where::evaluation_context context, parsers::where::node_type subject) {
  parsers::where::helpers::read_arg_type value = parsers::where::helpers::read_arguments(context, subject, "d");
  std::string expr = str::xtos(value.get<0>()) + value.get<2>();
  return parsers::where::factory::create_int(str::format::stox_as_time_sec<long long>(expr, "s"));
}

static const parsers::where::value_type type_custom_uptime = parsers::where::type_custom_int_1;
filter_obj_handler::filter_obj_handler() {
  // clang-format off
  registry_.add_int()
    ("boot", parsers::where::type_date, [] (auto obj, auto context) { return obj->get_boot(); }, "System boot time")
    ("uptime", type_custom_uptime, [] (auto obj, auto context) { return obj->get_uptime(); }, "Time since last boot").add_perf("s", "", "")
    ;
  registry_.add_converter()
    (type_custom_uptime, &parse_time)
    ;
  registry_.add_human_string()
    ("boot", [] (auto obj, auto context) { return obj->get_boot_s(); }, "The system boot time")
    ("uptime", [] (auto obj, auto context) { return obj->get_uptime_s(); }, "Time sine last boot")
    ;
  // clang-format on
}
}  // namespace check_uptime_filter

namespace os_version_filter {
filter_obj_handler::filter_obj_handler() {
  // clang-format off
  registry_.add_int()
    ("major", [] (auto obj, auto context) { return obj->get_major(); }, "Major version number").add_perf("")
    ("version", [] (auto obj, auto context) { return obj->get_version_i(); }, [] (auto obj, auto context) { return obj->get_version_s(); }, "The system version").add_perf("")
    ("minor", [] (auto obj, auto context) { return obj->get_minor(); }, "Minor version number").add_perf("")
    ("build", [] (auto obj, auto context) { return obj->get_build(); }, "Build version number").add_perf("")
    ;
  registry_.add_string()
    ("suite", [] (auto obj, auto context) { return obj->get_suite_string(); }, "Which suites are installed on the machine (Microsoft BackOffice, Web Edition, Compute Cluster Edition, Datacenter Edition, Enterprise Edition, Embedded, Home Edition, Remote Desktop Support, Small Business Server, Storage Server, Terminal Services, Home Server)")
    ;
  // clang-format on
}
}  // namespace os_version_filter