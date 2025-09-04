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
#include <list>
#include <map>
#include <parsers/where.hpp>
#include <parsers/where/helpers.hpp>
#include <simple_timer.hpp>
#include <str/utils.hpp>

using namespace parsers::where;

namespace check_cpu_filter {
filter_obj_handler::filter_obj_handler() {
  // clang-format off
  registry_.add_string()
    ("time", [] (auto obj, auto context) { return obj->get_time(); }, "The time frame to check")
    ("core", [] (auto obj, auto context) { return obj->get_core_s(); }, [] (auto obj, auto context) { return obj->get_core_i(); }, "The core to check (total or core ##)")
    ("core_id", [] (auto obj, auto context) { return obj->get_core_id(); }, [] (auto obj, auto context) { return obj->get_core_i(); }, "The core to check (total or core_##)")
    ;
  registry_.add_int()
    ("load", [] (auto obj, auto context) { return obj->get_total(); }, "The current load for a given core").add_perf("%")
    ("idle", [] (auto obj, auto context) { return obj->get_idle(); }, "The current idle load for a given core")
    ("kernel", [] (auto obj, auto context) { return obj->get_kernel(); }, "The current kernel load for a given core");
  // clang-format on
}
}  // namespace check_cpu_filter

namespace check_mem_filter {

parsers::where::node_type calculate_free(boost::shared_ptr<filter_obj> object, parsers::where::evaluation_context context, parsers::where::node_type subject) {
  parsers::where::helpers::read_arg_type value = parsers::where::helpers::read_arguments(context, subject, "%");
  long long number = value.get<1>();
  std::string unit = value.get<2>();

  if (unit == "%") {
    number = (object->get_total() * (number)) / 100;
  } else {
    number = str::format::decode_byte_units(number, unit);
  }
  return parsers::where::factory::create_int(number);
}

long long get_zero() { return 0; }

filter_obj_handler::filter_obj_handler() {
  static const parsers::where::value_type type_custom_used = parsers::where::type_custom_int_1;
  static const parsers::where::value_type type_custom_free = parsers::where::type_custom_int_2;

  // clang-format off
  registry_.add_string()
    ("type", [] (auto obj, auto context) { return obj->get_type(); }, "The type of memory to check")
    ;
  registry_
      .add_int()
        ("size", [] (auto obj, auto context) { return obj->get_total(); }, "Total size of memory")
        ("free", type_custom_free, [] (auto obj, auto context) { return obj->get_free(); }, "Free memory in bytes (g,m,k,b) or percentages %")
          .add_scaled_byte([] (auto obj, auto context) { return get_zero(); }, [] (auto obj, auto context) { return obj->get_total(); })
          .add_percentage([] (auto obj, auto context) { return obj->get_total(); }, "", " %")
        ("used", type_custom_used, [] (auto obj, auto context) { return obj->get_used(); }, "Used memory in bytes (g,m,k,b) or percentages %")
          .add_scaled_byte([] (auto obj, auto context) { return get_zero(); }, [] (auto obj, auto context) { return obj->get_total(); })
          .add_percentage([] (auto obj, auto context) { return obj->get_total(); }, "", " %")
      ;
  registry_.add_human_string()
    ("size", [] (auto obj, auto context) { return obj->get_total_human(); }, "")
    ("free", [] (auto obj, auto context) { return obj->get_free_human(); }, "")
    ("used", [] (auto obj, auto context) { return obj->get_used_human(); }, "")
    ;
  // clang-format on

  registry_.add_converter()(type_custom_free, &calculate_free)(type_custom_used, &calculate_free);
}
}  // namespace check_mem_filter
/*
namespace check_page_filter {

        parsers::where::node_type calculate_free(boost::shared_ptr<filter_obj> object, parsers::where::evaluation_context context, parsers::where::node_type
subject) { boost::tuple<long long, std::string> value = parsers::where::helpers::read_arguments(context, subject, "%"); long long number = value.get<0>();
                std::string unit = value.get<1>();

                if (unit == "%") {
                        number = (object->get_total()*(number))/100;
                } else {
                        number = format::decode_byte_units(number, unit);
                }
                return parsers::where::factory::create_int(number);
        }

        long long get_zero() {
                return 0;
        }

        filter_obj_handler::filter_obj_handler() {
                static const parsers::where::value_type type_custom_used = parsers::where::type_custom_intph::_1;
                static const parsers::where::value_type type_custom_free = parsers::where::type_custom_int_2;

                registry_.add_string()
                        ("name", [] (auto obj, auto context) { return obj->get_name(); }, "The name of the page file (location)")
                        ;
                registry_.add_int()
                        ("size", [] (auto obj, auto context) { return obj->get_total(); }, "Total size of pagefile")
                        ("free", type_custom_free, [] (auto obj, auto context) { return obj->get_free(); }, "Free memory in bytes (g,m,k,b) or percentages %")
                        .add_scaled_byte( [](auto obj, auto context) { return obj->get_zero), [] (auto obj, auto context) { return obj->get_total(); })
                        .add_percentage([] (auto obj, auto context) { return obj->get_total(); }, "", " %")

                        ("used", type_custom_used, [] (auto obj, auto context) { return obj->get_used(); }, "Used memory in bytes (g,m,k,b) or percentages %")
                        .add_scaled_byte( [](auto obj, auto context) { return obj->get_zero), [] (auto obj, auto context) { return obj->get_total(); })
                        .add_percentage([] (auto obj, auto context) { return obj->get_total(); }, "", " %")
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

        }
}


namespace check_svc_filter {

        bool check_state_is_perfect(DWORD state, DWORD start_type) {
                if (start_type == SERVICE_BOOT_START)
                        return state == SERVICE_RUNNING;
                if (start_type == SERVICE_SYSTEM_START)
                        return state == SERVICE_RUNNING;
                if (start_type == SERVICE_AUTO_START)
                        return state == SERVICE_RUNNING;
                if (start_type == SERVICE_DEMAND_START)
                        return true;
                if (start_type == SERVICE_DISABLED)
                        return state == SERVICE_STOPPED;
                return false;
        }

        bool check_state_is_ok(DWORD state, DWORD start_type, bool delayed) {
                if (
                        (state == SERVICE_START_PENDING) &&
                        (start_type == SERVICE_BOOT_START || start_type == SERVICE_SYSTEM_START || start_type == SERVICE_AUTO_START)
                        )
                        return true;
                if (delayed) {
                        if (start_type == SERVICE_BOOT_START || start_type == SERVICE_SYSTEM_START || start_type == SERVICE_AUTO_START)
                                return true;
                }
                return check_state_is_perfect(state, start_type);
        }

        node_type state_is_ok(const value_type target_type, evaluation_context context, const node_type subject) {
                native_context* n_context = reinterpret_cast<native_context*>(context.get());
                DWORD state = n_context->get_object()->state;
                DWORD start_type = n_context->get_object()->start_type;
                bool delayed = n_context->get_object()->delayed;
                if (check_state_is_ok(state, start_type, delayed))
                        return factory::create_true();
                else
                        return factory::create_false();
        }

        node_type state_is_perfect(const value_type target_type, evaluation_context context, const node_type subject) {
                native_context* n_context = reinterpret_cast<native_context*>(context.get());
                DWORD state = n_context->get_object()->state;
                DWORD start_type = n_context->get_object()->start_type;
                if (check_state_is_perfect(state, start_type))
                        return factory::create_true();
                else
                        return factory::create_false();
        }

        parsers::where::node_type parse_state(boost::shared_ptr<filter_obj> object, parsers::where::evaluation_context context, parsers::where::node_type
subject) { return parsers::where::factory::create_int(filter_obj::parse_state(subject->get_string_value(context)));
        }
        parsers::where::node_type parse_start_type(boost::shared_ptr<filter_obj> object, parsers::where::evaluation_context context, parsers::where::node_type
subject) { return parsers::where::factory::create_int(filter_obj::parse_start_type(subject->get_string_value(context)));
        }


        filter_obj_handler::filter_obj_handler() {
                static const parsers::where::value_type type_custom_state = parsers::where::type_custom_intph::_1;
                static const parsers::where::value_type type_custom_start_type = parsers::where::type_custom_int_2;

                registry_.add_string()
                        ("name", [] (auto obj, auto context) { return obj->get_name(); }, "Service name")
                        ("desc", [] (auto obj, auto context) { return obj->get_desc(); }, "Service description")
                        ("legacy_state", [] (auto obj, auto context) { return obj->get_legacy_state_s(); }, "Get legacy state (deprecated and only used by
check_nt)")
                        ;
                registry_.add_int()
                        ("pid", [] (auto obj, auto context) { return obj->get_pid(); }, "Process id")
                        ("state", type_custom_state, [] (auto obj, auto context) { return obj->get_state_i(); }, [] (auto obj, auto context) { return
obj->get_state_s(); }, "The current state
()").add_perf("","")
                        ("start_type", type_custom_start_type, [] (auto obj, auto context) { return obj->get_start_type_i(); },[] (auto obj, auto context) {
return obj->get_start_type_s, ph::_1), "The configured start type ()")
                        ("delayed", parsers::where::type_bool, [] (auto obj, auto context) { return obj->get_delayed(); },  "If the service is delayed")
                        ;

                registry_.add_int_fun()
                        ("state_is_perfect",  parsers::where::type_bool, &state_is_perfect, "Check if the state is ok, i.e. all running services are running")
                        ("state_is_ok",  parsers::where::type_bool, &state_is_ok, "Check if the state is ok, i.e. all running services are runningelayed
services are allowed to be stopped)")
                        ;

                registry_.add_converter()
                        (type_custom_state, &parse_state)
                        (type_custom_start_type, &parse_start_type)
                        ;
        }
}

*/

namespace check_uptime_filter {

parsers::where::node_type parse_time(boost::shared_ptr<filter_obj> object, parsers::where::evaluation_context context, parsers::where::node_type subject) {
  return parsers::where::factory::create_int(str::format::stox_as_time_sec<long long>(subject->get_string_value(context), "s"));
}

static const parsers::where::value_type type_custom_uptime = parsers::where::type_custom_int_1;
filter_obj_handler::filter_obj_handler() {
  registry_.add_int()(
      "boot", parsers::where::type_date, [](auto obj, auto context) { return obj->get_boot(); }, "System boot time")(
      "uptime", type_custom_uptime, [](auto obj, auto context) { return obj->get_uptime(); }, "Time since last boot");
  registry_.add_converter()(type_custom_uptime, &parse_time);
  registry_.add_human_string()(
      "boot", [](auto obj, auto context) { return obj->get_boot_s(); }, "The system boot time")(
      "uptime", [](auto obj, auto context) { return obj->get_uptime_s(); }, "Time sine last boot");
}
}  // namespace check_uptime_filter

/*


namespace check_proc_filter {


        parsers::where::node_type parse_state(boost::shared_ptr<filter_obj> object, parsers::where::evaluation_context context, parsers::where::node_type
subject) { return parsers::where::factory::create_int(filter_obj::parse_state(subject->get_string_value(context)));
        }

        filter_obj_handler::filter_obj_handler() {
                static const parsers::where::value_type type_custom_state = parsers::where::type_custom_intph::_1;
                static const parsers::where::value_type type_custom_start_type = parsers::where::type_custom_int_2;

                registry_.add_string()
                        ("filename", [] (auto obj, auto context) { return obj->get_filename(); }, "Name of process (with path)")
                        ("exe", [] (auto obj, auto context) { return obj->get_exe(); }, "The name of the executable")
                        ("command_line", [] (auto obj, auto context) { return obj->get_command_line(); }, "Command line of process (not always available)")
                        ("legacy_state", [] (auto obj, auto context) { return obj->get_legacy_state_s(); }, "Get process status (for legacy use via check_nt
only)")
                        ;
                registry_.add_int()
                        ("pid", [] (auto obj, auto context) { return obj->get_pid(); }, "Process id")
                        ("started", parsers::where::type_bool, [] (auto obj, auto context) { return obj->get_started(); }, "Process is started")
                        ("hung", parsers::where::type_bool, [] (auto obj, auto context) { return obj->get_hung(); }, "Process is hung")
                        ("stopped", parsers::where::type_bool, [] (auto obj, auto context) { return obj->get_stopped(); }, "Process is stopped")
                        ;
                registry_.add_int()
                        ("handles", [] (auto obj, auto context) { return obj->get_handleCount(); }, "Number of handles").add_perf("", "", " handle count")
                        ("gdi_handles", [] (auto obj, auto context) { return obj->get_gdiHandleCount(); }, "Number of handles").add_perf("", "", " GDI handle
count")
                        ("user_handles", [] (auto obj, auto context) { return obj->get_userHandleCount(); }, "Number of handles").add_perf("", "", " USER handle
count")
                        ("peak_virtual", parsers::where::type_size, [] (auto obj, auto context) { return obj->get_PeakVirtualSize(); }, "Peak virtual size in
bytes").add_scaled_byte(std::string(""), " pv_size")
                        ("virtual", parsers::where::type_size, [] (auto obj, auto context) { return obj->get_VirtualSize(); }, "Virtual size in
bytes").add_scaled_byte(std::string(""), " v_size")
                        ("page_fault", [] (auto obj, auto context) { return obj->get_PageFaultCount(); }, "Page fault count").add_perf("", "", " pf_count")
                        ("peak_working_set", parsers::where::type_size, [] (auto obj, auto context) { return obj->get_PeakWorkingSetSize(); }, "Peak working set
in bytes").add_scaled_byte(std::string(""), " pws_size")
                        ("working_set", parsers::where::type_size, [] (auto obj, auto context) { return obj->get_WorkingSetSize(); }, "Working set in
bytes").add_scaled_byte(std::string(""), " ws_size")
// 			("qouta", parsers::where::type_size, [] (auto obj, auto context) { return obj->get_QuotaPeakPagedPoolUsage(); },
"TODO").add_scaled_byte(std::string(""), " v_size")
// 			("virtual_size", parsers::where::type_size, [] (auto obj, auto context) { return obj->get_QuotaPagedPoolUsage(); },
"TODO").add_scaled_byte(std::string(""), " v_size")
// 			("virtual_size", parsers::where::type_size, [] (auto obj, auto context) { return obj->get_QuotaPeakNonPagedPoolUsage(); },
"TODO").add_scaled_byte(std::string(""), " v_size")
// 			("virtual_size", parsers::where::type_size, [] (auto obj, auto context) { return obj->get_QuotaNonPagedPoolUsage(); },
"TODO").add_scaled_byte(std::string(""), " v_size")
                        ("peak_pagefile", parsers::where::type_size, [] (auto obj, auto context) { return obj->get_PagefileUsage(); }, "Page file usage in
bytes").add_scaled_byte(std::string(""), " ppf_use")
                        ("pagefile", parsers::where::type_size, [] (auto obj, auto context) { return obj->get_PeakPagefileUsage(); }, "Peak page file use in
bytes").add_scaled_byte(std::string(""), " pf_use")

                        ("creation", parsers::where::type_date, [] (auto obj, auto context) { return obj->get_creation_time(); }, "Creation time").add_perf("",
"", " creation")
                        ("kernel", [] (auto obj, auto context) { return obj->get_kernel_time(); }, "Kernel time in seconds").add_perf("", "", " kernel")
                        ("user", [] (auto obj, auto context) { return obj->get_user_time(); }, "User time in seconds").add_perf("", "", " user")

                        ("state", type_custom_state, [] (auto obj, auto context) { return obj->get_state_i(); }, "The current state (started, stopped
hung)").add_perf("",
""," state")
                        ;

                registry_.add_human_string()
                        ("state", [] (auto obj, auto context) { return obj->get_state_s(); }, "The current state (started, stopped hung)")
                        ;

                registry_.add_converter()
                        (type_custom_state, &parse_state)
                        ;


        }
}
*/

namespace os_version_filter {
filter_obj_handler::filter_obj_handler() {
  registry_.add_string()(
      "kernel_name", [](auto obj, auto context) { return obj->get_kernel_name(); }, "Kernel name")(
      "nodename", [](auto obj, auto context) { return obj->get_nodename(); }, "Network node hostname")(
      "kernel_release", [](auto obj, auto context) { return obj->get_kernel_release(); }, "Kernel release")(
      "kernel_version", [](auto obj, auto context) { return obj->get_kernel_version(); }, "Kernel version")(
      "machine", [](auto obj, auto context) { return obj->get_machine(); }, "Machine hardware name")(
      "processor", [](auto obj, auto context) { return obj->get_processor(); }, "Processor type or unknown")(
      "os", [](auto obj, auto context) { return obj->get_processor(); }, "Operating system");
}
}  // namespace os_version_filter
