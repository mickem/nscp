// CheckEventLog.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include "NRPEListener.h"
#include <strEx.h>
#include <time.h>

NRPEListener gNRPEListener;

BOOL APIENTRY DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	NSCModuleWrapper::wrapDllMain(hModule, ul_reason_for_call);
	return TRUE;
}

NRPEListener::NRPEListener() {
}
NRPEListener::~NRPEListener() {
}


bool NRPEListener::loadModule() {
	return true;
}
bool NRPEListener::unloadModule() {
	return true;
}

std::string NRPEListener::getModuleName() {
	return "CheckDisk Various Disk related checks.";
}
NSCModuleWrapper::module_version NRPEListener::getModuleVersion() {
	NSCModuleWrapper::module_version version = {0, 0, 1 };
	return version;
}

bool NRPEListener::hasCommandHandler() {
	return true;
}
bool NRPEListener::hasMessageHandler() {
	return false;
}


std::string NRPEListener::handleCommand(const std::string command, const unsigned int argLen, char **char_args) {
	return "";
}


NSC_WRAPPERS_MAIN_DEF(gNRPEListener);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gNRPEListener);
