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

#include <map>
#include <vector>

#include <boost/foreach.hpp>
#include <boost/bind.hpp>
#include <boost/assign.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/cmdline.hpp>
#include <boost/filesystem.hpp>

#include <parsers/expression/expression.hpp>

#include <time.h>
#include <error.hpp>

#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_program_options.hpp>

#include <parsers/where/unary_fun.hpp>
#include <parsers/where/list_value.hpp>
#include <parsers/where/binary_op.hpp>
#include <parsers/where/unary_op.hpp>
#include <parsers/where/variable.hpp>

#include <simple_timer.hpp>
#include <settings/client/settings_client.hpp>

#include "CheckLogFile.h"
#include "real_time_thread.hpp"
#include "filter.hpp"
#include "filter_config_object.hpp"

namespace sh = nscapi::settings_helper;
namespace po = boost::program_options;

bool CheckLogFile::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {
	thread_.reset(new real_time_thread);

	sh::settings_registry settings(get_settings_proxy());
	settings.set_alias(alias, "logfile");

	thread_->filters_path_ = settings.alias().get_settings_path("real-time/checks");

	settings.alias().add_path_to_settings()
		("LOG FILE SECTION", "Section for log file checker")

		("real-time", "CONFIGURE REALTIME CHECKING", "A set of options to configure the real time checks")

		("real-time/checks", sh::fun_values_path(boost::bind(&real_time_thread::add_realtime_filter, thread_, get_settings_proxy(), _1, _2)),  
		"REALTIME FILTERS", "A set of filters to use in real-time mode")
		;

//	settings.alias().add_key_to_settings()
//		;

	settings.alias().add_key_to_settings("real-time")

		("enabled", sh::bool_fun_key<bool>(boost::bind(&real_time_thread::set_enabled, thread_, _1), false),
		"REAL TIME CHECKING", "Spawns a background thread which waits for file changes.")

		;

	settings.register_all();
	settings.notify();

	filters::command_reader::object_type tmp;
	tmp.alias = "sample";
	tmp.path = thread_->filters_path_ + "/sample";
	filters::command_reader::read_object(get_settings_proxy(), tmp, false, true);

	if (mode == NSCAPI::normalStart) {
		if (!thread_->start())
			NSC_LOG_ERROR_STD("Failed to start collection thread");
	}
	return true;
}
bool CheckLogFile::unloadModule() {
	if (thread_ && !thread_->stop())
		NSC_LOG_ERROR_STD("Failed to stop thread");
	return true;
}

void CheckLogFile::check_logfile(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	std::string tmp_msg;

	std::string regexp, line_split, column_split;
	std::string filter_string, warn_string, crit_string, ok_string;
	std::string syntax_top, syntax_detail, empty_detail, empty_state;
	std::vector<std::string> file_list;
	std::string files_string;
	std::string mode;

	po::options_description desc = nscapi::program_options::create_desc(request);
	desc.add_options()
//		("regexp", po::value<std::string>(&regexp),					"Lookup a numeric value in the PDH index table")
		("line-split", po::value<std::string>(&line_split)->default_value("\\n"), 
																	"Character string to split a file into lines")
		("column-split", po::value<std::string>(&column_split)->default_value("\\t"),
																	"Character string to split a line into columns")
		("split", po::value<std::string>(&column_split),			"Short alias for split-column")
		("filter", po::value<std::string>(&filter_string),			"Filter which marks interesting items.\nInteresting items are items which will be included in the check. They do not denote warning or critical state but they are checked use this to filter out unwanted items.")
		("warning", po::value<std::string>(&warn_string),			"Filter which marks items which generates a warning state.\nIf anything matches this filter the return status will be escalated to warning.")
		("warn", po::value<std::string>(&warn_string),				"Short alias for warning")
		("critical", po::value<std::string>(&crit_string),			"Filter which marks items which generates a critical state.\nIf anything matches this filter the return status will be escalated to critical.")
		("crit", po::value<std::string>(&crit_string),				"Short alias for critical.")
		("ok", po::value<std::string>(&ok_string),					"Filter which marks items which generates an ok state.\n"
																	"If anything matches this any previous state for this item will be reset to ok. "
																	"Consider a line which contains \"aaa,1000,1\" and you set warning to \"warning=column_int2 > 500\"."
																	"Setting ok to \"ok=column_int3=1\" will override the warning state and ignore escalation for this line.")
		("top-syntax", po::value<std::string>(&syntax_top)->default_value("${file}: ${count} (${problem_list})"), 
																	"Top level syntax.\n"
																	"Used to format the message to return can include strings as well as special keywords such as:\n"
																	"${count}          Number of matching lines (for filter)\n"
																	"${warning_count}  Number of warning lines found\n"
																	"${critical_count} Number of critical lines found\n"
																	"${problem_count}  Number of either warning or critical lines found\n"
																	"${list}           A list of all lines matching filter (to format the list use the syntax option)\n"
																	"${warning_list}   A list of all lines matching warning (to format the list use the syntax option)\n"
																	"${critical_list}  A list of all lines matching critical (to format the list use the syntax option)\n"
																	"${problem_list}   A list of all lines matching either warning or critical (to format the list use the syntax option)\n"
																	"${file}           The name of the file"
																	)
		("detail-syntax", po::value<std::string>(&syntax_detail)->default_value("${column1}"), 
																	"Detail level syntax.\n"
																	"This is the syntax of each item in the list of top-syntax (see above).\n"
																	"Possible values are:\n"
																	"${line}    The entire line\n"
																	"${column1} (to ${column9})Data in column identified by the number. First column is 1.\n"
																	"${file}    The name of the file"
																	)
		("empty-syntax", po::value<std::string>(&empty_detail)->default_value("No matches"), 
																	"Message to display when nothing matched filter.\nIf no filter is specified this will never happen unless the file is empty.")
		("empty-state", po::value<std::string>(&empty_state)->default_value("unknown"), 
																	"Return status to use when nothing matched filter.\nIf no filter is specified this will never happen unless the file is empty.")
		("file", po::value<std::vector<std::string> >(&file_list),	"File to read (can be specified multiple times to check multiple files.\nNotice that specifying multiple files will create an aggregate set you will not check each file individually."
																	"In other words if one file contains an error the entire check will result in error.")
//		("files", po::value<std::string>(&files_string),			"A comma separated list of files to scan")
//		("mode", po::value<std::string>(&mode),						"Mode of operation: count (count all critical/warning lines), find (find first critical/warning line)")
		;

	boost::program_options::variables_map vm;
	if (!nscapi::program_options::process_arguments_from_request(vm, desc, request, *response)) 
		return;
	if (filter_string.empty() && file_list.empty())
		return nscapi::program_options::invalid_syntax(desc, request.command(), "Missing filter (filter=...)", *response);
	if (column_split.empty())
		return nscapi::protobuf::functions::set_response_bad(*response, "No column-split specified");
	if (line_split.empty())
		return nscapi::protobuf::functions::set_response_bad(*response, "No line-split specified");

	logfile_filter::filter filter;
	if (!filter.build_syntax(syntax_top, syntax_detail, tmp_msg))
		return nscapi::protobuf::functions::set_response_bad(*response, tmp_msg);
	if (!filter.build_engines(filter_string, ok_string, warn_string, crit_string, tmp_msg)) 
		return nscapi::protobuf::functions::set_response_bad(*response, tmp_msg);

	strEx::replace(column_split, "\\t", "\t");
	strEx::replace(column_split, "\\n", "\n");

	if (!filter.validate(tmp_msg))
		return nscapi::protobuf::functions::set_response_bad(*response, tmp_msg);

	filter.reset();

	BOOST_FOREACH(const std::string &filename, file_list) {
		std::ifstream file(filename.c_str());
		if (file.is_open()) {
			std::string line;
			filter.summary.filename = filename;
			while (file.good()) {
				std::getline(file,line, '\n');
				std::list<std::string> chunks = strEx::s::splitEx(line, column_split);
				boost::shared_ptr<logfile_filter::filter_obj> record(new logfile_filter::filter_obj(filename, line, chunks, filter.summary.count_match));
				boost::tuple<bool,bool> ret = filter.match(record);
				if (ret.get<1>()) {
					break;
				}
			}
			file.close();
		} else {
			return nscapi::protobuf::functions::set_response_bad(*response, "Failed to open file: " + filename);
		}
	}

	if (!filter.has_matched) {
		response->set_message(empty_detail);
		response->set_result(nscapi::protobuf::functions::nagios_status_to_gpb(nscapi::plugin_helper::translateReturn(empty_state)));
		return;
	}
	filter.fetch_perf();
	response->set_result(nscapi::protobuf::functions::nagios_status_to_gpb(filter.returnCode));
	response->set_message(filter.message);
}
