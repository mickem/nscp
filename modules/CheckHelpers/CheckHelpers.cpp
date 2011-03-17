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
#include <vector>

#ifdef USE_BOOST
#include <program_options_ex.hpp>
#include <boost/program_options.hpp>
#include <boost/thread/thread.hpp>
#endif

CheckHelpers gCheckHelpers;

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
		NSCModuleHelper::registerCommand(_T("Negate"), _T("Change the returned status of another command"));
		NSCModuleHelper::registerCommand(_T("Timeout"), _T("Run another command with a timeout (in a background thread) and report a given status when it times out"));
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
NSCAPI::nagiosReturn CheckHelpers::checkSimpleStatus(NSCAPI::nagiosReturn status, const unsigned int argLen, TCHAR **char_args, std::wstring &message, std::wstring &perf) 
{
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	std::list<std::wstring> arguments = arrayBuffer::arrayBuffer2list(argLen, char_args);
	if (arguments.empty()) {
		message = NSCHelper::translateReturn(status) + _T(": Lets pretend everything is going to be ok.");
		return status;
	}
	std::list<std::wstring>::const_iterator cit;
	for (cit=arguments.begin();cit!=arguments.end();++cit)
		message += *cit;
	return status;
}

NSCAPI::nagiosReturn CheckHelpers::handleCommand(const strEx::blindstr command, const unsigned int argLen, TCHAR **char_args, std::wstring &message, std::wstring &perf) {
	if (command == _T("CheckVersion")) {
		message = NSCModuleHelper::getApplicationVersionString();
		return NSCAPI::returnOK;
	} else if (command == _T("CheckAlwaysOK")) {
		if (argLen < 2) {
			message = _T("ERROR: Missing arguments.");
			return NSCAPI::returnUNKNOWN;
		}
		NSCModuleHelper::InjectCommand(char_args[0], argLen-1, &char_args[1], message, perf);
		return NSCAPI::returnOK;
	} else if (command == _T("CheckAlwaysCRITICAL")) {
		if (argLen < 2) {
			message = _T("ERROR: Missing arguments.");
			return NSCAPI::returnUNKNOWN;
		}
		NSCModuleHelper::InjectCommand(char_args[0], argLen-1, &char_args[1], message, perf);
		return NSCAPI::returnCRIT;
	} else if (command == _T("CheckAlwaysWARNING")) {
		if (argLen < 2) {
			message = _T("ERROR: Missing arguments.");
			return NSCAPI::returnUNKNOWN;
		}
		NSCModuleHelper::InjectCommand(char_args[0], argLen-1, &char_args[1], message, perf);
		return NSCAPI::returnWARN;
	} else if (command == _T("CheckOK")) {
		return checkSimpleStatus(NSCAPI::returnOK, argLen, char_args, message, perf);
	} else if (command == _T("check_ok")) {
		return checkSimpleStatus(NSCAPI::returnOK, argLen, char_args, message, perf);
	} else if (command == _T("CheckWARNING")) {
		return checkSimpleStatus(NSCAPI::returnWARN, argLen, char_args, message, perf);
	} else if (command == _T("CheckCRITICAL")) {
		return checkSimpleStatus(NSCAPI::returnCRIT, argLen, char_args, message, perf);
	} else if (command == _T("CheckMultiple")) {
		return checkMultiple(argLen, char_args, message, perf);
	} else if (command == _T("Negate")) {
		return negate(argLen, char_args, message, perf);
	} else if (command == _T("Timeout")) {
		return timeout(argLen, char_args, message, perf);
	}
	return NSCAPI::returnIgnored;
}
NSCAPI::nagiosReturn CheckHelpers::checkMultiple(const unsigned int argLen, TCHAR **char_args, std::wstring &message, std::wstring &perf) 
{
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	std::list<std::wstring> arguments = arrayBuffer::arrayBuffer2list(argLen, char_args);
	if (arguments.empty()) {
		message = _T("Missing argument(s).");
		return NSCAPI::returnCRIT;
	}
	typedef std::pair<std::wstring, std::list<std::wstring> > sub_command;
	std::list<sub_command> commands;
	sub_command currentCommand;
	std::list<std::wstring>::const_iterator cit;
	for (cit=arguments.begin();cit!=arguments.end();++cit) {
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
		if (!message.empty())
			message += _T(", ");
		message += tMsg;
		perf += tPerf;
	}
	return returnCode;
}

NSCAPI::nagiosReturn CheckHelpers::negate(const unsigned int argLen, TCHAR **char_args, std::wstring &msg, std::wstring &perf) 
{
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	std::vector<std::wstring> arguments = arrayBuffer::arrayBuffer2vector(argLen, char_args);
	if (arguments.empty()) {
		msg = _T("Missing argument(s).");
		return NSCAPI::returnCRIT;
	}

	std::wstring command;
	NSCAPI::nagiosReturn OK = NSCAPI::returnOK, WARN = NSCAPI::returnWARN, CRIT = NSCAPI::returnCRIT, UNKNOWN = NSCAPI::returnUNKNOWN;
	std::vector<std::wstring> cmd_args;

	//#define USE_BOOST

	try {

#ifndef USE_BOOST
	msg = _T("Command not supported: Not compiled with boost");
	return NSCAPI::returnCRIT;
#else

		boost::program_options::options_description desc("Allowed options");
		desc.add_options()
			("help,h", "Show this help message.")

 			("ok,o",		boost::program_options::wvalue<std::wstring>(), "The state to return instead of OK")
 			("warning,w",	boost::program_options::wvalue<std::wstring>(), "The state to return instead of WARNING")
 			("critical,c",	boost::program_options::wvalue<std::wstring>(), "The state to return instead of CRITICAL")
 			("unknown,u",	boost::program_options::wvalue<std::wstring>(), "The state to return instead of UNKNOWN")

			("command,q",	boost::program_options::wvalue<std::wstring>(&command), "Wrapped command to execute")
   			("arguments,a",	boost::program_options::wvalue<std::vector<std::wstring> >(&cmd_args), "List of arguments (for wrapped command)")
			;

		boost::program_options::positional_options_description p;
		p.add("arguments", -1);

		boost::program_options::variables_map vm;
		boost::program_options::store(boost::program_options::basic_command_line_parser<wchar_t>(arguments).options(desc).positional(p).run(), vm);
		boost::program_options::notify(vm); 

		if (vm.count("help")) {
			std::stringstream ss;
			desc.print(ss);
			msg = strEx::string_to_wstring(ss.str());
			return NSCAPI::returnUNKNOWN;
		}

		if (vm.count("ok"))
			OK = NSCHelper::translateReturn(vm["ok"].as<std::wstring>());
		if (vm.count("warning"))
			WARN = NSCHelper::translateReturn(vm["warning"].as<std::wstring>());
		if (vm.count("critical"))
			CRIT = NSCHelper::translateReturn(vm["critical"].as<std::wstring>());
		if (vm.count("unknown"))
			UNKNOWN = NSCHelper::translateReturn(vm["unknown"].as<std::wstring>());

#endif


	} catch (std::exception &e) {
		msg = _T("Could not parse command: ") + strEx::string_to_wstring(e.what());
		return NSCAPI::returnCRIT;
	} catch (...) {
		msg = _T("Could not parse command: <UNKNOWN EXCEPTION>");
		return NSCAPI::returnCRIT;
	}
	std::list<std::wstring> cmd_args_l(cmd_args.begin(), cmd_args.end());

	NSCAPI::nagiosReturn tRet = NSCModuleHelper::InjectCommand(command.c_str(), cmd_args_l, msg, perf);
	switch (tRet) {
		case NSCAPI::returnOK:
			return OK;
		case NSCAPI::returnCRIT:
			return CRIT;
		case NSCAPI::returnWARN:
			return WARN;
		case NSCAPI::returnUNKNOWN:
			return UNKNOWN;
		default:
			return UNKNOWN;
	}
}


class worker {
public:
	void proc(std::wstring command, std::list<std::wstring> arguments) {
		code = NSCModuleHelper::InjectCommand(command.c_str(), arguments, msg, perf);
	}
	std::wstring msg;
	std::wstring perf;
	NSCAPI::nagiosReturn code;

};

NSCAPI::nagiosReturn CheckHelpers::timeout(const unsigned int argLen, TCHAR **char_args, std::wstring &msg, std::wstring &perf) 
{
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	std::vector<std::wstring> arguments = arrayBuffer::arrayBuffer2vector(argLen, char_args);
	if (arguments.empty()) {
		msg = _T("Missing argument(s).");
		return NSCAPI::returnCRIT;
	}

	std::wstring command;
	NSCAPI::nagiosReturn retCode = NSCAPI::returnUNKNOWN;
	std::vector<std::wstring> cmd_args;
	unsigned long timeout = 30;

	try {

#ifndef USE_BOOST
		msg = _T("Command not supported: Not compiled with boost");
		return NSCAPI::returnCRIT;
#else

		boost::program_options::options_description desc("Allowed options");
		desc.add_options()
			("help,h", "Show this help message.")

			("timeout,t",	boost::program_options::value<unsigned long>(&timeout), "The timeout value")

			("command,q",	boost::program_options::wvalue<std::wstring>(&command), "Wrapped command to execute")
			("arguments,a",	boost::program_options::wvalue<std::vector<std::wstring> >(&arguments), "List of arguments (for wrapped command)")
			;

		boost::program_options::positional_options_description p;
		p.add("arguments", -1);

		boost::program_options::variables_map vm;
		boost::program_options::store(boost::program_options::basic_command_line_parser<wchar_t>(cmd_args).options(desc).positional(p).run(), vm);
		boost::program_options::notify(vm); 

		if (vm.count("help")) {
			std::stringstream ss;
			desc.print(ss);
			msg = strEx::string_to_wstring(ss.str());
			return NSCAPI::returnUNKNOWN;
		}

		if (vm.count("return"))
			retCode = NSCHelper::translateReturn(vm["return"].as<std::wstring>());

#endif

	} catch (std::exception &e) {
		msg = _T("Could not parse command: ") + strEx::string_to_wstring(e.what());
		return NSCAPI::returnCRIT;
	} catch (...) {
		msg = _T("Could not parse command: <UNKNOWN EXCEPTION>");
		return NSCAPI::returnCRIT;
	}
	std::list<std::wstring> cmd_args_l(cmd_args.begin(), cmd_args.end());

#ifdef USE_BOOST

	worker obj;
	boost::shared_ptr<boost::thread> t = boost::shared_ptr<boost::thread>(new boost::thread(boost::bind(&worker::proc, obj, command, cmd_args_l)));

	if (t->timed_join(boost::posix_time::seconds(timeout))) {
		msg = obj.msg;
		perf = obj.perf;
		return obj.code;
	}
	t->detach();
	msg = _T("Thread failed to return within given timeout");
	return retCode;

#endif
}

NSC_WRAP_DLL();
NSC_WRAPPERS_MAIN_DEF(gCheckHelpers);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gCheckHelpers);
