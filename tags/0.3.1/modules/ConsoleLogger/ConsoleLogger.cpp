// ConsoleLogger.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include "ConsoleLogger.h"


ConsoleLogger gConsoleLogger;

BOOL APIENTRY DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	NSCModuleWrapper::wrapDllMain(hModule, ul_reason_for_call);
	return TRUE;
}

ConsoleLogger::ConsoleLogger() {
}
ConsoleLogger::~ConsoleLogger() {
}

bool ConsoleLogger::loadModule() {
	return true;
}
bool ConsoleLogger::unloadModule() {
	return true;
}
std::string ConsoleLogger::getModuleName() {
	return "Simple console logger (used for debug purposes).";
}
NSCModuleWrapper::module_version ConsoleLogger::getModuleVersion() {
	NSCModuleWrapper::module_version version = {0, 0, 1 };
	return version;
}
bool ConsoleLogger::hasCommandHandler() {
	return false;
}
bool ConsoleLogger::hasMessageHandler() {
	return true;
}
void ConsoleLogger::handleMessage(int msgType, char* file, int line, char* message) {
	std::wcout << "Incoming " << NSCHelper::translateMessageType(msgType) << " message (" << file << ":" << line << ")" << std::endl;
	std::wcout << "   -  " << message << std::endl;
}

NSC_WRAPPERS_MAIN_DEF(gConsoleLogger);
NSC_WRAPPERS_HANDLE_MSG_DEF(gConsoleLogger);
NSC_WRAPPERS_IGNORE_CMD_DEF();