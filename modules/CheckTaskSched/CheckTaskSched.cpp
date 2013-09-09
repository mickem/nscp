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
	sh::settings_registry settings(get_settings_proxy());
	settings.set_alias("check", alias, "task schedule");

	settings.alias().add_path_to_settings()
		("TASK SCHEDULE", "Section for system checks and system settings")

		;

	settings.alias().add_key_to_settings()

		("default buffer length", sh::wstring_key(&syntax, _T("%title% last run: %most-recent-run-time% (%exit-code%)")),
		"SYNTAX", "Set this to use a specific syntax string for all commands (that don't specify one)")
		;


	settings.register_all();
	settings.notify();
	return true;
}
bool CheckTaskSched::unloadModule() {
	return true;
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
	} catch (const nscp_exception &e) {
		return nscapi::protobuf::functions::set_response_bad(*response, "WMIQuery failed: " + e.reason());
	}
	modern_filter::perf_writer writer(response);
	filter_helper.post_process(filter, &writer);
}

int CheckTaskSched::commandLineExec(const std::string &command, const std::list<std::string> &arguments, std::string &result) {
	return 0;
}
