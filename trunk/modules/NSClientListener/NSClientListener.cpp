// CheckEventLog.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include "NSClientListener.h"
#include <strEx.h>
#include <time.h>
#include <config.h>

NSClientListener gNSClientListener;

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
	socket.setAllowedHosts(strEx::splitEx(NSCModuleHelper::getSettingsString("NRPE", "allowed_hosts", ""), ","));
	try {
		socket.StartListen(NSCModuleHelper::getSettingsInt("NSClient", "port", DEFAULT_NSCLIENT_PORT));
	} catch (simpleSocket::SocketException e) {
		NSC_LOG_ERROR_STD("Exception caught: " + e.getMessage());
		return false;
	}
	return true;
}
bool NSClientListener::unloadModule() {
	try {
		socket.close();
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

NSC_WRAPPERS_MAIN_DEF(gNSClientListener);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_IGNORE_CMD_DEF();
