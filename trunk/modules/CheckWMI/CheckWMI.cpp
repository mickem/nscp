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
#include "CheckWMI.h"
#include <strEx.h>
#include <time.h>
#include <map>


CheckWMI gCheckWMI;

BOOL APIENTRY DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	NSCModuleWrapper::wrapDllMain(hModule, ul_reason_for_call);
	return TRUE;
}

CheckWMI::CheckWMI() {
}
CheckWMI::~CheckWMI() {
}


bool CheckWMI::loadModule() {
	return wmiQuery.initialize();
}
bool CheckWMI::unloadModule() {
	wmiQuery.unInitialize();
	return true;
}

bool CheckWMI::hasCommandHandler() {
	return true;
}
bool CheckWMI::hasMessageHandler() {
	return false;
}


#define MAP_CHAINED_FILTER(value, obj) \
			else if (p__.first.length() > 8 && p__.first.substr(1,6) == "filter" && p__.first.substr(7,1) == "-" && p__.first.substr(8) == value) { \
				WMIQuery::wmi_filter filter; filter.obj = p__.second; chain.push_filter(p__.first, filter); }

#define MAP_SECONDARY_CHAINED_FILTER(value, obj) \
			else if (p2.first.length() > 8 && p2.first.substr(1,6) == "filter" && p2.first.substr(7,1) == "-" && p2.first.substr(8) == value) { \
			WMIQuery::wmi_filter filter; filter.obj = p__.second; filter.alias = p2.second; chain.push_filter(p__.first, filter); }

#define MAP_CHAINED_FILTER_STRING(value) \
	MAP_CHAINED_FILTER(value, string)

#define MAP_CHAINED_FILTER_NUMERIC(value) \
	MAP_CHAINED_FILTER(value, numeric)

NSCAPI::nagiosReturn CheckWMI::CheckSimpleWMI(const unsigned int argLen, char **char_args, std::string &message, std::string &perf) {
	typedef checkHolders::CheckConatiner<checkHolders::MaxMinBounds<checkHolders::NumericBounds<int, checkHolders::int_handler> > > WMIConatiner;

	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	typedef filters::chained_filter<WMIQuery::wmi_filter,WMIQuery::wmi_row> filter_chain;
	filter_chain chain;
	std::list<std::string> args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	if (args.empty()) {
		message = "Missing argument(s).";
		return NSCAPI::returnCRIT;
	}
	unsigned int truncate = 0;
	std::string query, alias;
	bool bPerfData = true;

	WMIConatiner result_query;
	try {
		MAP_OPTIONS_BEGIN(args)
		MAP_OPTIONS_STR("Query", query)
		MAP_OPTIONS_STR2INT("truncate", truncate)
		MAP_OPTIONS_STR("Alias", alias)
		MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
		MAP_OPTIONS_NUMERIC_ALL(result_query, "")
		MAP_OPTIONS_SHOWALL(result_query)
		MAP_CHAINED_FILTER("string",string)
		MAP_CHAINED_FILTER("numeric",numeric)
		MAP_OPTIONS_SECONDARY_BEGIN(":", p2)
		MAP_SECONDARY_CHAINED_FILTER("string",string)
		MAP_SECONDARY_CHAINED_FILTER("numeric",numeric)
				else if (p2.first == "Query") {
					query = p__.second;
					alias = p2.second;
				}
			MAP_OPTIONS_MISSING_EX(p2, message, "Unknown argument: ")
		MAP_OPTIONS_SECONDARY_END()
		MAP_OPTIONS_END()
	} catch (filters::parse_exception e) {
		message = "WMIQuery failed: " + e.getMessage();
		return NSCAPI::returnCRIT;
	}

	WMIQuery::result_type rows;
	try {
		rows = wmiQuery.execute(query);
	} catch (WMIException e) {
		message = "WMIQuery failed: " + e.getMessage();
		return NSCAPI::returnCRIT;
	}
	int hit_count = 0;

	bool match = chain.get_inital_state();
	for (WMIQuery::result_type::iterator citRow = rows.begin(); citRow != rows.end(); ++citRow) {
		WMIQuery::wmi_row vals = *citRow;
		match = chain.match(match, vals);
		if (match) {
			strEx::append_list(message, vals.render());
			hit_count++;
		}
	}

	if (!bPerfData)
		result_query.perfData = false;
	result_query.runCheck(hit_count, returnCode, message, perf);
	if ((truncate > 0) && (message.length() > (truncate-4)))
		message = message.substr(0, truncate-4) + "...";
	if (message.empty())
		message = "OK: WMI Query returned no results.";
	return returnCode;
}

NSCAPI::nagiosReturn CheckWMI::CheckSimpleWMIValue(const unsigned int argLen, char **char_args, std::string &message, std::string &perf) {
	message = "Not yet implemented :(";
	return NSCAPI::returnCRIT;
}


NSCAPI::nagiosReturn CheckWMI::handleCommand(const strEx::blindstr command, const unsigned int argLen, char **char_args, std::string &msg, std::string &perf) {
	if (command == "CheckWMI") {
		return CheckSimpleWMI(argLen, char_args, msg, perf);
	} else if (command == "CheckWMIValue") {
		return CheckSimpleWMIValue(argLen, char_args, msg, perf);
	}	
	return NSCAPI::returnIgnored;
}


NSC_WRAPPERS_MAIN_DEF(gCheckWMI);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gCheckWMI);
