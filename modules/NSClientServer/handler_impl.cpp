/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include "handler_impl.hpp"


#define REQ_CLIENTVERSION	1	// Works fine!
#define REQ_CPULOAD			2	// Quirks
#define REQ_UPTIME			3	// Works fine!
#define REQ_USEDDISKSPACE	4	// Works fine!
#define REQ_SERVICESTATE	5	// Works fine!
#define REQ_PROCSTATE		6	// Works fine!
#define REQ_MEMUSE			7	// Works fine!
#define REQ_COUNTER			8	// Works fine!
#define REQ_FILEAGE			9	// Works fine! (i hope)
#define REQ_INSTANCES		10	// Works fine! (i hope)

bool handler_impl::isPasswordOk(std::wstring remotePassword)  {
	std::wstring localPassword = get_password();
	if (localPassword == remotePassword) {
		return true;
	}
	if ((remotePassword == _T("None")) && (localPassword.empty())) {
		return true;
	}
	return false;
}

void split_to_list(std::list<std::wstring> &list, std::wstring str) {
	strEx::splitList add = strEx::splitEx(str, _T("&"));
	list.insert(list.begin(), add.begin(), add.end());
}

check_nt::packet handler_impl::handle(check_nt::packet p) {
	std::wstring buffer = p.get_payload();
	NSC_DEBUG_MSG_STD(_T("Data: ") + buffer);

	std::wstring::size_type pos = buffer.find_first_of(_T("\n\r"));
	if (pos != std::wstring::npos) {
		std::wstring::size_type pos2 = buffer.find_first_not_of(_T("\n\r"), pos);
		if (pos2 != std::wstring::npos) {
			std::wstring rest = buffer.substr(pos2);
			NSC_DEBUG_MSG_STD(_T("Ignoring data: ") + rest);
		}
		buffer = buffer.substr(0, pos);
	}

	strEx::token pwd = strEx::getToken(buffer, '&');
	if (!isPasswordOk(pwd.first)) {
		NSC_LOG_ERROR_STD(_T("Invalid password (") + pwd.first + _T(")."));
		return check_nt::packet("ERROR: Invalid password.");
	}
	if (pwd.second.empty())
		return check_nt::packet("ERROR: No command specified.");
	strEx::token cmd = strEx::getToken(pwd.second, '&');
	if (cmd.first.empty())
		return check_nt::packet("ERROR: No command specified.");

	int c = boost::lexical_cast<int>(cmd.first.c_str());

	NSC_DEBUG_MSG_STD(_T("Data: ") + cmd.second);

	std::list<std::wstring> args;

	// prefix various commands
	switch (c) {
		case REQ_CPULOAD:
			cmd.first = _T("checkCPU");
			split_to_list(args, cmd.second);
			args.push_back(_T("nsclient"));
			break;
		case REQ_UPTIME:
			cmd.first = _T("checkUpTime");
			args.push_back(_T("nsclient"));
			break;
		case REQ_USEDDISKSPACE:
			cmd.first = _T("CheckDriveSize");
			split_to_list(args, cmd.second);
			args.push_back(_T("nsclient"));
			break;
		case REQ_CLIENTVERSION:
			{
				//std::wstring v = SETTINGS_GET_STRING(nsclient::VERSION);
				//if (v == _T("auto"))
				std::wstring v = nscapi::plugin_singleton->get_core()->getApplicationName() + _T(" ") + nscapi::plugin_singleton->get_core()->getApplicationVersionString();
				return strEx::wstring_to_string(v);
			}
		case REQ_SERVICESTATE:
			cmd.first = _T("checkServiceState");
			split_to_list(args, cmd.second);
			args.push_back(_T("nsclient"));
			break;
		case REQ_PROCSTATE:
			cmd.first = _T("checkProcState");
			split_to_list(args, cmd.second);
			args.push_back(_T("nsclient"));
			break;
		case REQ_MEMUSE:
			cmd.first = _T("checkMem");
			args.push_back(_T("nsclient"));
			break;
		case REQ_COUNTER:
			cmd.first = _T("checkCounter");
			args.push_back(_T("Counter=") + cmd.second);
			args.push_back(_T("nsclient"));
			break;
		case REQ_FILEAGE:
			cmd.first = _T("getFileAge");
			args.push_back(_T("path=") + cmd.second);
			break;
		case REQ_INSTANCES:
			cmd.first = _T("listCounterInstances");
			args.push_back(cmd.second);
			break;


		default:
			split_to_list(args, cmd.second);
	}

	std::wstring message, perf;
	NSCAPI::nagiosReturn ret = nscapi::core_helper::simple_query(cmd.first.c_str(), args, message, perf);
	if (!nscapi::plugin_helper::isNagiosReturnCode(ret)) {
		if (message.empty())
			return check_nt::packet("ERROR: Could not complete the request check log file for more information.");
		return check_nt::packet("ERROR: " + strEx::wstring_to_string(message));
	}
	switch (c) {
		case REQ_UPTIME:		// Some check_nt commands has no return code syntax
		case REQ_MEMUSE:
		case REQ_CPULOAD:
		case REQ_CLIENTVERSION:
		case REQ_USEDDISKSPACE:
		case REQ_COUNTER:
		case REQ_FILEAGE:
			return check_nt::packet(message);

		case REQ_SERVICESTATE:	// Some check_nt commands return the return code (coded as a string)
		case REQ_PROCSTATE:
			return check_nt::packet(strEx::itos(nscapi::plugin_helper::nagios2int(ret)) + _T("& ") + message);

		default:				// "New" check_nscp also returns performance data
			if (perf.empty())
				return check_nt::packet(nscapi::plugin_helper::translateReturn(ret) + _T("&") + message);
			return check_nt::packet(nscapi::plugin_helper::translateReturn(ret) + _T("&") + message + _T("&") + perf);
	}

	return check_nt::packet("FOO");
}




