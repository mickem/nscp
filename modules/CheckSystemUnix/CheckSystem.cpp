/**************************************************************************
*   Copyright (C) 2004-2007 by Michael Medin <michael@medin.name>         *
*                                                                         *
*   This code is part of NSClient++ - http://trac.nakednuns.org/nscp      *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should  have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/
#include "module.hpp"
#include "CheckSystem.h"

#include <map>
#include <set>
#include <iostream>
#include <fstream>

#include <sys/types.h>
#include <sys/utsname.h>

#include <boost/program_options.hpp>
#include <boost/shared_ptr.hpp>
 
#include <utils.h>
#include <settings/client/settings_client.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <parsers/filter/cli_helper.hpp>

#include "filter.hpp"

namespace sh = nscapi::settings_helper;
namespace po = boost::program_options;

#define UPTIME_FILE  "/proc/uptime"
#define MEMINFO_FILE  "/proc/meminfo"



/**
 * New version of the load call.
 * Start the background collector thread and let it run until unloadModule() is called.
 * @return true
 */
bool CheckSystem::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {

	sh::settings_registry settings(get_settings_proxy());
	settings.set_alias("system", alias, "unix");
	std::string counter_path = settings.alias().get_settings_path("counters");

//	collector.filters_path_ = settings.alias().get_settings_path("real-time/checks");

	//filters::filter_config_handler::add_samples(get_settings_proxy(), collector.filters_path_);
	
	return true;
}


/**
 * Unload (terminate) module.
 * Attempt to stop the background processing thread.
 * @return true if successfully, false if not (if not things might be bad)
 */
bool CheckSystem::unloadModule() {
	return true;
}

int CheckSystem::commandLineExec(const std::string &command, const std::list<std::string> &arguments, std::string &result) {
	return 0;
}

void CheckSystem::check_cpu(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	/*
	typedef check_cpu_filter::filter filter_type;
	modern_filter::data_container data;
	modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
	std::vector<std::string> times;

	filter_type filter;
	filter_helper.add_options(filter.get_filter_syntax(), "CPU Load ok");
	filter_helper.add_syntax("${problem_list}", filter.get_format_syntax(), "${time}: ${load}%", "${core} ${time}");
	filter_helper.get_desc().add_options()
		("time", po::value<std::vector<std::string> >(&times), "The time to check")
		;

	if (!filter_helper.parse_options())
		return;

	filter_helper.set_default("load > 80", "load > 90");
	filter_helper.set_default_filter("core = 'total'");

	if (times.empty()) {
		times.push_back("5m");
		times.push_back("1m");
		times.push_back("5s");
	}

	if (!filter_helper.build_filter(filter))
		return;

	BOOST_FOREACH(const std::string &time, times) {
		std::map<std::string,windows::system_info::load_entry> vals = collector.get_cpu_load(format::decode_time<long>(time, 1));
		typedef std::map<std::string,windows::system_info::load_entry>::value_type vt;
		BOOST_FOREACH(vt v, vals) {
			boost::shared_ptr<check_cpu_filter::filter_obj> record(new check_cpu_filter::filter_obj(time, v.first, v.second));
			boost::tuple<bool,bool> ret = filter.match(record);
		}
	}
	modern_filter::perf_writer writer(response);
	filter_helper.post_process(filter, &writer);
	*/
}



bool get_uptime(double &uptime_secs, double &idle_secs) {
	try {
		std::locale mylocale("C");
		std::ifstream uptime_file;
		uptime_file.imbue(mylocale);
		uptime_file.open(UPTIME_FILE);
		uptime_file >> uptime_secs >> idle_secs;
		uptime_file.close();
	} catch (const std::exception &e) {
		return false;
	}
    return true;
}

void CheckSystem::check_uptime(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	typedef check_uptime_filter::filter filter_type;
	modern_filter::data_container data;
	modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
	std::vector<std::string> times;

	filter_type filter;
	filter_helper.add_options(filter.get_filter_syntax(), "Uptime ok");
	filter_helper.add_syntax("${problem_list}", filter.get_format_syntax(), "uptime: ${uptime}h, boot: ${boot} (UTC)", "uptime");

	if (!filter_helper.parse_options())
		return;

	if (filter_helper.empty()) {
		filter_helper.set_default("uptime < 2d", "uptime < 1d");
	}

	if (!filter_helper.build_filter(filter))
		return;

	double uptime_secs = 0, idle_secs = 0;
	get_uptime(uptime_secs, idle_secs);
	unsigned long long value = uptime_secs;

	boost::posix_time::ptime now = boost::posix_time::second_clock::universal_time();
	boost::posix_time::ptime epoch(boost::gregorian::date(1970,1,1));
	boost::posix_time::ptime boot = now - boost::posix_time::time_duration(0, 0, value);

	long long now_delta = (now-epoch).total_seconds();
	long long uptime = static_cast<long long>(value);
	boost::shared_ptr<check_uptime_filter::filter_obj> record(new check_uptime_filter::filter_obj(uptime, now_delta, boot));
	filter.match(record);

	modern_filter::perf_writer scaler(response);
	filter_helper.post_process(filter, &scaler);
}

void CheckSystem::check_os_version(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {

	typedef os_version_filter::filter filter_type;
	modern_filter::data_container data;
	modern_filter::cli_helper<filter_type> filter_helper(request, response, data);

	filter_type filter;
	filter_helper.add_options(filter.get_filter_syntax(), "Version ok");
	filter_helper.add_syntax("${list}", filter.get_format_syntax(), "${kernel_name} ${nodename} ${kernel_release} ${kernel_version} ${machine}", "version");

	if (!filter_helper.parse_options())
		return;

	if (!filter_helper.build_filter(filter))
		return;

    struct utsname name;
    if (uname(&name) == -1)
		return nscapi::protobuf::functions::set_response_bad(*response, "Cannot get system name");

	boost::shared_ptr<os_version_filter::filter_obj> record(new os_version_filter::filter_obj());
	record->kernel_name = name.sysname;
	record->nodename = name.nodename;
	record->kernel_version = name.version;
	record->kernel_release = name.release;
	record->machine = name.machine;

	filter.match(record);

	modern_filter::perf_writer scaler(response);
	filter_helper.post_process(filter, &scaler);
}

void CheckSystem::check_service(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	/*
	typedef check_svc_filter::filter filter_type;
	modern_filter::data_container data;
	modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
	std::vector<std::string> services, excludes;
	std::string type;
	std::string state;
	std::string computer;

	filter_type filter;
	filter_helper.add_options(filter.get_filter_syntax(), "OK all services are ok.");
	filter_helper.add_syntax("${problem_list}", filter.get_format_syntax(), "${name}=${state} (${start_type})", "${name}");
	filter_helper.get_desc().add_options()
		("computer", po::value<std::string>(&computer), "THe name of the remote computer to check")
		("service", po::value<std::vector<std::string>>(&services), "The service to check, set this to * to check all services")
		("exclude", po::value<std::vector<std::string>>(&excludes), "A list of services to ignore (mainly usefull in combination with service=*)")
		("type", po::value<std::string>(&type)->default_value("service"), "The types of services to enumerate available types are driver, file-system-driver, kernel-driver, service, service-own-process, service-share-process")
		("state", po::value<std::string>(&state)->default_value("all"), "The types of services to enumerate available states are active, inactive or all")
		;

	if (!filter_helper.parse_options())
		return;

	if (filter_helper.empty()) {
		filter_helper.set_default("not state_is_perfect()", "not state_is_ok()");
	}

	if (services.empty()) {
		services.push_back("*");
	}
	if (!filter_helper.build_filter(filter))
		return;

	BOOST_FOREACH(const std::string &service, services) {
		if (service == "*") {
			BOOST_FOREACH(const services_helper::service_info &info, services_helper::enum_services(computer, services_helper::parse_service_type(type), services_helper::parse_service_state(state))) {
				if (std::find(excludes.begin(), excludes.end(), info.get_name())!=excludes.end()
					|| std::find(excludes.begin(), excludes.end(), info.get_desc())!=excludes.end()
					)
					continue;
				boost::shared_ptr<services_helper::service_info> record(new services_helper::service_info(info));
				boost::tuple<bool,bool> ret = filter.match(record);
				if (filter.has_errors())
					return nscapi::protobuf::functions::set_response_bad(*response, "Filter processing failed (see log for details)");
			}
		} else {
			services_helper::service_info info = services_helper::get_service_info(computer, service);
			boost::shared_ptr<services_helper::service_info> record(new services_helper::service_info(info));
			boost::tuple<bool,bool> ret = filter.match(record);
		}
	}
	modern_filter::perf_writer writer(response);
	filter_helper.post_process(filter, &writer);
	*/
}


void CheckSystem::check_pagefile(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	/*
	typedef check_page_filter::filter filter_type;
	modern_filter::data_container data;
	modern_filter::cli_helper<filter_type> filter_helper(request, response, data);

	filter_type filter;
	filter_helper.add_options(filter.get_filter_syntax(), "OK pagefile within bounds.");
	filter_helper.add_syntax("${problem_list}", filter.get_format_syntax(), "${name} ${used} (${size})", "${name}");

	if (!filter_helper.parse_options())
		return;

	if (filter_helper.empty()) {
		filter_helper.set_default("used > 60%", "used > 80%");
	}

	if (!filter_helper.build_filter(filter))
		return;

	windows::system_info::pagefile_info total("total");
	BOOST_FOREACH(const windows::system_info::pagefile_info &info, windows::system_info::get_pagefile_info()) {
		boost::shared_ptr<check_page_filter::filter_obj> record(new check_page_filter::filter_obj(info));
		boost::tuple<bool,bool> ret = filter.match(record);
		total.add(info);
	}
	boost::shared_ptr<check_page_filter::filter_obj> record(new check_page_filter::filter_obj(total));
	boost::tuple<bool,bool> ret = filter.match(record);

	modern_filter::perf_writer writer(response);
	filter_helper.post_process(filter, &writer);
	*/
}

long long read_mem_line(std::stringstream &iss) {
	std::string unit;
	unsigned long long value;
	iss >> value >> unit;
	if (unit == "kB") {
		value*=1024;
	} else if (!unit.empty()) {
		NSC_LOG_ERROR("Invalid memory unit: " + unit);
	}
	return value;
}
std::list<check_mem_filter::filter_obj> get_memory() {
	std::list<check_mem_filter::filter_obj> ret;
	check_mem_filter::filter_obj physical("physical", 0, 0);
	check_mem_filter::filter_obj swap("swap", 0, 0);
	long long cached = 0;

	try {
		std::locale mylocale("C");
		std::ifstream file;
		file.imbue(mylocale);
		file.open(MEMINFO_FILE);
		std::string line;
		while (std::getline(file, line)) {
			std::stringstream iss(line);
			std::string tag;
			iss >> tag;
			if (tag == "MemTotal:")
				physical.total = read_mem_line(iss);
			else if (tag == "MemFree:")
				physical.free = read_mem_line(iss);
			else if (tag == "Buffers:" || tag == "Cached:")
				cached += read_mem_line(iss);
			else if (tag == "SwapTotal:")
				swap.total += read_mem_line(iss);
			else if (tag == "SwapFree:")
				swap.free += read_mem_line(iss);
		}
		ret.push_back(physical);
		ret.push_back(check_mem_filter::filter_obj("cached", physical.get_used()-cached, physical.total));
		ret.push_back(swap);
	} catch (const std::exception &e) {
		return ret;
	}
    return ret;
}

void CheckSystem::check_memory(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	typedef check_mem_filter::filter filter_type;
	modern_filter::data_container data;
	modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
	std::vector<std::string> types;

	filter_type filter;
	filter_helper.add_options(filter.get_filter_syntax(), "OK memory within bounds.");
	filter_helper.add_syntax("${problem_list}", filter.get_format_syntax(), "${type} = ${used}", "${type}");
	filter_helper.get_desc().add_options()
		("type", po::value<std::vector<std::string> >(&types), "The type of memory to check (physical = Physical memory (RAM), committed = total memory (RAM+PAGE)")
		;

	if (!filter_helper.parse_options())
		return;

	if (filter_helper.empty()) {
		filter_helper.set_default("used > 80%", "used > 90%");
	}

	if (types.empty()) {
		types.push_back("physical");
		types.push_back("cached");
		types.push_back("swap");
	}

	if (!filter_helper.build_filter(filter))
		return;

	std::list<check_mem_filter::filter_obj> mem_data;
	try {
		mem_data = get_memory();
	} catch (const std::exception &e) {
		return nscapi::protobuf::functions::set_response_bad(*response, e.what());
	}

	BOOST_FOREACH(const std::string &type, types) {
		bool found = false;
		BOOST_FOREACH(const check_mem_filter::filter_obj &o, mem_data) {
			if (o.type == type) {
				boost::shared_ptr<check_mem_filter::filter_obj> record(new check_mem_filter::filter_obj(o));
				filter.match(record);
				found = true;
				break;
			}
		}
		if (!found) {
			return nscapi::protobuf::functions::set_response_bad(*response, "Invalid type: " + type);
		}
	}

	modern_filter::perf_writer writer(response);
	filter_helper.post_process(filter, &writer);
}


void CheckSystem::check_process(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	/*
	typedef check_proc_filter::filter filter_type;
	modern_filter::data_container data;
	modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
	std::vector<std::string> processes;
	bool deep_scan = true;
	bool vdm_scan = false;
	bool unreadable_scan = true;
	bool delta_scan = false;

	NSC_error err;
	filter_type filter;
	filter_helper.add_options(filter.get_filter_syntax(), "OK all processes are ok.");
	filter_helper.add_syntax("${problem_list}", filter.get_format_syntax(), "${exe}=${state}", "${exe}");
	filter_helper.get_desc().add_options()
		("process", po::value<std::vector<std::string>>(&processes), "The service to check, set this to * to check all services")
		("scan-info", po::value<bool>(&deep_scan), "If all process metrics should be fetched (otherwise only status is fetched)")
		("scan-16bit", po::value<bool>(&vdm_scan), "If 16bit processes should be included")
		("delta", po::value<bool>(&delta_scan), "Calculate delta over one elapsed second.\nThis call will mesure values and then sleep for 2 second and then measure again caluclating deltas.")
		("scan-unreadable", po::value<bool>(&unreadable_scan), "If unreadable processes should be included (will not have information)")
		;

	if (!filter_helper.parse_options())
		return;

	if (filter_helper.empty()) {
		if (data.filter_string.empty())
			data.filter_string = "state != 'unreadable'";
		filter_helper.set_default("state not in ('started')", "state = 'stopped'");
	}

	if (processes.empty()) {
		processes.push_back("*");
	}
	if (!filter_helper.build_filter(filter))
		return;

	std::set<std::string> procs;
	bool all = false;
	BOOST_FOREACH(const std::string &process, processes) {
		if (process == "*")
			all = true;
		else if (procs.count(process) == 0)
			procs.insert(process);
	}

	std::vector<std::string> matched;
	process_helper::process_list list = delta_scan?process_helper::enumerate_processes_delta(!unreadable_scan, &err):process_helper::enumerate_processes(!unreadable_scan, vdm_scan, deep_scan, &err);
	BOOST_FOREACH(const process_helper::process_info &info, list) {
		bool wanted = procs.count(info.exe);
		if (all || wanted) {
			boost::shared_ptr<process_helper::process_info> record(new process_helper::process_info(info));
			boost::tuple<bool,bool> ret = filter.match(record);
		}
		if (wanted) {
			matched.push_back(info.exe);
		}
	}
	BOOST_FOREACH(const std::string &proc, matched) {
		procs.erase(proc);
	}
	BOOST_FOREACH(const std::string proc, procs) {
		boost::shared_ptr<process_helper::process_info> record(new process_helper::process_info(proc));
		boost::tuple<bool,bool> ret = filter.match(record);
	}
	modern_filter::perf_writer writer(response);
	filter_helper.post_process(filter, &writer);
	*/
}
