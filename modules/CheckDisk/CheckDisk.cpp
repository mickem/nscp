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
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/
#include "stdafx.h"
#include "CheckDisk.h"
#include <time.h>
#include <error.hpp>
#include <file_helpers.hpp>
#include <utils.h>

#include <parsers/expression/expression.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/filter/cli_helper.hpp>

#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>


#include "file_info.hpp"
#include "file_finder.hpp"
#include "filter.hpp"
#include <char_buffer.hpp>
#include <settings/client/settings_client.hpp>

#include <config.h>


#include "check_drive.hpp"

namespace sh = nscapi::settings_helper;
namespace po = boost::program_options;

CheckDisk::CheckDisk() : show_errors_(false) {
}

void log_args(const Plugin::QueryRequestMessage::Request &request) {
	std::stringstream ss;
	for (int i=0;i<request.arguments_size();i++) {
		if (i>0)
			ss << " ";
		ss << request.arguments(i);
	}
	NSC_DEBUG_MSG("Created command: " + ss.str());
}

void CheckDisk::checkDriveSize(Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	boost::program_options::options_description desc;

	std::vector<std::string> times;
	std::vector<std::string> types;
	nscapi::program_options::add_help(desc);
	desc.add_options()
		("ShowAll", po::value<std::string>()->implicit_value("short"), "Configures display format (if set shows all items not only failures, if set to long shows all cores).")
		("CheckAll", po::value<std::string>()->implicit_value("true"), "Checks all drives.")
		("CheckAllOthers", po::value<std::string>()->implicit_value("true"), "Checks all drives turns the drive option into an exclude option.")
		("MaxWarn", po::value<std::string>(), "Maximum value before a warning is returned.")
		("MaxCrit", po::value<std::string>(), "Maximum value before a critical is returned.")
		("MinWarn", po::value<std::string>(), "Minimum value before a warning is returned.")
		("MinCrit", po::value<std::string>(), "Minimum value before a critical is returned.")
		("MaxWarnFree", po::value<std::string>(), "Maximum value before a warning is returned.")
		("MaxCritFree", po::value<std::string>(), "Maximum value before a critical is returned.")
		("MinWarnFree", po::value<std::string>(), "Minimum value before a warning is returned.")
		("MinCritFree", po::value<std::string>(), "Minimum value before a critical is returned.")
		("MaxWarnUsed", po::value<std::string>(), "Maximum value before a warning is returned.")
		("MaxCritUsed", po::value<std::string>(), "Maximum value before a critical is returned.")
		("MinWarnUsed", po::value<std::string>(), "Minimum value before a warning is returned.")
		("MinCritUsed", po::value<std::string>(), "Minimum value before a critical is returned.")
		("Drive", po::value<std::vector<std::string>>(&times), "The drives to check")
		("FilterType", po::value<std::vector<std::string>>(&types), "The type of drives to check fixed, remote, cdrom, ramdisk, removable")
		;

	boost::program_options::variables_map vm;
	if (!nscapi::program_options::process_arguments_from_request(vm, desc, request, *response)) 
		return;
	std::string warn, crit;

	request.clear_arguments();
	if (vm.count("MaxWarn"))
		warn = "warn=used > " + vm["MaxWarn"].as<std::string>();
	if (vm.count("MaxCrit"))
		crit = "crit=used > " + vm["MaxCrit"].as<std::string>();
	if (vm.count("MinWarn"))
		warn = "warn=free < " + vm["MinWarn"].as<std::string>();
	if (vm.count("MinCrit"))
		crit = "crit=free < " + vm["MinCrit"].as<std::string>();
	if (vm.count("MaxWarnUsed"))
		warn = "warn=used > " + vm["MaxWarnUsed"].as<std::string>();
	if (vm.count("MaxCritUsed"))
		crit = "crit=used > " + vm["MaxCritUsed"].as<std::string>();
	if (vm.count("MinWarnUsed"))
		warn = "warn=used < " + vm["MinWarnUsed"].as<std::string>();
	if (vm.count("MinCritUsed"))
		crit = "crit=used < " + vm["MinCritUsed"].as<std::string>();
	if (vm.count("MaxWarnFree"))
		warn = "warn=free > " + vm["MaxWarnFree"].as<std::string>();
	if (vm.count("MaxCritFree"))
		crit = "crit=free > " + vm["MaxCritFree"].as<std::string>();
	if (vm.count("MinWarnFree"))
		warn = "warn=free < " + vm["MinWarnFree"].as<std::string>();
	if (vm.count("MinCritFree"))
		crit = "crit=free < " + vm["MinCritFree"].as<std::string>();
	if (!warn.empty())
		request.add_arguments(warn);
	if (!crit.empty())
		request.add_arguments(crit);
	if (vm.count("CheckAll"))
		request.add_arguments("drive=*");
	bool exclude = false;
	if (vm.count("CheckAllOthers")) {
		request.add_arguments("drive=*");
		exclude = true;
	}
	request.add_arguments("detail-syntax=${drive}: Total: ${size} - Used: ${used} - Free: ${free}");
	if (vm.count("ShowAll")) {
		//if (vm["ShowAll"].as<std::string>() == "long")
		request.add_arguments("top-syntax=${status}: ${list}");
	}
	BOOST_FOREACH(const std::string &t, times) {
		if (exclude)
			request.add_arguments("exclude=" + t);
		else
			request.add_arguments("drive=" + t);
	}
	if (!types.empty()) {
		std::string type_list = "";
		BOOST_FOREACH(const std::string &s, types) {
			if (!type_list.empty())
				type_list += ", ";
			type_list += "'" + s + "'";
		}
		request.add_arguments("filter=type in (" + type_list +")");
	}
	log_args(request);
	check_drive::check(request, response);
}

void CheckDisk::check_drivesize(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	check_drive::check(request, response);
}

void CheckDisk::check_files(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	modern_filter::data_container data;
	modern_filter::cli_helper<file_filter::filter> filter_helper(request, response, data);
	std::vector<std::string> file_list;
	std::string files_string;
	std::string mode;
	bool ignoreError = false;

	file_filter::filter filter;
	filter_helper.add_options(filter.get_filter_syntax(), "All files ok");
	filter_helper.add_syntax("${count} files (${problem_list})", filter.get_format_syntax(), "${name}", "${name}");
	filter_helper.get_desc().add_options()
		("path", po::value<std::vector<std::string> >(&file_list),	"The path to search for files under.\nNotice that specifying multiple path will create an aggregate set you will not check each path individually."
		"In other words if one path contains an error the entire check will result in error.")
		("file", po::value<std::vector<std::string> >(&file_list),	"Alias for path.")
		("paths", po::value<std::string>(&files_string),			"A comma separated list of paths to scan")
		;

	// 			MAP_OPTIONS_BOOL_TRUE("ignore-errors", ignoreError)
	// 			MAP_OPTIONS_STR2INT("max-dir-depth", fargs->max_level)
	// 			MAP_OPTIONS_STR("perf-unit", tmpObject.perf_unit)



	if (!filter_helper.parse_options())
		return;

	if (!files_string.empty())
		boost::split(file_list, files_string, boost::is_any_of(","));

	if (file_list.empty())
		return nscapi::protobuf::functions::set_response_bad(*response, "No path specified");

	if (!filter_helper.build_filter(filter))
		return;

	file_finder::scanner_context context;
	BOOST_FOREACH(const std::string &path, file_list) {
		file_finder::recursive_scan(filter, context, path);
 		//if (!ignoreError && fargs->error->has_error())
		//	return nscapi::protobuf::functions::set_response_bad(*response, fargs->error->get_error());
	}
	modern_filter::perf_writer writer(response);
	filter_helper.post_process(filter, &writer);
}
