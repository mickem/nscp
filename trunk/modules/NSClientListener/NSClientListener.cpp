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


bool NSClientListener::loadModule() {
	socketThreadManager.createThread(NULL);
	return true;
}
bool NSClientListener::unloadModule() {
	socketThreadManager.exitThread();
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
