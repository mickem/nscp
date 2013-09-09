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
