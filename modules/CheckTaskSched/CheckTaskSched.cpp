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
#include "CheckTaskSched.h"
#include <strEx.h>
#include <time.h>
#include <map>
#include <vector>


CheckTaskSched gCheckTaskSched;

BOOL APIENTRY DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	NSCModuleWrapper::wrapDllMain(hModule, ul_reason_for_call);
	return TRUE;
}

bool CheckTaskSched::loadModule(NSCAPI::moduleLoadMode mode) {
	try {
		NSCModuleHelper::registerCommand(_T("CheckTaskSchedValue"), _T("Run a WMI query and check the resulting value (the values of each row determin the state)."));
		NSCModuleHelper::registerCommand(_T("CheckTaskSched"), _T("Run a WMI query and check the resulting rows (the number of hits determine state)."));

		SETTINGS_REG_PATH(task_scheduler::SECTION);
		SETTINGS_REG_KEY_S(task_scheduler::SYNTAX);
	} catch (NSCModuleHelper::NSCMHExcpetion &e) {
		NSC_LOG_ERROR_STD(_T("Failed to register command: ") + e.msg_);
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to register command."));
	}
	syntax = SETTINGS_GET_STRING(task_scheduler::SYNTAX);
	return true;
}
bool CheckTaskSched::unloadModule() {
	return true;
}

bool CheckTaskSched::hasCommandHandler() {
	return true;
}
bool CheckTaskSched::hasMessageHandler() {
	return false;
}


#define MAP_CHAINED_FILTER(key) MAP_CHAINED_FILTER_EX(key, _T(#key) )

#define MAP_CHAINED_FILTER_EX(key, val) \
			else if (p__.first.length() > 8 && p__.first.substr(1,6) == _T("filter") && p__.first.substr(7,1) == _T("-") && p__.first.substr(8) == val) { \
			TaskSched::wmi_filter filter; filter.key = p__.second; chain.push_filter(p__.first, filter); }

#define MAP_CHAINED_FILTER_ALIAS(key) MAP_CHAINED_FILTER_EX(key, _T(key ## _alias))

#define MAP_CHAINED_FILTER_STRING(value) \
	MAP_CHAINED_FILTER(value, string)

#define MAP_CHAINED_FILTER_NUMERIC(value) \
	MAP_CHAINED_FILTER(value, numeric)


NSCAPI::nagiosReturn CheckTaskSched::TaskSchedule(const unsigned int argLen, TCHAR **char_args, std::wstring &message, std::wstring &perf) {
	typedef checkHolders::CheckConatiner<checkHolders::MaxMinBounds<checkHolders::NumericBounds<int, checkHolders::int_handler> > > WMIConatiner;

	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	typedef filters::chained_filter<TaskSched::wmi_filter,TaskSched::result> filter_chain;
	filter_chain chain;
	std::list<std::wstring> args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	if (args.empty()) {
		message = _T("Missing argument(s).");
		return NSCAPI::returnCRIT;
	}
	unsigned int truncate = 0;
	std::wstring query, alias;
	bool bPerfData = true;
	bool bDebug = false;

	WMIConatiner result_query;
	try {
		MAP_OPTIONS_BEGIN(args)
			MAP_OPTIONS_STR2INT(_T("truncate"), truncate)
			MAP_OPTIONS_STR(_T("Alias"), alias)
			MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
			MAP_OPTIONS_BOOL_TRUE(_T("debug"), bDebug)
			MAP_OPTIONS_NUMERIC_ALL(result_query, _T(""))
			MAP_OPTIONS_SHOWALL(result_query)


			MAP_CHAINED_FILTER(accountName)
		MAP_CHAINED_FILTER_ALIAS(applicationName)
		MAP_CHAINED_FILTER(comment)
		MAP_CHAINED_FILTER(creator)
		//FETCH_TASK_SIMPLE_DWORD(errorRetryCount, GetErrorRetryCount, 0);
		//FETCH_TASK_SIMPLE_DWORD(errorRetryInterval, GetErrorRetryInterval, 0);
		MAP_CHAINED_FILTER_ALIAS(exitCode)
		MAP_CHAINED_FILTER(flags)
		//FETCH_TASK_SIMPLE_DWORD(flags, GetIdleWait, 0)
		MAP_CHAINED_FILTER(flags)
		MAP_CHAINED_FILTER_ALIAS(mostRecentRunTime)
		MAP_CHAINED_FILTER_ALIAS(nextRunTime)

		MAP_CHAINED_FILTER(parameters)
		//MAP_CHAINED_FILTER(priority)
		//MAP_CHAINED_FILTER(status)
		// Trigger
		MAP_CHAINED_FILTER_ALIAS(workingDirectory)

//			MAP_CHAINED_FILTER(_T("numeric"),numeric)
			MAP_OPTIONS_MISSING(message,_T("Invalid argument: "))
			MAP_OPTIONS_END()
	} catch (filters::parse_exception e) {
		message = _T("TaskSched failed: ") + e.getMessage();
		return NSCAPI::returnCRIT;
	}

	if (bDebug)
		NSC_DEBUG_MSG_STD(_T("Filters: ") + chain.debug());
	TaskSched::result::fetch_key key(true);
	TaskSched::result_type rows;
	try {
		TaskSched wmiQuery;
		rows = wmiQuery.findAll(key);
	} catch (TaskSched::Exception e) {
		message = _T("WMIQuery failed: ") + e.getMessage();
		return NSCAPI::returnCRIT;
	}
	int hit_count = 0;

	bool match = chain.get_inital_state();
	for (TaskSched::result_type::iterator citRow = rows.begin(); citRow != rows.end(); ++citRow) {
		match = chain.match(match, *citRow);
		if (match) {
			strEx::append_list(message, (*citRow).render(syntax));
			hit_count++;
		}
	}

	if (!bPerfData)
		result_query.perfData = false;
	result_query.runCheck(hit_count, returnCode, message, perf);
	if ((truncate > 0) && (message.length() > (truncate-4)))
		message = message.substr(0, truncate-4) + _T("...");
	if (message.empty())
		message = _T("OK: All scheduled tasks are good.");
	return returnCode;
}

NSCAPI::nagiosReturn CheckTaskSched::handleCommand(const strEx::blindstr command, const unsigned int argLen, TCHAR **char_args, std::wstring &msg, std::wstring &perf) {
	if (command == _T("CheckTaskSched"))
		return TaskSchedule(argLen, char_args, msg, perf);
	return NSCAPI::returnIgnored;
}
int CheckTaskSched::commandLineExec(const TCHAR* command, const unsigned int argLen, TCHAR** char_args) {
	std::wstring query = command;
	query += _T(" ") + arrayBuffer::arrayBuffer2string(char_args, argLen, _T(" "));
	TaskSched::result_type rows;
	try {
		TaskSched::result::fetch_key key(true);
		TaskSched wmiQuery;
		rows = wmiQuery.findAll(key);
	} catch (TaskSched::Exception e) {
		NSC_LOG_ERROR_STD(_T("TaskSched failed: ") + e.getMessage());
		return -1;
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("TaskSched failed: UNKNOWN"));
		return -1;
	}
	for (TaskSched::result_type::const_iterator cit = rows.begin(); cit != rows.end(); ++cit) {
		std::wcout << (*cit).render(syntax) << std::endl;
	}
	return 0;
}


NSC_WRAPPERS_MAIN_DEF(gCheckTaskSched);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gCheckTaskSched);
NSC_WRAPPERS_CLI_DEF(gCheckTaskSched);
