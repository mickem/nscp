// CheckEventLog.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include "NSClientListener.h"
#include <strEx.h>
#include <time.h>

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

#define DEFAULT_TCP_PORT 12489

bool NSClientListener::loadModule() {
	socket.StartListen(NSCModuleHelper::getSettingsInt("NSClient", "port", DEFAULT_TCP_PORT));
	return true;
}
bool NSClientListener::unloadModule() {
	socket.close();
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
