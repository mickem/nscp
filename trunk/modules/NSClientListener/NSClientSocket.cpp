#include "stdafx.h"
#include "strEx.h"
#include "NSClientSocket.h"

/**
 * Default c-tor
 */
NSClientSocket::NSClientSocket() : SimpleSocketListsner(DEFAULT_TCP_PORT) {
}

NSClientSocket::~NSClientSocket() {
}


#define RECV_BUFFER_LEN 1024

std::string NSClientSocket::parseRequest(std::string buffer)  {
	strEx::token pwd = strEx::getToken(buffer, '&');
	NSC_DEBUG_MSG("Password: " + pwd.first);
	if ( (pwd.first.empty()) || (pwd.first != NSCModuleHelper::getSettingsString("generic", "password", "")) )
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

void NSClientSocket::onAccept(SOCKET client) {

	readAllDataBlock rdb = readAll(client);
	if (rdb.second > 0) {
		NSC_DEBUG_MSG_STD("Incoming data length: " + strEx::itos((int)rdb.second));
		std::string incoming((char*)rdb.first, (unsigned int)rdb.second);
		NSC_DEBUG_MSG_STD("Incoming data: " + incoming);
		std::string response = parseRequest(incoming);
		NSC_DEBUG_MSG("Outgoing data: " + response);
		send(client, response.c_str(), static_cast<int>(response.length()), 0);
	}
	delete [] rdb.first;
	closesocket(client);
}
