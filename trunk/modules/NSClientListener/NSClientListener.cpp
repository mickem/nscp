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
#define REQ_FILEAGE			9	// ... in the works ...
//#define REQ_INSTANCES	10	// ! - not implemented Don't know how to use

BOOL APIENTRY DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	NSCModuleWrapper::wrapDllMain(hModule, ul_reason_for_call);
	return TRUE;
}

NSClientListener::NSClientListener() {
}
NSClientListener::~NSClientListener() {
}
std::string getAllowedHosts() {
	std::string ret = NSCModuleHelper::getSettingsString(NSCLIENT_SECTION_TITLE, MAIN_ALLOWED_HOSTS, "");
	if (ret.empty())
		ret = NSCModuleHelper::getSettingsString(MAIN_SECTION_TITLE, MAIN_ALLOWED_HOSTS, MAIN_ALLOWED_HOSTS_DEFAULT);
	return ret;
}
bool getCacheAllowedHosts() {
	int val = NSCModuleHelper::getSettingsInt(NSCLIENT_SECTION_TITLE, MAIN_ALLOWED_HOSTS_CACHE, -1);
	if (val == -1)
		val = NSCModuleHelper::getSettingsInt(MAIN_SECTION_TITLE, MAIN_ALLOWED_HOSTS_CACHE, MAIN_ALLOWED_HOSTS_CACHE_DEFAULT);
	return val==1?true:false;
}

bool NSClientListener::loadModule() {
	allowedHosts.setAllowedHosts(strEx::splitEx(getAllowedHosts(), ","), getCacheAllowedHosts());
	unsigned short port = NSCModuleHelper::getSettingsInt(NSCLIENT_SECTION_TITLE, NSCLIENT_SETTINGS_PORT, NSCLIENT_SETTINGS_PORT_DEFAULT);
	std::string host = NSCModuleHelper::getSettingsString(NSCLIENT_SECTION_TITLE, NSCLIENT_SETTINGS_BINDADDR, NSCLIENT_SETTINGS_BINDADDR_DEFAULT);
	unsigned int backLog = NSCModuleHelper::getSettingsInt(NSCLIENT_SECTION_TITLE, NSCLIENT_SETTINGS_LISTENQUE, NSCLIENT_SETTINGS_LISTENQUE_DEFAULT);
	socketTimeout_ = NSCModuleHelper::getSettingsInt(NSCLIENT_SECTION_TITLE, NSCLIENT_SETTINGS_READ_TIMEOUT, NSCLIENT_SETTINGS_READ_TIMEOUT_DEFAULT);
	try {
		socket.setHandler(this);
		socket.StartListener(host, port, backLog);
	} catch (simpleSocket::SocketException e) {
		NSC_LOG_ERROR_STD("Exception caught: " + e.getMessage());
		return false;
	}
	return true;
}
bool NSClientListener::unloadModule() {
	try {
		socket.removeHandler(this);
		if (socket.hasListener())
			socket.StopListener();
	} catch (simpleSocket::SocketException e) {
		NSC_LOG_ERROR_STD("Exception caught: " + e.getMessage());
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


std::string getPassword() {
	static std::string password = "";
	if (password.empty()) {
		password = NSCModuleHelper::getSettingsString(NSCLIENT_SECTION_TITLE, MAIN_OBFUSCATED_PASWD, MAIN_OBFUSCATED_PASWD_DEFAULT);
		if (password.empty())
			password= NSCModuleHelper::getSettingsString(MAIN_SECTION_TITLE, MAIN_OBFUSCATED_PASWD, MAIN_OBFUSCATED_PASWD_DEFAULT);
		if (!password.empty()) {
			password = NSCModuleHelper::Decrypt(password);
		} else {
			password = NSCModuleHelper::getSettingsString(NSCLIENT_SECTION_TITLE, MAIN_SETTINGS_PWD, MAIN_SETTINGS_PWD_DEFAULT);
			if (password.empty())
				password = NSCModuleHelper::getSettingsString(MAIN_SECTION_TITLE, MAIN_SETTINGS_PWD, MAIN_SETTINGS_PWD_DEFAULT);
		}
	}
	return password;
}
bool NSClientListener::isPasswordOk(std::string remotePassword)  {
	std::string localPassword = getPassword();
	if (localPassword == remotePassword) {
		return true;
	}
	if ((remotePassword == "None") && (localPassword.empty())) {
		return true;
	}
	return false;
}

std::string NSClientListener::parseRequest(std::string buffer)  {
	NSC_DEBUG_MSG_STD("Data: " + buffer);

	std::string::size_type pos = buffer.find_first_of("\n\r");
	if (pos != std::string::npos) {
		std::string::size_type pos2 = buffer.find_first_not_of("\n\r", pos);
		if (pos2 != std::string::npos) {
			std::string rest = buffer.substr(pos2);
			NSC_DEBUG_MSG_STD("Ignoring data: " + rest);
		}
		buffer = buffer.substr(0, pos);
	}

	strEx::token pwd = strEx::getToken(buffer, '&');
	if (!isPasswordOk(pwd.first)) {
		NSC_LOG_ERROR_STD("Invalid password (" + pwd.first +").");
		return "ERROR: Invalid password."; 
	}
	if (pwd.second.empty())
		return "ERROR: No command specified.";
	strEx::token cmd = strEx::getToken(pwd.second, '&');
	if (cmd.first.empty())
		return "ERROR: No command specified.";

	int c = atoi(cmd.first.c_str());

	NSC_DEBUG_MSG_STD("Data: " + cmd.second);


	// prefix various commands
	switch (c) {
		case REQ_CPULOAD:
			cmd.first = "checkCPU";
			cmd.second += "&nsclient";
			break;
		case REQ_UPTIME:
			cmd.first = "checkUpTime";
			cmd.second = "nsclient";
			break;
		case REQ_USEDDISKSPACE:
			cmd.first = "CheckDriveSize";
			cmd.second += "&nsclient";
			break;
		case REQ_CLIENTVERSION:
			{
				std::string v = NSCModuleHelper::getSettingsString(NSCLIENT_SECTION_TITLE, NSCLIENT_SETTINGS_VERSION, NSCLIENT_SETTINGS_VERSION_DEFAULT);
				if (v == "auto")
					return NSCModuleHelper::getApplicationName() + " " + NSCModuleHelper::getApplicationVersionString();
				return v;
			}
		case REQ_SERVICESTATE:
			cmd.first = "checkServiceState";
			cmd.second += "&nsclient";
			break;
		case REQ_PROCSTATE:
			cmd.first = "checkProcState";
			cmd.second += "&nsclient";
			break;
		case REQ_MEMUSE:
			cmd.first = "checkMem";
			cmd.second = "nsclient";
			break;
		case REQ_COUNTER:
			cmd.first = "checkCounter";
			cmd.second = "Counter=" + cmd.second + "&nsclient";
			break;
		case REQ_FILEAGE:
			cmd.first = "getFileAge";
			cmd.second = "path=" + cmd.second;
			break;
	}

	std::string message, perf;
	NSCAPI::nagiosReturn ret = NSCModuleHelper::InjectSplitAndCommand(cmd.first.c_str(), cmd.second.c_str(), '&', message, perf);
	if (!NSCHelper::isNagiosReturnCode(ret)) {
		if (message.empty())
			return "ERROR: Could not complete the request check log file for more information.";
		return "ERROR: " + message;
	}
	switch (c) {
		case REQ_UPTIME:		// Some check_nt commands has no return code syntax
		case REQ_MEMUSE:
		case REQ_CPULOAD:
		case REQ_CLIENTVERSION:
		case REQ_USEDDISKSPACE:
		case REQ_COUNTER:
		case REQ_FILEAGE:
			return message;

		case REQ_SERVICESTATE:	// Some check_nt commands return the return code (coded as a string)
		case REQ_PROCSTATE:
			return strEx::itos(NSCHelper::nagios2int(ret)) + "& " + message;

		default:				// "New" check_nscp also returns performance data
			if (perf.empty())
				return NSCHelper::translateReturn(ret) + "&" + message;
			return NSCHelper::translateReturn(ret) + "&" + message + "|" + perf;
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
		bool lastReadRet = client->readAll(db);
		if (db.getLength() > 0) {
			unsigned long long pos = db.find('\n');
			if (pos==-1) {
				std::string incoming(db.getBuffer(), db.getLength());
				sendTheResponse(client, parseRequest(incoming));
				break;
			} else if (pos>0) {
				simpleSocket::DataBuffer buffer = db.unshift(static_cast<const unsigned int>(pos));
				std::string bstr(buffer.getBuffer(), buffer.getLength());
				db.nibble(1);
				std::string rstr(db.getBuffer(), db.getLength());
				std::string incoming(buffer.getBuffer(), buffer.getLength());
				sendTheResponse(client, parseRequest(incoming) + "\n");
			} else {
				NSC_LOG_ERROR_STD("First char should (i think) not be a \\n :(");
			}
		} else if (!lastReadRet) {
			NSC_LOG_MESSAGE("Could not read NSClient packet from socket :(");
			client->close();
			return;
		} else {
			Sleep(100);
		}
	}
	if (i >= maxWait) {
		NSC_LOG_ERROR_STD("Timeout reading NS-client packet (increase socket_timeout).");
		client->close();
		return;
	}
}


void NSClientListener::onAccept(simpleSocket::Socket *client) {
	if (!allowedHosts.inAllowedHosts(client->getAddr())) {
		NSC_LOG_ERROR("Unauthorized access from: " + client->getAddrString());
		client->close();
		return;
	}
	retrivePacket(client);



//	client->readAll(db);
//	if (db.getLength() > 0) {
//		std::string incoming(db.getBuffer(), db.getLength());
//		NSC_DEBUG_MSG_STD("Incoming data: " + incoming);
//		std::string response = parseRequest(incoming);
//		NSC_DEBUG_MSG("Outgoing data: " + response);
//		client->send(response.c_str(), static_cast<int>(response.length()), 0);
//	}
	client->close();
}

NSC_WRAPPERS_MAIN_DEF(gNSClientListener);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_IGNORE_CMD_DEF();
NSC_WRAPPERS_HANDLE_CONFIGURATION(gNSClientListener);


MODULE_SETTINGS_START(NSClientListener, "NSClient Listener configuration", "...")

PAGE("NSClient Listener configuration")

ITEM_EDIT_TEXT("port", "This is the port the NSClientListener.dll will listen to.")
ITEM_MAP_TO("basic_ini_text_mapper")
OPTION("section", "NSClient")
OPTION("key", "port")
OPTION("default", "12489")
ITEM_END()

PAGE_END()
ADVANCED_PAGE("Server settings")

ITEM_EDIT_TEXT("password", "Enter the password used by applications when queriying for data.")
ITEM_MAP_TO("basic_ini_text_mapper")
OPTION("section", "NSClient")
OPTION("key", "password")
OPTION("default", "")
ITEM_END()

ITEM_EDIT_OPTIONAL_LIST("Allow connection from:", "This is the hosts that will be allowed to poll performance data from the server.")
OPTION("disabledCaption", "Use global settings (defined previously)")
OPTION("enabledCaption", "Specify hosts for NSClient server")
OPTION("listCaption", "Add all IP addresses (not hosts) which should be able to connect:")
OPTION("separator", ",")
OPTION("disabled", "")
ITEM_MAP_TO("basic_ini_text_mapper")
OPTION("section", "NSClient")
OPTION("key", "allowed_hosts")
OPTION("default", "")
ITEM_END()

PAGE_END()
MODULE_SETTINGS_END()