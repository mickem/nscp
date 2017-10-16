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
#include <nscapi/nscapi_settings_helper.hpp>
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

void CheckSystem::check_cpu(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {

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
	filter_helper.add_options("uptime < 2d", "uptime < 1d", "", filter.get_filter_syntax(), "ignored");
	filter_helper.add_syntax("${status}: ${list}", "uptime: ${uptime}h, boot: ${boot} (UTC)", "uptime", "", "");

	if (!filter_helper.parse_options())
		return;

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

	filter_helper.post_process(filter);
}

void CheckSystem::check_os_version(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {

	typedef os_version_filter::filter filter_type;
	modern_filter::data_container data;
	modern_filter::cli_helper<filter_type> filter_helper(request, response, data);

	filter_type filter;
	filter_helper.add_options("version > 50", "version > 50", "", filter.get_filter_syntax(), "ignored");
	filter_helper.add_syntax("${status}: ${list}", "${version} (${major}.${minor}.${build})", "version", "", "");

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

	filter_helper.post_process(filter);
}

void CheckSystem::check_service(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {

}


void CheckSystem::check_pagefile(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {

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
	filter_helper.add_options("used > 80%", "used > 90%", "", filter.get_filter_syntax(), "ignored");
	filter_helper.add_syntax("${status}: ${list}", "${type} = ${used}", "${type}", "", "");
	filter_helper.get_desc().add_options()
		("type", po::value<std::vector<std::string> >(&types), "The type of memory to check (physical = Physical memory (RAM), committed = total memory (RAM+PAGE)")
		;

	if (!filter_helper.parse_options())
		return;

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

	filter_helper.post_process(filter);
}


void CheckSystem::check_process(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {

}
