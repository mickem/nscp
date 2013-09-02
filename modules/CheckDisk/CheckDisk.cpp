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

std::wstring CheckDisk::get_filter(unsigned int drvType) {
	if (drvType==DRIVE_FIXED)
		return _T("FIXED");
	if (drvType==DRIVE_NO_ROOT_DIR)
		return _T("NO_ROOT_DIR");
	if (drvType==DRIVE_CDROM)
		return _T("CDROM");
	if (drvType==DRIVE_REMOTE)
		return _T("REMOTE");
	if (drvType==DRIVE_REMOVABLE)
		return _T("REMOVABLE");
	return _T("unknown: ") + strEx::itos(drvType);
}

void CheckDisk::check_files(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {

	modern_filter::cli_helper<file_filter::filter> filter_helper(request, response);
	std::string regexp, line_split, column_split;
	std::vector<std::string> file_list;
	std::string files_string;
	std::string mode;
	std::list<std::string> paths;
	bool ignoreError = false;

	filter_helper.add_options();
	filter_helper.add_syntax("${file}: ${count} (${problem_list})", "TODO", "${column1}", "${column1}", "TODO");
	filter_helper.get_desc().add_options()
		//		("regexp", po::value<std::string>(&regexp),					"Lookup a numeric value in the PDH index table")
		("line-split", po::value<std::string>(&line_split)->default_value("\\n"), 
		"Character string to split a file into lines")
		("column-split", po::value<std::string>(&column_split)->default_value("\\t"),
		"Character string to split a line into columns")
		("split", po::value<std::string>(&column_split),			"Short alias for split-column")
		("file", po::value<std::vector<std::string> >(&file_list),	"File to read (can be specified multiple times to check multiple files.\nNotice that specifying multiple files will create an aggregate set you will not check each file individually."
		"In other words if one file contains an error the entire check will result in error.")
		//		("files", po::value<std::string>(&files_string),			"A comma separated list of files to scan")
		//		("mode", po::value<std::string>(&mode),						"Mode of operation: count (count all critical/warning lines), find (find first critical/warning line)")
		;

	// 			MAP_OPTIONS_PUSH("path", paths)
	// 			MAP_OPTIONS_PUSH("file", paths)
	// 			MAP_OPTIONS_BOOL_TRUE("ignore-errors", ignoreError)
	// 			MAP_OPTIONS_STR2INT("max-dir-depth", fargs->max_level)
	// 			MAP_OPTIONS_STR("perf-unit", tmpObject.perf_unit)


	filter_helper.parse_options();


	if (paths.empty())
		return nscapi::protobuf::functions::set_response_bad(*response, "No column-split specified");

	file_filter::filter filter;
	if (!filter_helper.build_filter(filter))
		return;

	//file_filter::filter_result result = file_filter::factories::create_result(fargs);
	file_finder::scanner_context context;
	BOOST_FOREACH(const std::string &path, paths) {
		file_finder::recursive_scan(filter, context, path);
// 		if (!ignoreError && fargs->error->has_error()) {
// 			if (show_errors_)
// 				msg = fargs->error->get_error();
// 			else
// 				msg = "Check contains error. Check log for details (or enable show_errors in nsc.ini)";
// 			return NSCAPI::returnUNKNOWN;
// 		}
	}

	filter_helper.post_process(filter, NULL);
}
