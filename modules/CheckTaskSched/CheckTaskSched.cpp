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
#include <utils.h>
#include <checkHelpers.hpp>
#include "TaskSched.h"

#include "filter.hpp"

#include <parsers/where/unary_fun.hpp>
#include <parsers/where/list_value.hpp>
#include <parsers/where/binary_op.hpp>
#include <parsers/where/unary_op.hpp>
#include <parsers/where/variable.hpp>

#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>

#include <settings/client/settings_client.hpp>

namespace sh = nscapi::settings_helper;
namespace po = boost::program_options;

bool CheckTaskSched::loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode) {
	sh::settings_registry settings(get_settings_proxy());
	settings.set_alias(_T("check"), alias, _T("task schedule"));

	settings.alias().add_path_to_settings()
		(_T("TASK SCHEDULE"), _T("Section for system checks and system settings"))

		;

	settings.alias().add_key_to_settings()

		(_T("default buffer length"), sh::wstring_key(&syntax, _T("%title% last run: %most-recent-run-time% (%exit-code%)")),
		_T("SYNTAX"), _T("Set this to use a specific syntax string for all commands (that don't specify one)"))
		;


	settings.register_all();
	settings.notify();
	return true;
}
bool CheckTaskSched::unloadModule() {
	return true;
}

void CheckTaskSched::check_tasksched(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	typedef checkHolders::CheckContainer<checkHolders::MaxMinBounds<checkHolders::NumericBounds<int, checkHolders::int_handler> > > WMIContainerQuery1;
	typedef checkHolders::CheckContainer<checkHolders::ExactBounds<checkHolders::NumericBounds<int, checkHolders::int_handler> > > WMIContainerQuery2;

	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	unsigned int truncate = 0;
	std::wstring query, alias;
	bool bPerfData = true;
	std::wstring syntax_top = _T("%list%");
	tasksched_filter::filter_argument args = tasksched_filter::factories::create_argument(_T("%task%"), DATE_FORMAT);

	WMIContainerQuery1 query1;
	WMIContainerQuery2 query2;


	po::options_description desc = nscapi::program_options::create_desc(request);
	desc.add_options()
		("truncate", po::value<unsigned int>(&truncate), "Truncate the resulting message (mainly useful in older version of nsclient++)")
		("filter", po::wvalue<std::wstring>(&args->filter),			"Filter which marks interesting items.\nInteresting items are items which will be included in the check. They do not denote warning or critical state but they are checked use this to filter out unwanted items.")
		("alias", po::wvalue<std::wstring>(&alias),			"Alias: TODO.")
		("top-syntax", po::wvalue<std::wstring>(&syntax_top)->default_value(_T("%list%")), "Top level syntax.\n")
		("master-syntax", po::wvalue<std::wstring>(&syntax_top)->default_value(_T("%list%")), "Top level syntax.\n")
		("syntax", po::wvalue<std::wstring>(&args->syntax)->default_value(_T("%task%")), "Detail level syntax.")
		("date-syntax", po::wvalue<std::wstring>(&args->date_syntax)->default_value(DATE_FORMAT), "Detail level syntax.")
		("debug", po::bool_switch(&args->debug), "Enable debug information.")
		;

	nscapi::program_options::legacy::add_numerical_all(desc);
	nscapi::program_options::legacy::add_exact_numerical_all(desc);
	nscapi::program_options::legacy::add_ignore_perf_data(desc, bPerfData);
	nscapi::program_options::legacy::add_show_all(desc);

	boost::program_options::variables_map vm;
	nscapi::program_options::unrecognized_map unrecognized;
	if (!nscapi::program_options::process_arguments_unrecognized(vm, unrecognized, desc, request, *response)) 
		return;
	nscapi::program_options::legacy::collect_numerical_all(vm, query1);
	nscapi::program_options::legacy::collect_exact_numerical_all(vm, query2);
	nscapi::program_options::legacy::collect_show_all(vm, query1);
	nscapi::program_options::legacy::collect_show_all(vm, query2);
	nscapi::program_options::alias_map aliases = nscapi::program_options::parse_legacy_alias(unrecognized, "Drive");

	tasksched_filter::filter_engine impl = tasksched_filter::factories::create_engine(args);
	if (!impl) {
		return nscapi::protobuf::functions::set_response_bad(*response, "Failed to initialize filter subsystem.");
	}
	impl->boot();
	NSC_DEBUG_MSG_STD(_T("Using: ") + impl->get_name() + _T(" ") + impl->get_subject());
	std::wstring tmp;
	if (!impl->validate(tmp)) {
		return nscapi::protobuf::functions::set_response_bad(*response, utf8::cvt<std::string>(tmp));
	}
	tasksched_filter::filter_result query_result = tasksched_filter::factories::create_result(args);

	try {
		TaskSched query;
		query.findAll(query_result, args, impl);
	} catch (TaskSched::Exception e) {
		return nscapi::protobuf::functions::set_response_bad(*response, utf8::cvt<std::string>(_T("WMIQuery failed: ") + e.getMessage()));
	}

	int count = query_result->get_match_count();
	std::wstring msg, perf;
	msg = query_result->render(syntax_top, returnCode);
	if (!bPerfData) {
		query1.perfData = false;
		query2.perfData = false;
	}
	if (query1.alias.empty())
		query1.alias = _T("eventlog");
	if (query2.alias.empty())
		query2.alias = _T("eventlog");
	if (query1.hasBounds())
		query1.runCheck(count, returnCode, msg, perf);
	else if (query2.hasBounds())
		query2.runCheck(count, returnCode, msg, perf);
	if ((truncate > 0) && (msg.length() > (truncate-4)))
		msg = msg.substr(0, truncate-4) + _T("...");
	if (msg.empty())
		msg = _T("OK: All scheduled tasks are good.");
	response->set_result(nscapi::protobuf::functions::nagios_status_to_gpb(returnCode));
	response->set_message(utf8::cvt<std::string>(msg));
}

int CheckTaskSched::commandLineExec(const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &result) {
// 	std::wstring query = command;
// 	query += _T(" ") + arrayBuffer::arrayBuffer2string(char_args, argLen, _T(" "));
// 	TaskSched::result_type rows;
// 	try {
// 		task_sched::result::fetch_key key(true);
// 		TaskSched wmiQuery;
// 		rows = wmiQuery.findAll(key);
// 	} catch (TaskSched::Exception e) {
// 		NSC_LOG_ERROR_STD(_T("TaskSched failed: ") + e.getMessage());
// 		return -1;
// 	} catch (...) {
// 		NSC_LOG_ERROR_STD(_T("TaskSched failed: UNKNOWN"));
// 		return -1;
// 	}
// 	for (TaskSched::result_type::const_iterator cit = rows.begin(); cit != rows.end(); ++cit) {
// 		std::wcout << (*cit).render(syntax) << std::endl;
// 	}
	return 0;
}
