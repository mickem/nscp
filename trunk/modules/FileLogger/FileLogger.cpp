// ConsoleLogger.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include "FileLogger.h"

FileLogger gFileLogger;

BOOL APIENTRY DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	NSCModuleWrapper::wrapDllMain(hModule, ul_reason_for_call);
	return TRUE;
}

FileLogger::FileLogger() {
}
FileLogger::~FileLogger() {
}

bool FileLogger::loadModule() {
	file_ = NSCModuleHelper::getSettingsString("log", "file", "nsclient.log");
	return true;
}
bool FileLogger::unloadModule() {
	return true;
}
std::string FileLogger::getModuleName() {
	return "File logger: " + NSCModuleHelper::getSettingsString("log", "file", "nsclient.log");
}
NSCModuleWrapper::module_version FileLogger::getModuleVersion() {
	NSCModuleWrapper::module_version version = {0, 0, 1 };
	return version;
}
bool FileLogger::hasCommandHandler() {
	return false;
}
bool FileLogger::hasMessageHandler() {
	return true;
}
void FileLogger::handleMessage(int msgType, char* file, int line, char* message) {
	std::ofstream stream(file_.c_str(), std::ios::app);
	stream << NSCHelper::translateMessageType(msgType) << ":" << file << ":" << line << ": " << message << std::endl;
}

NSC_WRAPPERS_MAIN_DEF(gFileLogger);
NSC_WRAPPERS_HANDLE_MSG_DEF(gFileLogger);
NSC_WRAPPERS_IGNORE_CMD_DEF();