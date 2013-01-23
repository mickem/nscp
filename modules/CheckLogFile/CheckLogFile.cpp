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
#include <utils.h>
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

bool CheckLogFile::loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode) {
	thread_.reset(new real_time_thread);

	sh::settings_registry settings(get_settings_proxy());
	settings.set_alias(alias, _T("logfile"));

	thread_->filters_path_ = settings.alias().get_settings_path(_T("real-time/checks"));

	settings.alias().add_path_to_settings()
		(_T("LOG FILE SECTION"), _T("Section for log file checker"))

		(_T("real-time"), _T("CONFIGURE REALTIME CHECKING"), _T("A set of options to configure the real time checks"))

		(_T("real-time/checks"), sh::fun_values_path(boost::bind(&real_time_thread::add_realtime_filter, thread_, get_settings_proxy(), _1, _2)),  
		_T("REALTIME FILTERS"), _T("A set of filters to use in real-time mode"))
		;

//	settings.alias().add_key_to_settings()
//		;

	settings.alias().add_key_to_settings(_T("real-time"))

		(_T("enabled"), sh::bool_fun_key<bool>(boost::bind(&real_time_thread::set_enabled, thread_, _1), false),
		_T("REAL TIME CHECKING"), _T("Spawns a backgrounnd thread which waits for file changes."))

		;

	settings.register_all();
	settings.notify();

	if (mode == NSCAPI::normalStart) {
		if (!thread_->start())
			NSC_LOG_ERROR_STD(_T("Failed to start collection thread"));
	}
	return true;
}
bool CheckLogFile::unloadModule() {
	if (thread_ && !thread_->stop())
		NSC_LOG_ERROR_STD(_T("Failed to stop thread"));
	return true;
}

void CheckLogFile::check_logfile(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	simple_timer time;
	std::string tmp_msg;

	std::string regexp, line_split, column_split;
	std::string filter_string, warn_string, crit_string, ok_string;
	std::string syntax_top, syntax_detail, empty_detail;
	std::vector<std::string> file_list;
	std::string files_string;
	std::string mode;

	po::options_description desc("Allowed options");
	desc.add_options()
		("help",													"Show help screen")
		("help-plumbing",											"Show help screen in a format easy to parse by scripts")
		("regexp", po::value<std::string>(&regexp),					"Lookup a numeric value in the PDH index table")
		("split", po::value<std::string>(&column_split),			"Lookup a string value in the PDH index table")
		("line-split", po::value<std::string>(&line_split)->default_value("\\n"), 
																	"Lookup a string value in the PDH index table")
		("column-split", po::value<std::string>(&column_split),		"Expand a counter path contaning wildcards into corresponding objects (for instance --expand-path \\System\\*)")
		("filter", po::value<std::string>(&filter_string),			"Check that performance counters are working")
		("warn", po::value<std::string>(&warn_string),				"Filter which generates a warning state")
		("crit", po::value<std::string>(&crit_string),				"Filter which generates a critical state")
		("warning", po::value<std::string>(&warn_string),			"Filter which generates a warning state")
		("critical", po::value<std::string>(&crit_string),			"Filter which generates a critical state")
		("ok", po::value<std::string>(&ok_string),					"Filter which generates an ok state")
		("top-syntax", po::value<std::string>(&syntax_top)->default_value("${file}: ${count} (${messages})"), 
																	"Top level syntax")
		("detail-syntax", po::value<std::string>(&syntax_detail)->default_value("${column1}, "), 
																	"Detail level syntax")
		("empty-syntax", po::value<std::string>(&empty_detail)->default_value("No matches"), 
																	"Message to use when no matchies was found")
		("file", po::value<std::vector<std::string> >(&file_list),	"List counters and/or instances")
		("files", po::value<std::string>(&files_string),			"List/check all counters not configured counter")
		("mode", po::value<std::string>(&mode),						"Mode of operation: count (count all critical/warning lines), find (find first critical/warning line)")
		;
	boost::program_options::variables_map vm;
	nscapi::program_options::basic_command_line_parser cmd(request);
	cmd.options(desc);
	cmd.extra_style_parser(nscapi::program_options::option_parser);

	po::parsed_options parsed = cmd.run();
	po::store(parsed, vm);
	po::notify(vm);

	if (vm.count("help-plumbing"))
		return nscapi::program_options::basic_command_line_parser::help_plumbing(desc, request.command(), response);
	if (vm.count("help") || (filter_string.empty() && file_list.empty()))
		return nscapi::program_options::basic_command_line_parser::invalid_syntax(desc, request.command(), response);

	logfile_filter::filter filter;
	if (!filter.build_syntax(syntax_top, syntax_detail, tmp_msg))
		return nscapi::protobuf::query::set_response_unknown(response, tmp_msg);
	filter.build_engines(filter_string, ok_string, warn_string, crit_string);

	if (!column_split.empty()) {
		strEx::replace(column_split, "\\t", "\t");
		strEx::replace(column_split, "\\n", "\n");
	}

	if (!filter.validate(tmp_msg))
		return nscapi::protobuf::query::set_response_unknown(response, tmp_msg);

	NSC_DEBUG_MSG_STD(_T("Boot time: ") + strEx::itos(time.stop()));

	BOOST_FOREACH(const std::string &filename, file_list) {
		std::ifstream file(filename.c_str());
		if (file.is_open()) {
			std::string line;
			filter.summary.filename = filename;
			while (file.good()) {
				std::getline(file,line, '\n');
				if (!column_split.empty()) {
					std::list<std::string> chunks = strEx::s::splitEx(line, column_split);
					boost::shared_ptr<logfile_filter::filter_obj> record(new logfile_filter::filter_obj(filename, line, chunks, filter.summary.match_count));
					boost::tuple<bool,bool> ret = filter.match(record);
					if (ret.get<1>()) {
						break;
					}
				}
			}
			file.close();
		} else {
			return nscapi::protobuf::query::set_response_unknown(response, "Failed to open file: " + filename);
		}
	}
	NSC_DEBUG_MSG_STD(_T("Evaluation time: ") + strEx::itos(time.stop()));

	filter.fetch_perf();
	response->set_result(nscapi::functions::nagios_status_to_gpb(filter.returnCode));
	if (filter.message.empty())
		response->set_message(empty_detail);
	else
		response->set_message(filter.message);
}
