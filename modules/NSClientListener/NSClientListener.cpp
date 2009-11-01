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
#include "NSClientListener.h"
#include <strEx.h>
#include <time.h>
#include <config.h>

NSClientListener gNSClientListener;


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

BOOL APIENTRY DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	NSCModuleWrapper::wrapDllMain(hModule, ul_reason_for_call);
	return TRUE;
}

NSClientListener::NSClientListener() {
}
NSClientListener::~NSClientListener() {
}
std::wstring getAllowedHosts() {
	std::wstring ret = SETTINGS_GET_STRING(nsclient::ALLOWED_HOSTS);
	if (ret.empty())
		ret = SETTINGS_GET_STRING(protocol_def::ALLOWED_HOSTS);
	return ret;
}
bool getCacheAllowedHosts() {
	return SETTINGS_GET_INT_FALLBACK(nsclient::CACHE_ALLOWED, protocol_def::CACHE_ALLOWED);
}

bool NSClientListener::loadModule(NSCAPI::moduleLoadMode mode) {
	SETTINGS_REG_PATH(nsclient::SECTION);
	SETTINGS_REG_KEY_I(nsclient::PORT);
	SETTINGS_REG_KEY_S(nsclient::VERSION);
	SETTINGS_REG_KEY_S(nsclient::BINDADDR);
	SETTINGS_REG_KEY_I(nsclient::LISTENQUE);
	SETTINGS_REG_KEY_I(nsclient::READ_TIMEOUT);

	allowedHosts.setAllowedHosts(strEx::splitEx(getAllowedHosts(), _T(",")), getCacheAllowedHosts());
	unsigned short port = SETTINGS_GET_INT(nsclient::PORT);
	std::wstring host = SETTINGS_GET_STRING(nsclient::BINDADDR);
	unsigned int backLog = SETTINGS_GET_INT(nsclient::LISTENQUE);
	socketTimeout_ = SETTINGS_GET_INT(nsclient::READ_TIMEOUT);
	if (mode == NSCAPI::normalStart) {
		try {
			socket.setHandler(this);
			socket.StartListener(host, port, backLog);
		} catch (simpleSocket::SocketException e) {
			NSC_LOG_ERROR_STD(_T("Exception caught: ") + e.getMessage());
			return false;
		}
	}
	return true;
}
bool NSClientListener::unloadModule() {
	try {
		socket.removeHandler(this);
		if (socket.hasListener())
			socket.StopListener();
	} catch (simpleSocket::SocketException e) {
		NSC_LOG_ERROR_STD(_T("Exception caught: ") + e.getMessage());
		return false;
	}
	return true;
}

/**
* Main command parser and delegator.
* This also handles a lot of the simpler responses (though some are deferred to other helper functions)
*
#define REQ_CLIENTVERSION	1	// Works fine!
#define REQ_CPULOAD		2	// Quirks
- Needs settings for default buffer size (backlog) and possibly other things.
- Buffer needs to be synced with the client (don't know the size of that)
- I think the idea was that the buffer would recursive to make a smaller memory footprint.
(ie. the first level buffer have samples from every second, next level samples from every minute, next level hours etc...)
(and I don't know the status of this, doesn't seem to be that way)
#define REQ_UPTIME			3	// Works fine!
#define REQ_USEDDISKSPACE	4	// Works fine!
#define REQ_SERVICESTATE	5	// Works fine!
#define REQ_PROCSTATE		6	// Works fine!
#define REQ_MEMUSE			7	// Works fine!
//#define REQ_COUNTER		8	// ! - not implemented Have to look at this, if anyone has a sample let me know...
//#define REQ_FILEAGE		9	// ! - not implemented Don't know how to use
//#define REQ_INSTANCES	10	// ! - not implemented Don't know how to use
*
*/


std::wstring getPassword() {
	static std::wstring password = _T("");
	if (password.empty()) {
		password = SETTINGS_GET_STRING(nsclient::OBFUSCATED_PWD);
		if (password.empty())
			password= SETTINGS_GET_STRING(protocol_def::OBFUSCATED_PWD);
		if (!password.empty()) {
			password = NSCModuleHelper::Decrypt(password);
		} else {
			password = SETTINGS_GET_STRING(nsclient::PWD);
			if (password.empty())
				password = SETTINGS_GET_STRING(protocol_def::PWD);
		}
	}
	return password;
}
bool NSClientListener::isPasswordOk(std::wstring remotePassword)  {
	std::wstring localPassword = getPassword();
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
std::string NSClientListener::parseRequest(std::string str_buffer)  {
	std::wstring buffer = strEx::string_to_wstring(str_buffer);
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
		return "ERROR: Invalid password.";
	}
	if (pwd.second.empty())
		return "ERROR: No command specified.";
	strEx::token cmd = strEx::getToken(pwd.second, '&');
	if (cmd.first.empty())
		return "ERROR: No command specified.";

	int c = _wtoi(cmd.first.c_str());

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
				std::wstring v = SETTINGS_GET_STRING(nsclient::VERSION);
				if (v == _T("auto"))
					v = NSCModuleHelper::getApplicationName() + _T(" ") + NSCModuleHelper::getApplicationVersionString();
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
	NSCAPI::nagiosReturn ret = NSCModuleHelper::InjectCommand(cmd.first.c_str(), args, message, perf);
	if (!NSCHelper::isNagiosReturnCode(ret)) {
		if (message.empty())
			return "ERROR: Could not complete the request check log file for more information.";
		return "ERROR: " + strEx::wstring_to_string(message);
	}
	switch (c) {
		case REQ_UPTIME:		// Some check_nt commands has no return code syntax
		case REQ_MEMUSE:
		case REQ_CPULOAD:
		case REQ_CLIENTVERSION:
		case REQ_USEDDISKSPACE:
		case REQ_COUNTER:
		case REQ_FILEAGE:
			return strEx::wstring_to_string(message);

		case REQ_SERVICESTATE:	// Some check_nt commands return the return code (coded as a string)
		case REQ_PROCSTATE:
			return strEx::wstring_to_string(strEx::itos(NSCHelper::nagios2int(ret)) + _T("& ") + message);

		default:				// "New" check_nscp also returns performance data
			if (perf.empty())
				return strEx::wstring_to_string(NSCHelper::translateReturn(ret) + _T("&") + message);
			return strEx::wstring_to_string(NSCHelper::translateReturn(ret) + _T("&") + message + _T("&") + perf);
	}
}

void NSClientListener::onClose()
{}

void NSClientListener::sendTheResponse(simpleSocket::Socket *client, std::string response) {
	client->send(response.c_str(), static_cast<int>(response.length()), 0);
}

void NSClientListener::retrivePacket(simpleSocket::Socket *client) {
	simpleSocket::DataBuffer db;
	unsigned int i;
	unsigned int maxWait = socketTimeout_*10;
	for (i=0;i<maxWait;i++) {
		bool lastReadHasMore = false;
		try {
			lastReadHasMore = client->readAll(db);
		} catch (simpleSocket::SocketException e) {
			NSC_LOG_ERROR_STD(_T("Read on socket failed: ") + e.getMessage());
			client->close();
			return;
		}
		if (db.getLength() > 0) {
			unsigned long long pos = db.find('\n');
			if (pos==-1) {
				std::string incoming(db.getBuffer(), db.getLength());
				sendTheResponse(client, parseRequest(incoming));
				break;
			} else if (pos > 0) {
				simpleSocket::DataBuffer buffer = db.unshift(static_cast<const unsigned int>(pos));
				std::string bstr(buffer.getBuffer(), buffer.getLength());
				db.nibble(1);
				std::string rstr(db.getBuffer(), db.getLength());
				std::string incoming(buffer.getBuffer(), buffer.getLength());
				sendTheResponse(client, parseRequest(incoming) + "\n");
			} else {
				db.nibble(1);
				NSC_LOG_ERROR_STD(_T("First char should (i think) not be a \\n :("));
			}
		} else if (!lastReadHasMore) {
			client->close();
			return;
		} else {
			Sleep(100);
		}
	}
	if (i >= maxWait) {
		NSC_LOG_ERROR_STD(_T("Timeout reading NS-client packet (increase socket_timeout)."));
		client->close();
		return;
	}
}


void NSClientListener::onAccept(simpleSocket::Socket *client) {
	if (!allowedHosts.inAllowedHosts(client->getAddr())) {
		NSC_LOG_ERROR(_T("Unauthorized access from: ") + client->getAddrString());
		client->close();
		return;
	}
	//client->setNonBlock();
	retrivePacket(client);



//	client->readAll(db);
//	if (db.getLength() > 0) {
//		std::wstring incoming(db.getBuffer(), db.getLength());
//		NSC_DEBUG_MSG_STD("Incoming data: " + incoming);
//		std::wstring response = parseRequest(incoming);
//		NSC_DEBUG_MSG("Outgoing data: " + response);
//		client->send(response.c_str(), static_cast<int>(response.length()), 0);
//	}
	client->close();
}

NSC_WRAPPERS_MAIN_DEF(gNSClientListener);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_IGNORE_CMD_DEF();
NSC_WRAPPERS_HANDLE_CONFIGURATION(gNSClientListener);


MODULE_SETTINGS_START(NSClientListener, _T("NSClient Listener configuration"), _T("..."))

PAGE(_T("NSClient Listener configuration"))

ITEM_EDIT_TEXT(_T("port"), _T("This is the port the NSClientListener.dll will listen to."))
ITEM_MAP_TO(_T("basic_ini_text_mapper"))
OPTION(_T("section"), _T("NSClient"))
OPTION(_T("key"), _T("port"))
OPTION(_T("default"), _T("12489"))
ITEM_END()

PAGE_END()
ADVANCED_PAGE(_T("Server settings"))

ITEM_EDIT_TEXT(_T("password"), _T("Enter the password used by applications when queriying for data."))
ITEM_MAP_TO(_T("basic_ini_text_mapper"))
OPTION(_T("section"), _T("NSClient"))
OPTION(_T("key"), _T("password"))
OPTION(_T("default"), _T(""))
ITEM_END()

ITEM_EDIT_OPTIONAL_LIST(_T("Allow connection from:"), _T("This is the hosts that will be allowed to poll performance data from the server."))
OPTION(_T("disabledCaption"), _T("Use global settings (defined previously)"))
OPTION(_T("enabledCaption"), _T("Specify hosts for NSClient server"))
OPTION(_T("listCaption"), _T("Add all IP addresses (not hosts) which should be able to connect:"))
OPTION(_T("separator"), _T(","))
OPTION(_T("disabled"), _T(""))
ITEM_MAP_TO(_T("basic_ini_text_mapper"))
OPTION(_T("section"), _T("NSClient"))
OPTION(_T("key"), _T("allowed_hosts"))
OPTION(_T("default"), _T(""))
ITEM_END()

PAGE_END()
MODULE_SETTINGS_END()
