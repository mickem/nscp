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
#include "CheckHelpers.h"
#include <strEx.h>
#include <time.h>
#include <utils.h>

CheckHelpers gCheckHelpers;

BOOL APIENTRY DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	NSCModuleWrapper::wrapDllMain(hModule, ul_reason_for_call);
	return TRUE;
}

CheckHelpers::CheckHelpers() {
}
CheckHelpers::~CheckHelpers() {
}


bool CheckHelpers::loadModule() {
	try {
		NSCModuleHelper::registerCommand(_T("CheckAlwaysOK"), _T("Run another check and regardless of its return code return OK."));
		NSCModuleHelper::registerCommand(_T("CheckAlwaysCRITICAL"), _T("Run another check and regardless of its return code return CRIT."));
		NSCModuleHelper::registerCommand(_T("CheckAlwaysWARNING"), _T("Run another check and regardless of its return code return WARN."));
		NSCModuleHelper::registerCommand(_T("CheckMultiple"), _T("Run more then one check and return the worst state."));
		NSCModuleHelper::registerCommand(_T("CheckOK"), _T("Just return OK (anything passed along will be used as a message)."));
		NSCModuleHelper::registerCommand(_T("CheckWARNING"), _T("Just return WARN (anything passed along will be used as a message)."));
		NSCModuleHelper::registerCommand(_T("CheckCRITICAL"), _T("Just return CRIT (anything passed along will be used as a message)."));
		NSCModuleHelper::registerCommand(_T("CheckVersion"), _T("Just return the nagios version (along with OK status)."));
	} catch (NSCModuleHelper::NSCMHExcpetion &e) {
		NSC_LOG_ERROR_STD(_T("Failed to register command: ") + e.msg_);
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to register command."));
	}
	return true;
}
bool CheckHelpers::unloadModule() {
	return true;
}

bool CheckHelpers::hasCommandHandler() {
	return true;
}
bool CheckHelpers::hasMessageHandler() {
	return false;
}
NSCAPI::nagiosReturn CheckHelpers::checkSimpleStatus(NSCAPI::nagiosReturn status, const unsigned int argLen, TCHAR **char_args, std::wstring &msg, std::wstring &perf) 
{
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	std::list<std::wstring> args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	if (args.empty()) {
		msg = NSCHelper::translateReturn(status) + _T(": Lets pretend everything is going to be ok.");
		return status;
	}
	std::list<std::wstring>::const_iterator cit;
	for (cit=args.begin();cit!=args.end();++cit)
		msg += *cit;
	return status;
}

NSCAPI::nagiosReturn CheckHelpers::handleCommand(const strEx::blindstr command, const unsigned int argLen, TCHAR **char_args, std::wstring &msg, std::wstring &perf) {
	if (command == _T("CheckAlwaysOK")) {
		if (argLen < 2) {
			msg = _T("ERROR: Missing arguments.");
			return NSCAPI::returnUNKNOWN;
		}
		NSCModuleHelper::InjectCommand(char_args[0], argLen-1, &char_args[1], msg, perf);
		return NSCAPI::returnOK;
	} else if (command == _T("CheckVersion")) {
		msg = NSCModuleHelper::getApplicationVersionString();
		return NSCAPI::returnOK;
	} else if (command == _T("CheckOK")) {
		return checkSimpleStatus(NSCAPI::returnOK, argLen, char_args, msg, perf);
	} else if (command == _T("CheckWARNING")) {
		return checkSimpleStatus(NSCAPI::returnWARN, argLen, char_args, msg, perf);
	} else if (command == _T("CheckCRITICAL")) {
		return checkSimpleStatus(NSCAPI::returnCRIT, argLen, char_args, msg, perf);
	} else if (command == _T("CheckAlwaysCRITICAL")) {
		if (argLen < 2) {
			msg = _T("ERROR: Missing arguments.");
			return NSCAPI::returnUNKNOWN;
		}
		NSCModuleHelper::InjectCommand(char_args[0], argLen-1, &char_args[1], msg, perf);
		return NSCAPI::returnCRIT;
	} else if (command == _T("CheckAlwaysWARNING")) {
		if (argLen < 2) {
			msg = _T("ERROR: Missing arguments.");
			return NSCAPI::returnUNKNOWN;
		}
		NSCModuleHelper::InjectCommand(char_args[0], argLen-1, &char_args[1], msg, perf);
		return NSCAPI::returnWARN;
	} else if (command == _T("CheckMultiple")) {
		return checkMultiple(argLen, char_args, msg, perf);
	}
	return NSCAPI::returnIgnored;
}
NSCAPI::nagiosReturn CheckHelpers::checkMultiple(const unsigned int argLen, TCHAR **char_args, std::wstring &msg, std::wstring &perf) 
{
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	std::list<std::wstring> args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	if (args.empty()) {
		msg = _T("Missing argument(s).");
		return NSCAPI::returnCRIT;
	}
	typedef std::pair<std::wstring, std::list<std::wstring> > sub_command;
	std::list<sub_command> commands;
	sub_command currentCommand;
	std::list<std::wstring>::const_iterator cit;
	for (cit=args.begin();cit!=args.end();++cit) {
		std::wstring arg = *cit;
		std::pair<std::wstring,std::wstring> p = strEx::split(arg,_T("="));
		if (p.first == _T("command")) {
			if (!currentCommand.first.empty())
				commands.push_back(currentCommand);
			currentCommand.first = p.second;
			currentCommand.second.clear();
		} else {
			currentCommand.second.push_back(*cit);
		}
	}
	if (!currentCommand.first.empty())
		commands.push_back(currentCommand);
	std::list<sub_command>::iterator cit2;
	for (cit2 = commands.begin(); cit2 != commands.end(); ++cit2) {
		unsigned int length = 0;
		TCHAR ** args = arrayBuffer::list2arrayBuffer((*cit2).second, length);
		std::wstring tMsg, tPerf;
		NSCAPI::nagiosReturn tRet = NSCModuleHelper::InjectCommand((*cit2).first.c_str(), length, args, tMsg, tPerf);
		arrayBuffer::destroyArrayBuffer(args, length);
		returnCode = NSCHelper::maxState(returnCode, tRet);
		if (!msg.empty())
			msg += _T(", ");
		msg += tMsg;
		perf += tPerf;
	}
	return returnCode;
}


NSC_WRAPPERS_MAIN_DEF(gCheckHelpers);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gCheckHelpers);
