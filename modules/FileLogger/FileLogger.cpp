/**************************************************************************
*   Copyright (C) 2004-2007 by Michael Medin <michael@medin.name>         *
*                                                                         *
*   This code is part of NSClient++ - http://trac.nakednuns.org/nscp      *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/

#include "stdafx.h"
#include "FileLogger.h"

#include <sys/timeb.h>
#include <time.h>
#include <utils.h>

FileLogger gFileLogger;

BOOL APIENTRY DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	NSCModuleWrapper::wrapDllMain(hModule, ul_reason_for_call);
	return TRUE;
}

FileLogger::FileLogger() : init_(false) {
}
FileLogger::~FileLogger() {
}

#ifndef CSIDL_COMMON_APPDATA 
#define CSIDL_COMMON_APPDATA 0x0023 
#endif
typedef BOOL (WINAPI *fnSHGetSpecialFolderPath)(HWND hwndOwner, LPTSTR lpszPath, int nFolder, BOOL fCreate);

__inline BOOL WINAPI _SHGetSpecialFolderPath(HWND hwndOwner, LPTSTR lpszPath, int nFolder, BOOL fCreate) {
	static fnSHGetSpecialFolderPath __SHGetSpecialFolderPath = NULL;
	if (!__SHGetSpecialFolderPath) {
		HMODULE hDLL = LoadLibrary(_T("shfolder.dll"));
		if (hDLL != NULL)
			__SHGetSpecialFolderPath = (fnSHGetSpecialFolderPath)GetProcAddress(hDLL,"SHGetSpecialFolderPathW");
	}
	if(__SHGetSpecialFolderPath)
		return __SHGetSpecialFolderPath(hwndOwner, lpszPath, nFolder, fCreate);
	return FALSE;
}

std::wstring getFolder(std::wstring key) {
	if (key == _T("exe")) {
		return NSCModuleHelper::getBasePath();
	} else {
		if (key == _T("local-app-data")) {
			TCHAR buf[MAX_PATH+1];
			_SHGetSpecialFolderPath(NULL, buf, CSIDL_COMMON_APPDATA, FALSE);
			return buf;
		}
	}
	return NSCModuleHelper::getBasePath();
}
std::wstring FileLogger::getFileName() {
	if (file_.empty()) {
		file_ = SETTINGS_GET_STRING(log::FILENAME);
		if (file_.empty())
			file_ = settings::log::FILENAME_DEFAULT;
		if (file_.find(_T("\\")) == std::wstring::npos) {
			std::wstring root = getFolder(SETTINGS_GET_STRING(log::ROOT));
			std::wstring::size_type pos = root.find_last_not_of(L'\\');
			if (pos != std::wstring::npos) {
				//root = root.substr(0, pos);
			}
			file_ = root + _T("\\") + file_;
		}
	}
	return file_;
}

bool FileLogger::loadModule(NSCAPI::moduleLoadMode mode) {
	_tzset();
	getFileName();

	try {
		SETTINGS_REG_PATH(log::SECTION);

		SETTINGS_REG_KEY_S(log::FILENAME);
		SETTINGS_REG_KEY_S(log::DATEMASK);
		SETTINGS_REG_KEY_B(log::DEBUG_LOG);

	} catch (NSCModuleHelper::NSCMHExcpetion &e) {
		NSC_LOG_ERROR_STD(_T("Failed to register command: ") + e.msg_);
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to register command."));
	}


	format_ = SETTINGS_GET_STRING(log::DATEMASK);
	init_ = true;
	std::wstring hello = _T("Starting to log for: ") + NSCModuleHelper::getApplicationName() + _T(" - ") + NSCModuleHelper::getApplicationVersionString();
	handleMessage(NSCAPI::log, __FILEW__, __LINE__, hello.c_str());
	NSC_LOG_MESSAGE_STD(_T("Log path is: ") + file_ );
	return true;
}
bool FileLogger::unloadModule() {
	return true;
}
bool FileLogger::hasCommandHandler() {
	return false;
}
bool FileLogger::hasMessageHandler() {
	return true;
}
HANDLE openAppendOrNew(std::wstring file) {
	DWORD numberOfBytesWritten = 0;
	HANDLE hFile = ::CreateFile(file.c_str(), GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		hFile = ::CreateFile(file.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile != INVALID_HANDLE_VALUE) {
			WORD wBOM = 0xFEFF;
			::WriteFile(hFile, &wBOM, sizeof(WORD), &numberOfBytesWritten, NULL);
		}
	}
	return hFile;
}
void FileLogger::writeEntry(std::wstring line) {
	DWORD numberOfBytesWritten;
	HANDLE hFile = openAppendOrNew(file_);
	if (hFile == INVALID_HANDLE_VALUE) {
		if (line.find(_T("can not log to file")) != std::wstring::npos)
			NSC_LOG_MESSAGE_STD(_T("Failed to create log file and temporary log! (can not log to file): ") + file_ );
		return;
	}
	if (::SetFilePointer(hFile, 0, NULL, FILE_END) == INVALID_SET_FILE_POINTER) {
		std::wcout << _T("Failed to move pointer to end of file...") << std::endl;
	}
	::WriteFile(hFile, line.c_str(), (line.length())*(sizeof(TCHAR)), &numberOfBytesWritten, NULL);
	::CloseHandle(hFile);
}

void FileLogger::handleMessage(int msgType, TCHAR* file, int line, const TCHAR* message) {
	if (!init_) {
		std::wcout << _T("Discarding message (not initzialized yet") << std::endl;
		return;
	}
	TCHAR buffer[65];
	__time64_t ltime;
	_time64( &ltime );
	struct tm *today = _localtime64( &ltime );
	if (today) {
		size_t len = wcsftime(buffer, 63, format_.c_str(), today);
		if ((len < 1)||(len > 64))
			wcsncpy_s(buffer, 64, _T("???"), 63);
		else
			buffer[len] = 0;
	} else {
		wcsncpy_s(buffer, 64, _T("???"), 63);
	}
	writeEntry(std::wstring(buffer) + _T(": ") + 
		NSCHelper::translateMessageType(msgType) + _T(":") + 
		std::wstring(file) + _T(":") + strEx::itos(line) +_T(": ") + 
		message + _T("\r\n"));
}

NSC_WRAPPERS_MAIN_DEF(gFileLogger);
NSC_WRAPPERS_HANDLE_MSG_DEF(gFileLogger);
NSC_WRAPPERS_IGNORE_CMD_DEF();
NSC_WRAPPERS_HANDLE_CONFIGURATION(gFileLogger);




MODULE_SETTINGS_START(FileLogger, _T("File logger configuration"),_T("..."))
PAGE(_T("Filelogger"))

ITEM_CHECK_BOOL(_T("Log debug messages"), _T("Enable this to log debug messages (when running with /test debuglog is always enabled)"))
ITEM_MAP_TO(_T("basic_ini_text_mapper"))
OPTION(_T("section"), _T("log"))
OPTION(_T("key"), _T("debug"))
OPTION(_T("default"), _T("false"))
OPTION(_T("true_value"), _T("1"))
OPTION(_T("false_value"), _T("0"))
ITEM_END()

ITEM_EDIT_TEXT(_T("Log file"), _T("This is the size of the buffer that stores CPU history."))
OPTION(_T("unit"), _T("(relative to NSClient++ binary"))
ITEM_MAP_TO(_T("basic_ini_text_mapper"))
OPTION(_T("section"), _T("log"))
OPTION(_T("key"), _T("file"))
OPTION(_T("default"), _T("NSC.log"))
ITEM_END()

ITEM_EDIT_TEXT(_T("Date mask"), _T("The date/timeformat in the log file."))
ITEM_MAP_TO(_T("basic_ini_text_mapper"))
OPTION(_T("section"), _T("log"))
OPTION(_T("key"), _T("date_mask"))
OPTION(_T("default"), _T("%Y-%m-%d %H:%M:%S"))
ITEM_END()

PAGE_END()
MODULE_SETTINGS_END()
