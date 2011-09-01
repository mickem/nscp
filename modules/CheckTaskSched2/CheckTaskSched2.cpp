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
#include "CheckTaskSched2.h"
#include <strEx.h>
#include <time.h>
#include <map>
#include <vector>

#include "filter.hpp"

#include <parsers/where/unary_fun.hpp>
#include <parsers/where/list_value.hpp>
#include <parsers/where/binary_op.hpp>
#include <parsers/where/unary_op.hpp>
#include <parsers/where/variable.hpp>

#include <settings/client/settings_client.hpp>
#include "../CheckTaskSched/settings.hpp"

bool CheckTaskSched2::loadModule() {
	return false;
}
bool CheckTaskSched2::loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode) {
	try {
		get_core()->registerCommand(_T("CheckTaskSchedValue"), _T("Run a WMI query and check the resulting value (the values of each row determin the state)."));
		get_core()->registerCommand(_T("CheckTaskSched"), _T("Run a WMI query and check the resulting rows (the number of hits determine state)."));

		SETTINGS_REG_PATH(task_scheduler::SECTION);
		SETTINGS_REG_KEY_S(task_scheduler::SYNTAX);
	} catch (nscapi::nscapi_exception &e) {
		NSC_LOG_ERROR_STD(_T("Failed to register command: ") + utf8::cvt<std::wstring>(e.what()));
		return false;
	} catch (std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Exception: ") + utf8::cvt<std::wstring>(e.what()));
		return false;
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to register command."));
		return false;
	}
	syntax = SETTINGS_GET_STRING(task_scheduler::SYNTAX);
	return true;
}
bool CheckTaskSched2::unloadModule() {
	return true;
}

bool CheckTaskSched2::hasCommandHandler() {
	return true;
}
bool CheckTaskSched2::hasMessageHandler() {
	return false;
}




NSCAPI::nagiosReturn CheckTaskSched2::TaskSchedule(std::list<std::wstring> arguments, std::wstring &message, std::wstring &perf) {
	typedef checkHolders::CheckContainer<checkHolders::MaxMinBounds<checkHolders::NumericBounds<int, checkHolders::int_handler> > > WMIContainerQuery1;
	typedef checkHolders::CheckContainer<checkHolders::ExactBounds<checkHolders::NumericBounds<int, checkHolders::int_handler> > > WMIContainerQuery2;

	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	if (arguments.empty()) {
		message = _T("Missing argument(s).");
		return NSCAPI::returnCRIT;
	}
	unsigned int truncate = 0;
	std::wstring query, alias;
	bool bPerfData = true;
	std::wstring masterSyntax = _T("%list%");
	tasksched_filter::filter_argument args = tasksched_filter::factories::create_argument(_T("%task%"), DATE_FORMAT);

	WMIContainerQuery1 query1;
	WMIContainerQuery2 query2;
	try {
		MAP_OPTIONS_BEGIN(arguments)
			MAP_OPTIONS_STR2INT(_T("truncate"), truncate)
			MAP_OPTIONS_STR(_T("Alias"), alias)
			MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
			MAP_OPTIONS_BOOL_TRUE(_T("debug"), args->debug)
			MAP_OPTIONS_STR(_T("date-syntax"), args->date_syntax)
			MAP_OPTIONS_NUMERIC_ALL(query1, _T(""))
			MAP_OPTIONS_EXACT_NUMERIC_ALL(query2, _T(""))
			//MAP_OPTIONS_SHOWALL(result_query)
			MAP_OPTIONS_STR(_T("master-syntax"), masterSyntax)
			MAP_OPTIONS_STR(_T("filter"), args->filter)
			MAP_OPTIONS_STR(_T("syntax"), args->syntax)
			MAP_OPTIONS_MISSING(message,_T("Invalid argument: "))
			MAP_OPTIONS_END()
	} catch (filters::parse_exception e) {
		message = _T("TaskSched failed: ") + e.getMessage();
		return NSCAPI::returnCRIT;
	}

	tasksched_filter::filter_engine impl = tasksched_filter::factories::create_engine(args);
	if (!impl) {
		message = _T("Failed to initialize filter subsystem.");
		return NSCAPI::returnUNKNOWN;
	}
	impl->boot();
	NSC_DEBUG_MSG_STD(_T("Using: ") + impl->get_name() + _T(" ") + impl->get_subject());
	if (!impl->validate(message)) {
		return NSCAPI::returnUNKNOWN;
	}
	tasksched_filter::filter_result result = tasksched_filter::factories::create_result(args);

	try {
		TaskSched query;
		query.findAll(result, args, impl);
	} catch (TaskSched::Exception e) {
		message = _T("WMIQuery failed: ") + e.getMessage();
		return NSCAPI::returnCRIT;
	}

	int count = result->get_match_count();
	message = result->render(masterSyntax, returnCode);
	if (!bPerfData) {
		query1.perfData = false;
		query2.perfData = false;
	}
	if (query1.alias.empty())
		query1.alias = _T("eventlog");
	if (query2.alias.empty())
		query2.alias = _T("eventlog");
	if (query1.hasBounds())
		query1.runCheck(count, returnCode, message, perf);
	else if (query2.hasBounds())
		query2.runCheck(count, returnCode, message, perf);
	if ((truncate > 0) && (message.length() > (truncate-4)))
		message = message.substr(0, truncate-4) + _T("...");
	if (message.empty())
		message = _T("OK: All scheduled tasks are good.");
	return returnCode;
}

NSCAPI::nagiosReturn CheckTaskSched2::handleCommand(const std::wstring &target, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &message, std::wstring &perf) {
	if (command == _T("checktasksched"))
		return TaskSchedule(arguments, message, perf);
	return NSCAPI::returnIgnored;
}
int CheckTaskSched2::commandLineExec(const wchar_t* command, const unsigned int argLen, wchar_t** char_args) {
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


NSC_WRAP_DLL();
NSC_WRAPPERS_MAIN_DEF(CheckTaskSched2);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(CheckTaskSched2);
//NSC_WRAPPERS_CLI_DEF(gCheckTaskSched);
