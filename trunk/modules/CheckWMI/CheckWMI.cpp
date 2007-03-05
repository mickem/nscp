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



NSCAPI::nagiosReturn CheckWMI::CheckSimpleWMI(const unsigned int argLen, char **char_args, std::string &message, std::string &perf) {
	typedef checkHolders::CheckConatiner<checkHolders::MaxMinBounds<checkHolders::NumericBounds<int, checkHolders::int_handler> > > WMIConatiner;

	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	std::list<std::string> args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	if (args.empty()) {
		message = "Missing argument(s).";
		return NSCAPI::returnCRIT;
	}

	WMIConatiner tmpObject;
	std::list<WMIConatiner> queries;

	MAP_OPTIONS_BEGIN(args)
		MAP_OPTIONS_STR_AND("Query", tmpObject.data, queries.push_back(tmpObject))
		MAP_OPTIONS_NUMERIC_ALL(tmpObject, "")
		MAP_OPTIONS_SHOWALL(tmpObject)
		MAP_OPTIONS_SECONDARY_BEGIN(":", p2)
			else if (p2.first == "Query") {
				tmpObject.data = p__.second;
				tmpObject.alias = p2.second;
				queries.push_back(tmpObject);
			}
			MAP_OPTIONS_MISSING_EX(p2, message, "Unknown argument: ")
		MAP_OPTIONS_SECONDARY_END()
	MAP_OPTIONS_FALLBACK_AND(tmpObject.data, queries.push_back(tmpObject))
	MAP_OPTIONS_END()

	for (std::list<WMIConatiner>::const_iterator pit = queries.begin();pit!=queries.end();++pit) {
		WMIConatiner query = (*pit);
		std::map<std::string,int> vals;
		try {
			vals = wmiQuery.execute(query.data);
		} catch (WMIException e) {
			message = "WMIQuery failed...";
			return NSCAPI::returnCRIT;
		}
		int val = 0; //(*vals.begin()).second;

		for (std::map<std::string,int>::const_iterator it = vals.begin(); it != vals.end(); ++it) {
			std::cout << "Values: " << (*it).first << " = " << (*it).second << std::endl;
		}

		query.setDefault(tmpObject);
		query.runCheck(val, returnCode, message, perf);
	}
	if (message.empty())
		message = "OK: Queries within bounds.";
	return returnCode;
}



NSCAPI::nagiosReturn CheckWMI::handleCommand(const strEx::blindstr command, const unsigned int argLen, char **char_args, std::string &msg, std::string &perf) {
	if (command == "CheckWMI") {
		return CheckSimpleWMI(argLen, char_args, msg, perf);
	}	
	return NSCAPI::returnIgnored;
}


NSC_WRAPPERS_MAIN_DEF(gCheckWMI);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gCheckWMI);
