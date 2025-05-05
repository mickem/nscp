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

#include <map>
#include <list>

#include <boost/bind/bind.hpp>
#include <boost/assign.hpp>

#include <parsers/where.hpp>
#include <parsers/where/helpers.hpp>

#include <simple_timer.hpp>
#include <str/utils.hpp>
#include "filter.hpp"

using namespace parsers::where;
namespace ph = boost::placeholders;

namespace check_cpu_filter {
filter_obj_handler::filter_obj_handler() {
  // clang-format off
  registry_.add_string()
    ("time", boost::bind(&filter_obj::get_time, ph::_1), "The time frame to check")
    ("core", boost::bind(&filter_obj::get_core_s, ph::_1), boost::bind(&filter_obj::get_core_i, ph::_1), "The core to check (total or core ##)")
    ("core_id", boost::bind(&filter_obj::get_core_id, ph::_1), boost::bind(&filter_obj::get_core_i, ph::_1), "The core to check (total or core_##)")
    ;
  registry_.add_int()
    ("load", boost::bind(&filter_obj::get_total, ph::_1), "The current load for a given core").add_perf("%")
    ("idle", boost::bind(&filter_obj::get_idle, ph::_1), "The current idle load for a given core")
    ("kernel", boost::bind(&filter_obj::get_kernel, ph::_1), "The current kernel load for a given core");
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
    ("type", boost::bind(&filter_obj::get_type, ph::_1), "The type of memory to check")
    ;
  registry_
      .add_int()
        ("size", boost::bind(&filter_obj::get_total, ph::_1), "Total size of memory")
        ("free", type_custom_free, boost::bind(&filter_obj::get_free, ph::_1), "Free memory in bytes (g,m,k,b) or percentages %")
          .add_scaled_byte(boost::bind(&get_zero), boost::bind(&filter_obj::get_total, ph::_1))
          .add_percentage(boost::bind(&filter_obj::get_total, ph::_1), "", " %")
        ("used", type_custom_used, boost::bind(&filter_obj::get_used, ph::_1), "Used memory in bytes (g,m,k,b) or percentages %")
          .add_scaled_byte(boost::bind(&get_zero), boost::bind(&filter_obj::get_total, ph::_1))
          .add_percentage(boost::bind(&filter_obj::get_total, ph::_1), "", " %")
      ;
  registry_.add_human_string()
    ("size", boost::bind(&filter_obj::get_total_human, ph::_1), "")
    ("free", boost::bind(&filter_obj::get_free_human, ph::_1), "")
    ("used", boost::bind(&filter_obj::get_used_human, ph::_1), "")
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
                        ("name", boost::bind(&filter_obj::get_name, ph::_1), "The name of the page file (location)")
                        ;
                registry_.add_int()
                        ("size", boost::bind(&filter_obj::get_total, ph::_1), "Total size of pagefile")
                        ("free", type_custom_free, boost::bind(&filter_obj::get_free, ph::_1), "Free memory in bytes (g,m,k,b) or percentages %")
                        .add_scaled_byte(boost::bind(&get_zero), boost::bind(&filter_obj::get_total, ph::_1))
                        .add_percentage(boost::bind(&filter_obj::get_total, ph::_1), "", " %")

                        ("used", type_custom_used, boost::bind(&filter_obj::get_used, ph::_1), "Used memory in bytes (g,m,k,b) or percentages %")
                        .add_scaled_byte(boost::bind(&get_zero), boost::bind(&filter_obj::get_total, ph::_1))
                        .add_percentage(boost::bind(&filter_obj::get_total, ph::_1), "", " %")
                        ;
                registry_.add_human_string()
                        ("size", boost::bind(&filter_obj::get_total_human, ph::_1), "")
                        ("free", boost::bind(&filter_obj::get_free_human, ph::_1), "")
                        ("used", boost::bind(&filter_obj::get_used_human, ph::_1), "")
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
                        ("name", boost::bind(&filter_obj::get_name, ph::_1), "Service name")
                        ("desc", boost::bind(&filter_obj::get_desc, ph::_1), "Service description")
                        ("legacy_state", boost::bind(&filter_obj::get_legacy_state_s, ph::_1), "Get legacy state (deprecated and only used by check_nt)")
                        ;
                registry_.add_int()
                        ("pid", boost::bind(&filter_obj::get_pid, ph::_1), "Process id")
                        ("state", type_custom_state, boost::bind(&filter_obj::get_state_i, ph::_1), boost::bind(&filter_obj::get_state_s, ph::_1), "The current
state
()").add_perf("","")
                        ("start_type", type_custom_start_type, boost::bind(&filter_obj::get_start_type_i, ph::_1),boost::bind(&filter_obj::get_start_type_s,
ph::_1), "The configured start type ()")
                        ("delayed", parsers::where::type_bool, boost::bind(&filter_obj::get_delayed, ph::_1),  "If the service is delayed")
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
  registry_.add_int()("boot", parsers::where::type_date, boost::bind(&filter_obj::get_boot, ph::_1), "System boot time")(
      "uptime", type_custom_uptime, boost::bind(&filter_obj::get_uptime, ph::_1), "Time since last boot");
  registry_.add_converter()(type_custom_uptime, &parse_time);
  registry_.add_human_string()("boot", boost::bind(&filter_obj::get_boot_s, ph::_1), "The system boot time")(
      "uptime", boost::bind(&filter_obj::get_uptime_s, ph::_1), "Time sine last boot");
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
                        ("filename", boost::bind(&filter_obj::get_filename, ph::_1), "Name of process (with path)")
                        ("exe", boost::bind(&filter_obj::get_exe, ph::_1), "The name of the executable")
                        ("command_line", boost::bind(&filter_obj::get_command_line, ph::_1), "Command line of process (not always available)")
                        ("legacy_state", boost::bind(&filter_obj::get_legacy_state_s, ph::_1), "Get process status (for legacy use via check_nt only)")
                        ;
                registry_.add_int()
                        ("pid", boost::bind(&filter_obj::get_pid, ph::_1), "Process id")
                        ("started", parsers::where::type_bool, boost::bind(&filter_obj::get_started, ph::_1), "Process is started")
                        ("hung", parsers::where::type_bool, boost::bind(&filter_obj::get_hung, ph::_1), "Process is hung")
                        ("stopped", parsers::where::type_bool, boost::bind(&filter_obj::get_stopped, ph::_1), "Process is stopped")
                        ;
                registry_.add_int()
                        ("handles", boost::bind(&filter_obj::get_handleCount, ph::_1), "Number of handles").add_perf("", "", " handle count")
                        ("gdi_handles", boost::bind(&filter_obj::get_gdiHandleCount, ph::_1), "Number of handles").add_perf("", "", " GDI handle count")
                        ("user_handles", boost::bind(&filter_obj::get_userHandleCount, ph::_1), "Number of handles").add_perf("", "", " USER handle count")
                        ("peak_virtual", parsers::where::type_size, boost::bind(&filter_obj::get_PeakVirtualSize, ph::_1), "Peak virtual size in
bytes").add_scaled_byte(std::string(""), " pv_size")
                        ("virtual", parsers::where::type_size, boost::bind(&filter_obj::get_VirtualSize, ph::_1), "Virtual size in
bytes").add_scaled_byte(std::string(""), " v_size")
                        ("page_fault", boost::bind(&filter_obj::get_PageFaultCount, ph::_1), "Page fault count").add_perf("", "", " pf_count")
                        ("peak_working_set", parsers::where::type_size, boost::bind(&filter_obj::get_PeakWorkingSetSize, ph::_1), "Peak working set in
bytes").add_scaled_byte(std::string(""), " pws_size")
                        ("working_set", parsers::where::type_size, boost::bind(&filter_obj::get_WorkingSetSize, ph::_1), "Working set in
bytes").add_scaled_byte(std::string(""), " ws_size")
// 			("qouta", parsers::where::type_size, boost::bind(&filter_obj::get_QuotaPeakPagedPoolUsage, ph::_1),
"TODO").add_scaled_byte(std::string(""), " v_size")
// 			("virtual_size", parsers::where::type_size, boost::bind(&filter_obj::get_QuotaPagedPoolUsage, ph::_1),
"TODO").add_scaled_byte(std::string(""), " v_size")
// 			("virtual_size", parsers::where::type_size, boost::bind(&filter_obj::get_QuotaPeakNonPagedPoolUsage, ph::_1),
"TODO").add_scaled_byte(std::string(""), " v_size")
// 			("virtual_size", parsers::where::type_size, boost::bind(&filter_obj::get_QuotaNonPagedPoolUsage, ph::_1),
"TODO").add_scaled_byte(std::string(""), " v_size")
                        ("peak_pagefile", parsers::where::type_size, boost::bind(&filter_obj::get_PagefileUsage, ph::_1), "Page file usage in
bytes").add_scaled_byte(std::string(""), " ppf_use")
                        ("pagefile", parsers::where::type_size, boost::bind(&filter_obj::get_PeakPagefileUsage, ph::_1), "Peak page file use in
bytes").add_scaled_byte(std::string(""), " pf_use")

                        ("creation", parsers::where::type_date, boost::bind(&filter_obj::get_creation_time, ph::_1), "Creation time").add_perf("", "", "
creation")
                        ("kernel", boost::bind(&filter_obj::get_kernel_time, ph::_1), "Kernel time in seconds").add_perf("", "", " kernel")
                        ("user", boost::bind(&filter_obj::get_user_time, ph::_1), "User time in seconds").add_perf("", "", " user")

                        ("state", type_custom_state, boost::bind(&filter_obj::get_state_i, ph::_1), "The current state (started, stopped hung)").add_perf("",
""," state")
                        ;

                registry_.add_human_string()
                        ("state", boost::bind(&filter_obj::get_state_s, ph::_1), "The current state (started, stopped hung)")
                        ;

                registry_.add_converter()
                        (type_custom_state, &parse_state)
                        ;


        }
}
*/

namespace os_version_filter {
filter_obj_handler::filter_obj_handler() {
  registry_.add_string()("kernel_name", boost::bind(&filter_obj::get_kernel_name, ph::_1), "Kernel name")(
      "nodename", boost::bind(&filter_obj::get_nodename, ph::_1), "Network node hostname")(
      "kernel_release", boost::bind(&filter_obj::get_kernel_release, ph::_1), "Kernel release")(
      "kernel_version", boost::bind(&filter_obj::get_kernel_version, ph::_1), "Kernel version")("machine", boost::bind(&filter_obj::get_machine, ph::_1),
                                                                                                "Machine hardware name")(
      "processor", boost::bind(&filter_obj::get_processor, ph::_1), "Processor type or unknown")("os", boost::bind(&filter_obj::get_processor, ph::_1),
                                                                                                 "Operating system");
}
}  // namespace os_version_filter
