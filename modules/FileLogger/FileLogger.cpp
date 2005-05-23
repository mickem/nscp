// ConsoleLogger.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include "FileLogger.h"

#include <sys/timeb.h>
#include <time.h>

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

std::string FileLogger::getFileName()
{
	if (file_.empty()) {
		file_ = NSCModuleHelper::getSettingsString(LOG_SECTION_TITLE, LOG_FILENAME, LOG_FILENAME_DEFAULT);
		if (file_.find("\\") == std::string::npos)
			file_ = NSCModuleHelper::getBasePath() + "\\" + file_;
	}
	return file_;
}

bool FileLogger::loadModule() {
	_tzset();
	getFileName();
	format_ = NSCModuleHelper::getSettingsString(LOG_SECTION_TITLE, LOG_DATEMASK, LOG_DATEMASK_DEFAULT);
	return true;
}
bool FileLogger::unloadModule() {
	return true;
}
std::string FileLogger::getModuleName() {
	return "File logger: " + getFileName();
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
	char buffer[64];
	std::ofstream stream(file_.c_str(), std::ios::app);
	__time64_t ltime;
	_time64( &ltime );
	struct tm *today = _localtime64( &ltime );
	if (today) {
		int len = strftime(buffer, 63, format_.c_str(), today);
		if ((len < 1)||(len > 64))
			strncpy(buffer, "???", 63);
		else
			buffer[len] = 0;
	} else {
		strncpy(buffer, "???", 63);
	}
	stream << buffer << ": " << NSCHelper::translateMessageType(msgType) << ":" << file << ":" << line << ": " << message << std::endl;
}

NSC_WRAPPERS_MAIN_DEF(gFileLogger);
NSC_WRAPPERS_HANDLE_MSG_DEF(gFileLogger);
NSC_WRAPPERS_IGNORE_CMD_DEF();