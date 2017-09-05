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

#include <boost/bind.hpp>
#include <boost/assign.hpp>

#include <parsers/where.hpp>
#include <parsers/where/helpers.hpp>

#include <simple_timer.hpp>
#include <str/utils.hpp>
#include "filter.hpp"

using namespace parsers::where;

namespace check_cpu_filter {
	parsers::where::node_type calculate_load(boost::shared_ptr<filter_obj> object, parsers::where::evaluation_context context, parsers::where::node_type subject) {
		parsers::where::helpers::read_arg_type value = parsers::where::helpers::read_arguments(context, subject, "%");
		double number = value.get<1>();
		std::string unit = value.get<2>();

		if (unit != "%")
			context->error("Invalid unit: " + unit);
		return parsers::where::factory::create_int(number);
	}

	filter_obj_handler::filter_obj_handler() {
		static const parsers::where::value_type type_custom_pct = parsers::where::type_custom_int_1;

		registry_.add_string()
			("time", boost::bind(&filter_obj::get_time, _1), "The time frame to check")
			("core", boost::bind(&filter_obj::get_core_s, _1), boost::bind(&filter_obj::get_core_i, _1), "The core to check (total or core ##)")
			("core_id", boost::bind(&filter_obj::get_core_id, _1), boost::bind(&filter_obj::get_core_i, _1), "The core to check (total or core_##)")
			;
		registry_.add_int()
			("load", type_custom_pct, boost::bind(&filter_obj::get_total, _1), "The current load for a given core").add_perf("%")
			("idle", boost::bind(&filter_obj::get_idle, _1), "The current idle load for a given core")
			("kernel", boost::bind(&filter_obj::get_kernel, _1), "The current kernel load for a given core")
			;

		registry_.add_converter()
			(type_custom_pct, &calculate_load)
			;
	}
}


namespace check_page_filter {
	parsers::where::node_type calculate_free(boost::shared_ptr<filter_obj> object, parsers::where::evaluation_context context, parsers::where::node_type subject) {
		parsers::where::helpers::read_arg_type value = parsers::where::helpers::read_arguments(context, subject, "%");
		double number = value.get<1>();
		std::string unit = value.get<2>();

		if (unit == "%") {
			number = (static_cast<double>(object->get_total())*number) / 100.0;
		} else {
			number = str::format::decode_byte_units(number, unit);
		}
		return parsers::where::factory::create_int(number);
	}

	long long get_zero() {
		return 0;
	}

	filter_obj_handler::filter_obj_handler() {
		static const parsers::where::value_type type_custom_used = parsers::where::type_custom_int_1;
		static const parsers::where::value_type type_custom_free = parsers::where::type_custom_int_2;

		registry_.add_string()
			("name", boost::bind(&filter_obj::get_name, _1), "The name of the page file (location)")
			;
		registry_.add_int()
			("size", boost::bind(&filter_obj::get_total, _1), "Total size of pagefile")
			("free", type_custom_free, boost::bind(&filter_obj::get_free, _1), "Free memory in bytes (g,m,k,b) or percentages %")
			.add_scaled_byte(boost::bind(&get_zero), boost::bind(&filter_obj::get_total, _1))
			.add_percentage(boost::bind(&filter_obj::get_total, _1), "", " %")

			("used", type_custom_used, boost::bind(&filter_obj::get_used, _1), "Used memory in bytes (g,m,k,b) or percentages %")
			.add_scaled_byte(boost::bind(&get_zero), boost::bind(&filter_obj::get_total, _1))
			.add_percentage(boost::bind(&filter_obj::get_total, _1), "", " %")
			("free_pct", boost::bind(&filter_obj::get_free_pct, _1), "% free memory")
			("used_pct", boost::bind(&filter_obj::get_used_pct, _1), "% used memory")
			;
		registry_.add_human_string()
			("size", boost::bind(&filter_obj::get_total_human, _1), "")
			("free", boost::bind(&filter_obj::get_free_human, _1), "")
			("used", boost::bind(&filter_obj::get_used_human, _1), "")
			;

		registry_.add_converter()
			(type_custom_free, &calculate_free)
			(type_custom_used, &calculate_free)
			;
	}
}

namespace check_svc_filter {
	bool check_state_is_perfect(DWORD state, DWORD start_type, bool trigger) {
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

	bool check_state_is_ok(DWORD state, DWORD start_type, bool delayed, bool trigger) {
		if (
			(state == SERVICE_START_PENDING) &&
			(start_type == SERVICE_BOOT_START || start_type == SERVICE_SYSTEM_START || start_type == SERVICE_AUTO_START)
			)
			return true;
		if (delayed) {
			if (start_type == SERVICE_BOOT_START || start_type == SERVICE_SYSTEM_START || start_type == SERVICE_AUTO_START)
				return true;
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
		} catch (const std::string &e) {
			context->error(e);
			return factory::create_false();
		}
	}
	parsers::where::node_type parse_start_type(boost::shared_ptr<filter_obj> object, parsers::where::evaluation_context context, parsers::where::node_type subject) {
		try {
			return parsers::where::factory::create_int(filter_obj::parse_start_type(subject->get_string_value(context)));
		} catch (const std::string &e) {
			context->error(e);
			return factory::create_false();
		}
	}

	filter_obj_handler::filter_obj_handler() {
		static const parsers::where::value_type type_custom_state = parsers::where::type_custom_int_1;
		static const parsers::where::value_type type_custom_start_type = parsers::where::type_custom_int_2;

		registry_.add_string()
			("name", boost::bind(&filter_obj::get_name, _1), "Service name")
			("desc", boost::bind(&filter_obj::get_desc, _1), "Service description")
			("legacy_state", boost::bind(&filter_obj::get_legacy_state_s, _1), "Get legacy state (deprecated and only used by check_nt)")
			("classification", boost::bind(&filter_obj::get_classification, _1), "Get classification")
			;
		registry_.add_int()
			("pid", boost::bind(&filter_obj::get_pid, _1), "Process id")
			("state", type_custom_state, boost::bind(&filter_obj::get_state_i, _1), "The current state ()").add_perf("", "")
			("start_type", type_custom_start_type, boost::bind(&filter_obj::get_start_type_i, _1), "The configured start type ()")
			("delayed", parsers::where::type_bool, boost::bind(&filter_obj::get_delayed, _1), "If the service is delayed")
			("is_trigger", parsers::where::type_bool, boost::bind(&filter_obj::get_is_trigger, _1), "If the service is has associated triggers")
			("triggers", parsers::where::type_int, boost::bind(&filter_obj::get_triggers, _1), "The number of associated triggers for this service")
			;

		registry_.add_int_fun()
			("state_is_perfect", parsers::where::type_bool, &state_is_perfect, "Check if the state is ok, i.e. all running services are running")
			("state_is_ok", parsers::where::type_bool, &state_is_ok, "Check if the state is ok, i.e. all running services are runningelayed services are allowed to be stopped)")
			;

		registry_.add_human_string()
			("state", boost::bind(&filter_obj::get_state_s, _1), "The current state ()")
			("start_type", boost::bind(&filter_obj::get_start_type_s, _1), "The configured start type ()")
			;

		registry_.add_converter()
			(type_custom_state, &parse_state)
			(type_custom_start_type, &parse_start_type)
			;
	}
}

namespace check_uptime_filter {
	parsers::where::node_type parse_time(boost::shared_ptr<filter_obj> object, parsers::where::evaluation_context context, parsers::where::node_type subject) {
		parsers::where::helpers::read_arg_type value = parsers::where::helpers::read_arguments(context, subject, "d");
		std::string expr = str::xtos(value.get<0>()) + value.get<2>();
		return parsers::where::factory::create_int(str::format::stox_as_time_sec<long long>(expr, "s"));
	}

	static const parsers::where::value_type type_custom_uptime = parsers::where::type_custom_int_1;
	filter_obj_handler::filter_obj_handler() {
		registry_.add_int()
			("boot", parsers::where::type_date, boost::bind(&filter_obj::get_boot, _1), "System boot time")
			("uptime", type_custom_uptime, boost::bind(&filter_obj::get_uptime, _1), "Time since last boot").add_perf("s", "", "")
			;
		registry_.add_converter()
			(type_custom_uptime, &parse_time)
			;
		registry_.add_human_string()
			("boot", boost::bind(&filter_obj::get_boot_s, _1), "The system boot time")
			("uptime", boost::bind(&filter_obj::get_uptime_s, _1), "Time sine last boot")
			;
	}
}

namespace os_version_filter {
	filter_obj_handler::filter_obj_handler() {
		registry_.add_int()
			("major", boost::bind(&filter_obj::get_major, _1), "Major version number").add_perf("")
			("version", boost::bind(&filter_obj::get_version_i, _1), boost::bind(&filter_obj::get_version_s, _1), "The system version").add_perf("")
			("minor", boost::bind(&filter_obj::get_minor, _1), "Minor version number").add_perf("")
			("build", boost::bind(&filter_obj::get_build, _1), "Build version number").add_perf("")
			;
 		registry_.add_string()
 			("suite", boost::bind(&filter_obj::get_suite_string, _1), "Which suites are installed on the machine (Microsoft BackOffice, Web Edition, Compute Cluster Edition, Datacenter Edition, Enterprise Edition, Embedded, Home Edition, Remote Desktop Support, Small Business Server, Storage Server, Terminal Services, Home Server)")
 			;
	}
}