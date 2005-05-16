// CheckEventLog.cpp : Defines the entry point for the DLL application.
//

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
	return true;
}
bool CheckHelpers::unloadModule() {
	return true;
}

std::string CheckHelpers::getModuleName() {
	return "CheckHelpers Various helper function to extend other checks.";
}
NSCModuleWrapper::module_version CheckHelpers::getModuleVersion() {
	NSCModuleWrapper::module_version version = {0, 3, 0 };
	return version;
}

bool CheckHelpers::hasCommandHandler() {
	return true;
}
bool CheckHelpers::hasMessageHandler() {
	return false;
}

NSCAPI::nagiosReturn CheckHelpers::handleCommand(const strEx::blindstr command, const unsigned int argLen, char **char_args, std::string &msg, std::string &perf) {
	if (command == "CheckAlwaysOK") {
		if (argLen < 2) {
			msg = "ERROR: Missing arguments.";
			return NSCAPI::returnUNKNOWN;
		}
		NSCModuleHelper::InjectCommand(char_args[0], argLen-1, &char_args[1], msg, perf);
		return NSCAPI::returnOK;
	} else if (command == "CheckAlwaysCRITICAL") {
		if (argLen < 2) {
			msg = "ERROR: Missing arguments.";
			return NSCAPI::returnUNKNOWN;
		}
		NSCModuleHelper::InjectCommand(char_args[0], argLen-1, &char_args[1], msg, perf);
		return NSCAPI::returnCRIT;
	} else if (command == "CheckAlwaysWARNING") {
		if (argLen < 2) {
			msg = "ERROR: Missing arguments.";
			return NSCAPI::returnUNKNOWN;
		}
		NSCModuleHelper::InjectCommand(char_args[0], argLen-1, &char_args[1], msg, perf);
		return NSCAPI::returnWARN;
	} else if (command == "CheckMultiple") {
		return checkMultiple(argLen, char_args, msg, perf);
	}
	return NSCAPI::returnIgnored;
}
NSCAPI::nagiosReturn CheckHelpers::checkMultiple(const unsigned int argLen, char **char_args, std::string &msg, std::string &perf) 
{
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	std::list<std::string> args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	if (args.empty()) {
		msg = "Missing argument(s).";
		return NSCAPI::returnCRIT;
	}
	typedef std::pair<std::string, std::list<std::string> > sub_command;
	std::list<sub_command> commands;
	sub_command currentCommand;
	std::list<std::string>::const_iterator cit;
	for (cit=args.begin();cit!=args.end();++cit) {
		std::string arg = *cit;
		std::pair<std::string,std::string> p = strEx::split(arg,"=");
		if (p.first == "command") {
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
		char ** args = arrayBuffer::list2arrayBuffer((*cit2).second, length);
		std::string tMsg, tPerf;
		NSCAPI::nagiosReturn tRet = NSCModuleHelper::InjectCommand((*cit2).first.c_str(), length, args, tMsg, tPerf);
		returnCode = NSCHelper::maxState(returnCode, tRet);
		if (!msg.empty())
			msg += ", ";
		msg += tMsg;
		perf += tPerf;
	}
	return returnCode;
}


NSC_WRAPPERS_MAIN_DEF(gCheckHelpers);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gCheckHelpers);
