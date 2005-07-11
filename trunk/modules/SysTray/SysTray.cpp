// SysTray.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include "SysTray.h"
#include "TrayIcon.h"

SysTray gSysTray;

BOOL APIENTRY DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	NSCModuleWrapper::wrapDllMain(hModule, ul_reason_for_call);
    return TRUE;
}

SysTray::SysTray() {}
SysTray::~SysTray() {}
bool SysTray::loadModule() {
	icon.createThread();
	return true;
}
bool SysTray::unloadModule() {
	if (!icon.exitThread(20000)) {
		std::cout << "MAJOR ERROR: Could not unload thread..." << std::endl;
		NSC_LOG_ERROR("Could not exit the thread, memory leak and potential corruption may be the result...");
		return false;
	}
	return true;
}


bool SysTray::hasCommandHandler() {
	return false;
}
bool SysTray::hasMessageHandler() {
	return false;
}

NSC_WRAPPERS_MAIN_DEF(gSysTray);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_IGNORE_CMD_DEF();

