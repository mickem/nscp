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


DWORD WINAPI threadProc(LPVOID param) {
	SysTray* parent = reinterpret_cast<SysTray*>(param);
	try {
		TrayIcon::createDialog();
	} catch (const std::string& s) {
		NSC_LOG_ERROR_STD(s);
		return FALSE;
	}
	return TRUE;
}

SysTray::SysTray() : hThread_(NULL) {
}
SysTray::~SysTray() {
	if (hThread_) {
		NSC_LOG_ERROR("Thread has not closed when exiting...");
		CloseHandle(hThread_);
	}
}

bool SysTray::loadModule() {
	hThread_ = ::CreateThread(NULL, NULL, threadProc, this, NULL, &dwThreadID_);
	if (!hThread_)
		NSC_LOG_ERROR("Could not create thread...");
	return true;
}
bool SysTray::unloadModule() {
	TrayIcon::removeIcon();
	TrayIcon::destroyDialog();
	bool ret = TrayIcon::waitForTermination();
	CloseHandle(hThread_);
	hThread_ = NULL;
	return ret;
}


std::string SysTray::getModuleName() {
	return "This is a nice systray module.";
}
NSCModuleWrapper::module_version SysTray::getModuleVersion() {
	NSCModuleWrapper::module_version version = {0, 0, 1 };
	return version;
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

