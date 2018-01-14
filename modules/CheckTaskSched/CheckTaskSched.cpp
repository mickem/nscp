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


#include "CheckTaskSched.h"


#include "TaskSched.h"
#include "filter.hpp"

#include <str/utils.hpp>

#include <parsers/filter/cli_helper.hpp>

#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>

#include <boost/program_options.hpp>

#include <map>
#include <vector>

namespace sh = nscapi::settings_helper;
namespace po = boost::program_options;

bool CheckTaskSched::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode) {
	return true;
}
bool CheckTaskSched::unloadModule() {
	return true;
}

void log_args(const Plugin::QueryRequestMessage::Request &request) {
	std::stringstream ss;
	for (int i = 0; i < request.arguments_size(); i++) {
		if (i > 0)
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
		boost::replace_all(topSyntax, "%status%", "${task_status}");
		request.add_arguments("top-syntax=" + topSyntax);
	}
	if (!syntax.empty()) {
		boost::replace_all(syntax, "%title%", "${title}");
		boost::replace_all(syntax, "%status%", "${task_status}");
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
	bool recursive = true, old = false, hidden = false;

	filter_type filter;
	filter_helper.add_options("exit_code != 0", "exit_code < 0", "enabled = 1", filter.get_filter_syntax(), "warning");
	filter_helper.add_syntax("${status}: ${problem_list}", "${folder}/${title}: ${exit_code} != 0", "${title}", "%(status): No tasks found", "%(status): All tasks are ok");
	filter_helper.get_desc().add_options()
		("force-old", po::bool_switch(&old), "The name of the computer that you want to connect to.")
		("computer", po::value<std::string>(&computer), "The name of the computer that you want to connect to.")
		("user", po::value<std::string>(&user), "The user name that is used during the connection to the computer.")
		("domain", po::value<std::string>(&domain), "The domain of the user specified in the user parameter.")
		("password", po::value<std::string>(&password), "The password that is used to connect to the computer. If the user name and password are not specified, then the current token is used.")
		("folder", po::value<std::string>(&folder), "The folder in which the tasks to check reside.")
		("recursive", po::value<bool>(&recursive), "Recurse sub folder (defaults to true).")
		("hidden", po::value<bool>(&hidden), "Look for hidden tasks.")
		;

	if (!filter_helper.parse_options())
		return;

	if (!filter_helper.build_filter(filter))
		return;

	try {
		TaskSched query;
		query.findAll(filter, computer, user, domain, password, folder, recursive, hidden, old);
		filter_helper.post_process(filter);
	} catch (const nsclient::nsclient_exception &e) {
		return nscapi::protobuf::functions::set_response_bad(*response, "Failed to fetch tasks: " + e.reason());
	}
}