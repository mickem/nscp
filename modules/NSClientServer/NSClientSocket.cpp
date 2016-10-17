/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "strEx.h"
#include "NSClientSocket.h"

/**
 * Default c-tor
 */
NSClientSocket::NSClientSocket() {
}

NSClientSocket::~NSClientSocket() {
}


std::string NSClientSocket::parseRequest(std::string buffer)  {
	strEx::token pwd = strEx::getToken(buffer, '&');
	NSC_DEBUG_MSG("Password: " + pwd.first);
break-on-compile
	if ( (pwd.first.empty()) || (pwd.first != NSCModuleHelper::getSettingsString("NSClient", "password", "")) )
		return "ERROR: Invalid password.";
	if (pwd.second.empty())
		return "ERRRO: No command specified.";
	strEx::token cmd = strEx::getToken(pwd.second, '&');
	if (cmd.first.empty())
		return "ERRRO: No command specified.";
	NSC_DEBUG_MSG("Command: " + cmd.first);
	std::string message, perf;
	NSCAPI::nagiosReturn ret = NSCModuleHelper::InjectSplitAndCommand(cmd.first.c_str(), cmd.second.c_str(), '&', message, perf);
	int c = atoi(cmd.first.c_str());
	switch (c) {
		case REQ_UPTIME:		// Some check_nt commands has no return code syntax
		case REQ_MEMUSE:
		case REQ_CPULOAD:
		case REQ_CLIENTVERSION:
		case REQ_USEDDISKSPACE:
			return message;
		
		case REQ_SERVICESTATE:	// Some check_nt commands return the return code (coded as a string)
		case REQ_PROCSTATE:
			return NSCHelper::translateReturn(ret) + "&" + message;

		default:				// "New" check_nscp also returns performance data
			if (perf.empty())
				return NSCHelper::translateReturn(ret) + "&" + message;
			return NSCHelper::translateReturn(ret) + "&" + message + "|" + perf;
	}
}

void NSClientSocket::onAccept(simpleSocket::Socket client) {
	if (!inAllowedHosts(client.getAddrString())) {
		NSC_LOG_ERROR("Unothorized access from: " + client.getAddrString());
		client.close();
		return;
	}
	simpleSocket::DataBuffer db;
	client.readAll(db);
	if (db.getLength() > 0) {
		std::string incoming(db.getBuffer(), db.getLength());
		NSC_DEBUG_MSG_STD("Incoming data: " + incoming);
		std::string response = parseRequest(incoming);
		NSC_DEBUG_MSG("Outgoing data: " + response);
		client.send(response.c_str(), static_cast<int>(response.length()), 0);
	}
	client.close();
}
