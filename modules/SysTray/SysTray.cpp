// SysTray.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include "SysTray.h"
#include "TrayIcon.h"
#include <ServiceCmd.h>
#include <config.h>

SysTray gSysTray;

BOOL APIENTRY DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	NSCModuleWrapper::wrapDllMain(hModule, ul_reason_for_call);
    return TRUE;
}

SysTray::SysTray() : icon("SysTray") {}
SysTray::~SysTray() {}
bool SysTray::loadModule() {
	try {
		if ((serviceControll::GetServiceType(SZSERVICENAME)&SERVICE_INTERACTIVE_PROCESS)!=SERVICE_INTERACTIVE_PROCESS) {
			NSC_LOG_ERROR("SysTray is not installed (or it cannot interact with the desktop) SysTray wont be loaded. Run " SZAPPNAME " SysTray install ti change this.");
			return true;
		}
	} catch (serviceControll::SCException e) {
		NSC_LOG_ERROR("SysTray is not installed (or it cannot interact with the desktop) SysTray wont be loaded. Run " SZAPPNAME " SysTray install ti change this.");
		return true;
	}
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

int SysTray::commandLineExec(const char* command,const unsigned int argLen,char** args) {
	if (stricmp(command, "install") == 0) {
		try {
			serviceControll::ModifyServiceType(SZSERVICENAME, SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS);
			NSC_LOG_MESSAGE(MODULE_NAME " is now able to run as the SERVICE_INTERACTIVE_PROCESS flag has been set.");
		} catch (const serviceControll::SCException& e) {
			NSC_LOG_ERROR("Could not modify service: " + e.error_);
			return -1;
		}
	} else if (stricmp(command, "uninstall") == 0) {
		try {
			serviceControll::ModifyServiceType(SZSERVICENAME, SERVICE_WIN32_OWN_PROCESS);
			NSC_LOG_MESSAGE(MODULE_NAME " is now not able to run as the SERVICE_INTERACTIVE_PROCESS flag has been reset.");
		} catch (const serviceControll::SCException& e) {
			NSC_LOG_ERROR("Could not modify service: " + e.error_);
			return -1;
		}
	}
	return 0;
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
NSC_WRAPPERS_CLI_DEF(gSysTray);

