// CheckEventLog.cpp : Defines the entry point for the DLL application.
//

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
#define REQ_COUNTER			8	// ... in the works ...
//#define REQ_FILEAGE		9	// ! - not implemented Dont know how to use
//#define REQ_INSTANCES	10	// ! - not implemented Dont know how to use

BOOL APIENTRY DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	NSCModuleWrapper::wrapDllMain(hModule, ul_reason_for_call);
	return TRUE;
}

NSClientListener::NSClientListener() {
}
NSClientListener::~NSClientListener() {
}

bool NSClientListener::loadModule() {
	allowedHosts.setAllowedHosts(strEx::splitEx(NSCModuleHelper::getSettingsString(NSCLIENT_SECTION_TITLE, NSCLIENT_SETTINGS_ALLOWED, NSCLIENT_SETTINGS_ALLOWED_DEFAULT), ","));
	try {
		socket.setHandler(this);
		socket.StartListener(NSCModuleHelper::getSettingsInt(NSCLIENT_SECTION_TITLE, NSCLIENT_SETTINGS_PORT, NSCLIENT_SETTINGS_PORT_DEFAULT));
	} catch (simpleSocket::SocketException e) {
		NSC_LOG_ERROR_STD("Exception caught: " + e.getMessage());
		return false;
	}
	return true;
}
bool NSClientListener::unloadModule() {
	try {
		socket.removeHandler(this);
		socket.StopListener();
	} catch (simpleSocket::SocketException e) {
		NSC_LOG_ERROR_STD("Exception caught: " + e.getMessage());
		return false;
	}
	return true;
}

std::string NSClientListener::getModuleName() {
	return "NSClient Listener.";
}
NSCModuleWrapper::module_version NSClientListener::getModuleVersion() {
	NSCModuleWrapper::module_version version = {0, 0, 1 };
	return version;
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

std::string NSClientListener::parseRequest(std::string buffer)  {
	strEx::token pwd = strEx::getToken(buffer, '&');
	if ( (pwd.first.empty()) || (pwd.first != NSCModuleHelper::getSettingsString(NSCLIENT_SECTION_TITLE, NSCLIENT_SETTINGS_PWD, NSCLIENT_SETTINGS_PWD_DEFAULT)) )
		return "ERROR: Invalid password.";
	if (pwd.second.empty())
		return "ERRRO: No command specified.";
	strEx::token cmd = strEx::getToken(pwd.second, '&');
	if (cmd.first.empty())
		return "ERRRO: No command specified.";

	int c = atoi(cmd.first.c_str());

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
				std::string v = NSCModuleHelper::getSettingsString("nsclient compat", "version", "modern");
				if (v == "modern")
					return NSCModuleHelper::getApplicationName() + " " + NSCModuleHelper::getApplicationVersionString();
				return NSCModuleHelper::getSettingsString("nsclient compat", "version", "modern");
			}
		case REQ_SERVICESTATE:
			cmd.first = "checkServiceState";
			break;
		case REQ_PROCSTATE:
			cmd.first = "checkProcState";
			break;
		case REQ_MEMUSE:
			cmd.first = "checkMem";
			cmd.second = "nsclient";
			break;
		case REQ_COUNTER:
			cmd.first = "checkCounter";
			cmd.second += "&nsclient";
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

void NSClientListener::onClose()
{}

void NSClientListener::onAccept(simpleSocket::Socket *client) {
	if (!allowedHosts.inAllowedHosts(client->getAddrString())) {
		NSC_LOG_ERROR("Unothorized access from: " + client->getAddrString());
		client->close();
		return;
	}
	simpleSocket::DataBuffer db;



	for (int i=0;i<100;i++) {
		client->readAll(db);
		// @todo Make this check if a pcket is read instead of just if we have data
		if (db.getLength() > 0)
			break;
		Sleep(100);
	}
	if (i == 100) {
		NSC_LOG_ERROR_STD("Could not retrieve NSClient packet.");
		client->close();
		return;
	}



//	client->readAll(db);
	if (db.getLength() > 0) {
		std::string incoming(db.getBuffer(), db.getLength());
//		NSC_DEBUG_MSG_STD("Incoming data: " + incoming);
		std::string response = parseRequest(incoming);
//		NSC_DEBUG_MSG("Outgoing data: " + response);
		client->send(response.c_str(), static_cast<int>(response.length()), 0);
	}
	client->close();
}

NSC_WRAPPERS_MAIN_DEF(gNSClientListener);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_IGNORE_CMD_DEF();
