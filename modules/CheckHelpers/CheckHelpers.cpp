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
#include <program_options_ex.hpp>
#include <boost/program_options.hpp>
#include <boost/thread/thread.hpp>

#include <nscapi/nscapi_core_helper.hpp>

#include <settings/client/settings_client.hpp>

CheckHelpers::CheckHelpers() {
}
CheckHelpers::~CheckHelpers() {
}


bool CheckHelpers::loadModule() {
	return false;
}

bool CheckHelpers::loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode) {
	try {
		register_command(_T("CheckAlwaysOK"), _T("Run another check and regardless of its return code return OK."));
		register_command(_T("CheckAlwaysCRITICAL"), _T("Run another check and regardless of its return code return CRIT."));
		register_command(_T("CheckAlwaysWARNING"), _T("Run another check and regardless of its return code return WARN."));
		register_command(_T("CheckMultiple"), _T("Run more then one check and return the worst state."));
		register_command(_T("CheckOK"), _T("Just return OK (anything passed along will be used as a message)."));
		register_command(_T("check_ok"), _T("Just return OK (anything passed along will be used as a message)."));
		register_command(_T("CheckWARNING"), _T("Just return WARN (anything passed along will be used as a message)."));
		register_command(_T("CheckCRITICAL"), _T("Just return CRIT (anything passed along will be used as a message)."));
		register_command(_T("CheckVersion"), _T("Just return the nagios version (along with OK status)."));
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
NSCAPI::nagiosReturn CheckHelpers::checkSimpleStatus(NSCAPI::nagiosReturn status, const std::list<std::wstring> arguments, std::wstring &message, std::wstring &perf) 
{
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	if (arguments.empty()) {
		message = nscapi::plugin_helper::translateReturn(status) + _T(": Lets pretend everything is going to be ok.");
		return status;
	}
	std::list<std::wstring>::const_iterator cit;
	for (cit=arguments.begin();cit!=arguments.end();++cit)
		message += *cit;
	return status;
}

NSCAPI::nagiosReturn CheckHelpers::handleCommand(const std::wstring &target, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &message, std::wstring &perf) {
	if (command == _T("checkversion")) {
		message = GET_CORE()->getApplicationVersionString();
		return NSCAPI::returnOK;
	} else if (command == _T("checkalwaysok")) {
		if (arguments.size() < 1) {
			message = _T("ERROR: Missing arguments.");
			return NSCAPI::returnUNKNOWN;
		}
		std::wstring new_command = arguments.front(); arguments.pop_front();
		nscapi::core_helper::simple_query(new_command, arguments, message, perf);
		return NSCAPI::returnOK;
	} else if (command == _T("checkalwayscritical")) {
		if (arguments.size() < 1) {
			message = _T("ERROR: Missing arguments.");
			return NSCAPI::returnUNKNOWN;
		}
		std::wstring new_command = arguments.front(); arguments.pop_front();
		nscapi::core_helper::simple_query(new_command, arguments, message, perf);
		return NSCAPI::returnCRIT;
	} else if (command == _T("checkalwayswarning")) {
		if (arguments.size() < 1) {
			message = _T("ERROR: Missing arguments.");
			return NSCAPI::returnUNKNOWN;
		}
		std::wstring new_command = arguments.front(); arguments.pop_front();
		nscapi::core_helper::simple_query(new_command, arguments, message, perf);
		return NSCAPI::returnWARN;
	} else if (command == _T("checkok")) {
		return checkSimpleStatus(NSCAPI::returnOK, arguments, message, perf);
	} else if (command == _T("check_ok")) {
		return checkSimpleStatus(NSCAPI::returnOK, arguments, message, perf);
	} else if (command == _T("checkwarning")) {
		return checkSimpleStatus(NSCAPI::returnWARN, arguments, message, perf);
	} else if (command == _T("checkcritical")) {
		return checkSimpleStatus(NSCAPI::returnCRIT, arguments, message, perf);
	} else if (command == _T("checkmultiple")) {
		return checkMultiple(arguments, message, perf);
	} else if (command == _T("Negate")) {
		return negate(arguments, message, perf);
	} else if (command == _T("Timeout")) {
		return timeout(arguments, message, perf);
	}
	return NSCAPI::returnIgnored;
}
NSCAPI::nagiosReturn CheckHelpers::checkMultiple(const std::list<std::wstring> arguments, std::wstring &message, std::wstring &perf) 
{
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
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
		std::pair<std::wstring,std::wstring> p = strEx::split(arg,std::wstring(_T("=")));
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
		std::list<std::wstring> sub_args;
		std::wstring tMsg, tPerf;
		NSCAPI::nagiosReturn tRet = nscapi::core_helper::simple_query((*cit2).first.c_str(), (*cit2).second, tMsg, tPerf);
		returnCode = nscapi::plugin_helper::maxState(returnCode, tRet);
		if (!message.empty())
			message += _T(", ");
		message += tMsg;
		perf += tPerf;
	}
	return returnCode;
}

NSCAPI::nagiosReturn CheckHelpers::negate(std::list<std::wstring> arguments, std::wstring &msg, std::wstring &perf) 
{
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	if (arguments.empty()) {
		msg = _T("Missing argument(s).");
		return NSCAPI::returnCRIT;
	}

	std::wstring command;
	NSCAPI::nagiosReturn OK = NSCAPI::returnOK, WARN = NSCAPI::returnWARN, CRIT = NSCAPI::returnCRIT, UNKNOWN = NSCAPI::returnUNKNOWN;
	std::vector<std::wstring> cmd_args;

	//#define USE_BOOST

	try {


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

		std::vector<std::wstring> arg_list(arguments.begin(), arguments.end());

		boost::program_options::variables_map vm;
		boost::program_options::store(boost::program_options::basic_command_line_parser<wchar_t>(arg_list).options(desc).positional(p).run(), vm);
		boost::program_options::notify(vm); 

		if (vm.count("help")) {
			std::stringstream ss;
			desc.print(ss);
			msg = strEx::string_to_wstring(ss.str());
			return NSCAPI::returnUNKNOWN;
		}

		if (vm.count("ok"))
			OK = nscapi::plugin_helper::translateReturn(vm["ok"].as<std::wstring>());
		if (vm.count("warning"))
			WARN = nscapi::plugin_helper::translateReturn(vm["warning"].as<std::wstring>());
		if (vm.count("critical"))
			CRIT = nscapi::plugin_helper::translateReturn(vm["critical"].as<std::wstring>());
		if (vm.count("unknown"))
			UNKNOWN = nscapi::plugin_helper::translateReturn(vm["unknown"].as<std::wstring>());

	} catch (std::exception &e) {
		msg = _T("Could not parse command: ") + strEx::string_to_wstring(e.what());
		return NSCAPI::returnCRIT;
	} catch (...) {
		msg = _T("Could not parse command: <UNKNOWN EXCEPTION>");
		return NSCAPI::returnCRIT;
	}
	std::list<std::wstring> cmd_args_l(cmd_args.begin(), cmd_args.end());

	NSCAPI::nagiosReturn tRet = nscapi::core_helper::simple_query(command, cmd_args_l, msg, perf);
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
		code = nscapi::core_helper::simple_query(command, arguments, msg, perf);
	}
	std::wstring msg;
	std::wstring perf;
	NSCAPI::nagiosReturn code;

};

NSCAPI::nagiosReturn CheckHelpers::timeout(std::list<std::wstring> arguments, std::wstring &msg, std::wstring &perf) 
{
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	if (arguments.empty()) {
		msg = _T("Missing argument(s).");
		return NSCAPI::returnCRIT;
	}

	std::wstring command;
	NSCAPI::nagiosReturn retCode = NSCAPI::returnUNKNOWN;
	std::vector<std::wstring> cmd_args;
	unsigned long timeout = 30;

	try {

		boost::program_options::options_description desc("Allowed options");
		desc.add_options()
			("help,h", "Show this help message.")

			("timeout,t",	boost::program_options::value<unsigned long>(&timeout), "The timeout value")

			("command,q",	boost::program_options::wvalue<std::wstring>(&command), "Wrapped command to execute")
			("arguments,a",	boost::program_options::wvalue<std::vector<std::wstring> >(&cmd_args), "List of arguments (for wrapped command)")
			;

		boost::program_options::positional_options_description p;
		p.add("arguments", -1);

		std::vector<std::wstring> arg_list(arguments.begin(), arguments.end());

		boost::program_options::variables_map vm;
		boost::program_options::store(boost::program_options::basic_command_line_parser<wchar_t>(arg_list).options(desc).positional(p).run(), vm);
		boost::program_options::notify(vm); 

		if (vm.count("help")) {
			std::stringstream ss;
			desc.print(ss);
			msg = strEx::string_to_wstring(ss.str());
			return NSCAPI::returnUNKNOWN;
		}

		if (vm.count("return"))
			retCode = nscapi::plugin_helper::translateReturn(vm["return"].as<std::wstring>());

	} catch (std::exception &e) {
		msg = _T("Could not parse command: ") + strEx::string_to_wstring(e.what());
		return NSCAPI::returnCRIT;
	} catch (...) {
		msg = _T("Could not parse command: <UNKNOWN EXCEPTION>");
		return NSCAPI::returnCRIT;
	}
	std::list<std::wstring> cmd_args_l(cmd_args.begin(), cmd_args.end());

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
}

NSC_WRAP_DLL();
NSC_WRAPPERS_MAIN_DEF(CheckHelpers);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF();
