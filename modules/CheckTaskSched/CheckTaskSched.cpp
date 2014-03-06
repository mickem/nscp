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

#include <boost/program_options.hpp>

#include "CheckTaskSched.h"
#include <strEx.h>
#include <time.h>
#include <map>
#include <vector>

#include <strEx.h>
#include "TaskSched.h"

#include "filter.hpp"

#include <parsers/filter/cli_helper.hpp>

#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>

#include <settings/client/settings_client.hpp>

namespace sh = nscapi::settings_helper;
namespace po = boost::program_options;

bool CheckTaskSched::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {
	return true;
}
bool CheckTaskSched::unloadModule() {
	return true;
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

void CheckTaskSched::CheckTaskSched_(Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	boost::program_options::options_description desc;

	std::vector<std::string> counters;
	std::string syntax, topSyntax, filter, arg_warn, arg_crit;
	bool debug = false;
	nscapi::program_options::add_help(desc);
	desc.add_options()
		("warn", po::value<std::string>(&arg_warn), "Warning bounds.")
		("crit", po::value<std::string>(&arg_crit), "Critical bounds.")
		("MaxWarn", po::value<std::string>(), "Maximum value before a warning is returned.")
		("MaxCrit", po::value<std::string>(), "Maximum value before a critical is returned.")
		("MinWarn", po::value<std::string>(), "Minimum value before a warning is returned.")
		("MinCrit", po::value<std::string>(), "Minimum value before a critical is returned.")
		("Counter", po::value<std::vector<std::string>>(&counters), "The time to check")
		("truncate", po::value<std::string>(), "Deprecated option")
		("syntax", po::value<std::string>(&syntax), "Syntax (same as detail-syntax in the check_tasksched check)")
		("master-syntax", po::value<std::string>(&topSyntax), "Master Syntax (same as top-syntax in the check_tasksched check)")
		("filter", po::value<std::string>(&filter), "Filter (same as filter in the check_tasksched check)")
		("debug", po::bool_switch(&debug), "Filter (same as filter in the check_tasksched check)")
		;

	boost::program_options::variables_map vm;
	std::vector<std::string> extra;
	if (!nscapi::program_options::process_arguments_from_request(vm, desc, request, *response, true, extra)) 
		return;
	std::string warn, crit;

	request.clear_arguments();
	if (vm.count("MaxWarn"))
		warn = "warn=count > " + vm["MaxWarn"].as<std::string>();
	if (vm.count("MaxCrit"))
		crit = "crit=count > " + vm["MaxCrit"].as<std::string>();
	if (vm.count("MinWarn"))
		warn = "warn=count < " + vm["MinWarn"].as<std::string>();
	if (vm.count("MinCrit"))
		crit = "crit=count > " + vm["MinCrit"].as<std::string>();
	if (!arg_warn.empty())
		warn = "warn=count " + arg_warn;
	if (!arg_crit.empty())
		crit = "crit=count " + arg_crit;
	if (!warn.empty())
		request.add_arguments(warn);
	if (!crit.empty())
		request.add_arguments(crit);
	if (debug)
		request.add_arguments("debug");
	if (!filter.empty())
		request.add_arguments("filter=" + filter);
	if (!topSyntax.empty()) {
		boost::replace_all(topSyntax, "%status%", "${status}");
		request.add_arguments("top-syntax=" + topSyntax);
	}
	if (!syntax.empty()) {
		boost::replace_all(syntax, "%title%", "${title}");
		boost::replace_all(syntax, "%status%", "${status}");
		boost::replace_all(syntax, "%exit_code%", "${exit_code}");
		boost::replace_all(syntax, "%most_recent_run_time%", "${most_recent_run_time}");
		
		request.add_arguments("detail-syntax=" + syntax);
	}

	log_args(request);
	check_tasksched(request, response);
}


void CheckTaskSched::check_tasksched(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	typedef tasksched_filter::filter filter_type;
	modern_filter::data_container data;
	modern_filter::cli_helper<filter_type> filter_helper(request, response, data);

	std::vector<std::string> file_list;
	std::string files_string;
	std::string computer, user, domain, password, folder;
	bool recursive;

	filter_type filter;
	filter_helper.add_options(filter.get_filter_syntax(), "All stats ok");
	filter_helper.add_syntax("${problem_list}", filter.get_format_syntax(), "${folder}/${title}: ${exit_code} != 0", "${title}");
	filter_helper.get_desc().add_options()
		("computer", po::value<std::string>(&computer), "The name of the computer that you want to connect to.")
		("user", po::value<std::string>(&user), "The user name that is used during the connection to the computer.")
		("domain", po::value<std::string>(&domain), "The domain of the user specified in the user parameter.")
		("password", po::value<std::string>(&password), "The password that is used to connect to the computer. If the user name and password are not specified, then the current token is used.")
		("folder", po::value<std::string>(&folder), "The folder in which the tasks to check reside.")
		("recursive", po::value<bool>(&recursive), "Recurse subfolder (defaults to true).")
		;

	if (!filter_helper.parse_options())
		return;

	if (filter_helper.empty()) {
		if (data.filter_string.empty()) {
			data.filter_string = "enabled = 1";
		}
		filter_helper.set_default("exit_code != 0", "exit_code < 0");
	}

	if (!filter_helper.build_filter(filter))
		return;

	try {
		TaskSched query;
		query.findAll(filter, computer, user, domain, password, folder, recursive);
		modern_filter::perf_writer writer(response);
		filter_helper.post_process(filter, &writer);
	} catch (const nscp_exception &e) {
		return nscapi::protobuf::functions::set_response_bad(*response, "Failed to fetch tasks: " + e.reason());
	}
}

int CheckTaskSched::commandLineExec(const std::string &command, const std::list<std::string> &arguments, std::string &result) {
	return 0;
}
