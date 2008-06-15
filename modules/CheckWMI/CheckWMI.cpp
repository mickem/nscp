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
#include <vector>


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


bool CheckWMI::loadModule(NSCAPI::moduleLoadMode mode) {
	try {
		NSCModuleHelper::registerCommand(_T("CheckWMIValue"), _T("Run a WMI query and check the resulting value (the values of each row determin the state)."));
		NSCModuleHelper::registerCommand(_T("CheckWMI"), _T("Run a WMI query and check the resulting rows (the number of hits determine state)."));
	} catch (NSCModuleHelper::NSCMHExcpetion &e) {
		NSC_LOG_ERROR_STD(_T("Failed to register command: ") + e.msg_);
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to register command."));
	}
	return true;
}
bool CheckWMI::unloadModule() {
	return true;
}

bool CheckWMI::hasCommandHandler() {
	return true;
}
bool CheckWMI::hasMessageHandler() {
	return false;
}


#define MAP_CHAINED_FILTER(value, obj) \
			else if (p__.first.length() > 8 && p__.first.substr(1,6) == _T("filter") && p__.first.substr(7,1) == _T("-") && p__.first.substr(8) == value) { \
			WMIQuery::wmi_filter filter; filter.obj = p__.second; chain.push_filter(p__.first, filter); }

#define MAP_SECONDARY_CHAINED_FILTER(value, obj) \
			else if (p2.first.length() > 8 && p2.first.substr(1,6) == _T("filter") && p2.first.substr(7,1) == _T("-") && p2.first.substr(8) == value) { \
			WMIQuery::wmi_filter filter; filter.obj = p__.second; filter.alias = p2.second; chain.push_filter(p__.first, filter); }

#define MAP_CHAINED_FILTER_STRING(value) \
	MAP_CHAINED_FILTER(value, string)

#define MAP_CHAINED_FILTER_NUMERIC(value) \
	MAP_CHAINED_FILTER(value, numeric)

NSCAPI::nagiosReturn CheckWMI::CheckSimpleWMI(const unsigned int argLen, TCHAR **char_args, std::wstring &message, std::wstring &perf) {
	typedef checkHolders::CheckConatiner<checkHolders::MaxMinBounds<checkHolders::NumericBounds<int, checkHolders::int_handler> > > WMIConatiner;

	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	typedef filters::chained_filter<WMIQuery::wmi_filter,WMIQuery::wmi_row> filter_chain;
	filter_chain chain;
	std::list<std::wstring> args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	if (args.empty()) {
		message = _T("Missing argument(s).");
		return NSCAPI::returnCRIT;
	}
	unsigned int truncate = 0;
	std::wstring query, alias;
	bool bPerfData = true;

	WMIConatiner result_query;
	try {
		MAP_OPTIONS_BEGIN(args)
		MAP_OPTIONS_STR(_T("Query"), query)
		MAP_OPTIONS_STR2INT(_T("truncate"), truncate)
		MAP_OPTIONS_STR(_T("Alias"), alias)
		MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
		MAP_OPTIONS_NUMERIC_ALL(result_query, _T(""))
		MAP_OPTIONS_SHOWALL(result_query)
		MAP_CHAINED_FILTER(_T("string"),string)
		MAP_CHAINED_FILTER(_T("numeric"),numeric)
		MAP_OPTIONS_SECONDARY_BEGIN(_T(":"), p2)
		MAP_SECONDARY_CHAINED_FILTER(_T("string"),string)
		MAP_SECONDARY_CHAINED_FILTER(_T("numeric"),numeric)
			else if (p2.first == _T("Query")) {
					query = p__.second;
					alias = p2.second;
				}
		MAP_OPTIONS_MISSING_EX(p2, message, _T("Unknown argument: "))
			MAP_OPTIONS_SECONDARY_END()
		MAP_OPTIONS_END()
	} catch (filters::parse_exception e) {
		message = _T("WMIQuery failed: ") + e.getMessage();
		return NSCAPI::returnCRIT;
	}

	WMIQuery::result_type rows;
	try {
		WMIQuery wmiQuery;
		rows = wmiQuery.execute(query);
	} catch (WMIException e) {
		message = _T("WMIQuery failed: ") + e.getMessage();
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
		message = message.substr(0, truncate-4) + _T("...");
	if (message.empty())
		message = _T("OK: WMI Query returned no results.");
	return returnCode;
}

NSCAPI::nagiosReturn CheckWMI::CheckSimpleWMIValue(const unsigned int argLen, TCHAR **char_args, std::wstring &message, std::wstring &perf) {
	typedef checkHolders::CheckConatiner<checkHolders::MaxMinBounds<checkHolders::NumericBounds<long long, checkHolders::int64_handler> > > WMIConatiner;
	std::list<std::wstring> stl_args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	if (stl_args.empty()) {
		message = _T("ERROR: Missing argument exception.");
		return NSCAPI::returnUNKNOWN;
	}
	std::list<WMIConatiner> list;
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	WMIConatiner tmpObject;
	bool bPerfData = true;
	unsigned int truncate = 0;
	std::wstring query;
	std::wstring aliasCol;

	// Query=Select ... MaxWarn=5 MaxCrit=12 Check=Col1 --(later)-- Match==test Check=Col2
	// MaxWarnNumeric:ID=>5
	try {
		MAP_OPTIONS_BEGIN(stl_args)
			MAP_OPTIONS_SHOWALL(tmpObject)
			MAP_OPTIONS_NUMERIC_ALL(tmpObject, _T(""))
			MAP_OPTIONS_STR(_T("Alias"), tmpObject.data)
			MAP_OPTIONS_STR(_T("AliasCol"), aliasCol)
			MAP_OPTIONS_STR(_T("Query"), query)
			MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
			MAP_OPTIONS_STR_AND(_T("Check"), tmpObject.data, list.push_back(tmpObject))
			MAP_OPTIONS_STR(_T("Alias"), tmpObject.data)
			MAP_OPTIONS_SECONDARY_BEGIN(_T(":"), p2)
			MAP_OPTIONS_SECONDARY_STR_AND(p2,_T("Check"), tmpObject.data, tmpObject.alias, list.push_back(tmpObject))
				MAP_OPTIONS_MISSING_EX(p2, message, _T("Unknown argument: "))
				MAP_OPTIONS_SECONDARY_END()
			MAP_OPTIONS_MISSING(message, _T("Unknown argument: "))
			MAP_OPTIONS_END()

	} catch (filters::parse_exception e) {
		message = _T("WMIQuery failed: ") + e.getMessage();
		return NSCAPI::returnCRIT;
	}

	WMIQuery::result_type rows;
	try {
		WMIQuery wmiQuery;
		rows = wmiQuery.execute(query);
	} catch (WMIException e) {
		message = _T("WMIQuery failed: ") + e.getMessage();
		return NSCAPI::returnCRIT;
	}
	int hit_count = 0;

	for (std::list<WMIConatiner>::const_iterator it = list.begin(); it != list.end(); ++it) {
		WMIConatiner itm = (*it);
		itm.setDefault(tmpObject);
		itm.perfData = bPerfData;
		if (itm.data == _T("*")) {
			for (WMIQuery::result_type::const_iterator citRow = rows.begin(); citRow != rows.end(); ++citRow) {
				for (WMIQuery::wmi_row::list_type::const_iterator citCol = (*citRow).results.begin(); citCol != (*citRow).results.end(); ++citCol) {
					long long value = (*citCol).second.numeric;
					itm.runCheck(value, returnCode, message, perf);
				}
			}
		}
	}
	for (WMIQuery::result_type::const_iterator citRow = rows.begin(); citRow != rows.end(); ++citRow) {
		bool found = false;
		std::wstring alias;
		if (!aliasCol.empty()) {
			alias = (*citRow).get(aliasCol).string;
		}
		for (WMIQuery::wmi_row::list_type::const_iterator citCol = (*citRow).results.begin(); citCol != (*citRow).results.end(); ++citCol) {
			for (std::list<WMIConatiner>::const_iterator it = list.begin(); it != list.end(); ++it) {
				WMIConatiner itm = (*it);
				if (itm.data == _T("*")) {
					found = true;
				} else if ((*citCol).first == itm.data) {
					std::wstring oldAlias = itm.alias;
					if (!alias.empty())
						itm.alias = alias + _T(" ") + itm.getAlias();
					found = true;
					long long value = (*citCol).second.numeric;
					itm.runCheck(value, returnCode, message, perf);
					itm.alias = oldAlias;
				}
			}
		}
		if (!found) {
			NSC_LOG_ERROR_STD(_T("At least one of the queried columns was not found!"));
		}
	}

	if ((truncate > 0) && (message.length() > (truncate-4)))
		message = message.substr(0, truncate-4) + _T("...");
	if (message.empty())
		message = _T("OK: WMI Query returned no results.");
	return returnCode;
}


NSCAPI::nagiosReturn CheckWMI::handleCommand(const strEx::blindstr command, const unsigned int argLen, TCHAR **char_args, std::wstring &msg, std::wstring &perf) {
	if (command == _T("CheckWMI")) {
		return CheckSimpleWMI(argLen, char_args, msg, perf);
	} else if (command == _T("CheckWMIValue")) {
		return CheckSimpleWMIValue(argLen, char_args, msg, perf);
	}	
	return NSCAPI::returnIgnored;
}
int CheckWMI::commandLineExec(const TCHAR* command, const unsigned int argLen, TCHAR** char_args) {
	//WMIQuery wmiQuery;
	std::wstring query = command;
	query += _T(" ") + arrayBuffer::arrayBuffer2string(char_args, argLen, _T(" "));
	WMIQuery::result_type rows;
	try {
		WMIQuery wmiQuery;
		rows = wmiQuery.execute(query);
	} catch (WMIException e) {
		NSC_LOG_ERROR_STD(_T("WMIQuery failed: ") + e.getMessage());
		return -1;
	}
	std::vector<std::wstring::size_type> widths;
	for (WMIQuery::result_type::const_iterator citRow = rows.begin(); citRow != rows.end(); ++citRow) {
		const WMIQuery::wmi_row vals = *citRow;
		if (citRow == rows.begin()) {
			for (WMIQuery::wmi_row::list_type::const_iterator citCol = vals.results.begin(); citCol != vals.results.end(); ++citCol) {
				widths.push_back( (*citCol).first.length()+1 );
			}
		}
		int i=0;
		for (WMIQuery::wmi_row::list_type::const_iterator citCol = vals.results.begin(); citCol != vals.results.end(); ++citCol, i++) {
			widths[i] = max(widths[i], (*citCol).second.string.length()+1);
		}
	}

	std::wstring row2 = _T("|");
	for (WMIQuery::result_type::iterator citRow = rows.begin(); citRow != rows.end(); ++citRow) {
		const WMIQuery::wmi_row vals = *citRow;
		if (citRow == rows.begin()) {
			int i=0;
			std::wstring row1 = _T("|");
			for (WMIQuery::wmi_row::list_type::const_iterator citCol = vals.results.begin(); citCol != vals.results.end(); ++citCol, i++) {
				std::wstring::size_type w = widths[i]-(*citCol).first.length();
				if (w<0) w=0;
				row1 += std::wstring(w, ' ') + (*citCol).first + _T(" |");
				row2 += std::wstring(widths[i], '-') + _T("-+");

			}
			NSC_LOG_MESSAGE(row2);
			NSC_LOG_MESSAGE(row1);
			NSC_LOG_MESSAGE(row2);
		}
		int i=0;
		std::wstring row = _T("|");
		for (WMIQuery::wmi_row::list_type::const_iterator citCol = vals.results.begin(); citCol != vals.results.end(); ++citCol, i++) {
			std::wstring::size_type w = widths[i]-(*citCol).second.string.length();
			if (w<0) w=0;
			row += std::wstring(w, ' ') + (*citCol).second.string + _T(" |");
		}
		NSC_LOG_MESSAGE(row);
	}
	NSC_LOG_MESSAGE(row2);
	return 0;
}


NSC_WRAPPERS_MAIN_DEF(gCheckWMI);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gCheckWMI);
NSC_WRAPPERS_CLI_DEF(gCheckWMI);
