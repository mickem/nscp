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
#include <boost/date_time.hpp>
#include <fstream>
#include <utils.h>

namespace sh = nscapi::settings_helper;

FileLogger gFileLogger;

FileLogger::FileLogger() : init_(false) {
}
FileLogger::~FileLogger() {
}

#ifdef WIN32
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
#endif

std::wstring getFolder(std::wstring key) {
	if (key == _T("exe")) {
		return GET_CORE()->getBasePath();
	} else {
#ifdef WIN32
		if (key == _T("local-app-data")) {
			TCHAR buf[MAX_PATH+1];
			_SHGetSpecialFolderPath(NULL, buf, CSIDL_COMMON_APPDATA, FALSE);
			return buf;
		}
#endif
	}
	return GET_CORE()->getBasePath();
}
std::string FileLogger::getFileName() {
	if (file_.empty()) {
		file_ = to_string(cfg_file_);
		if (file_.empty())
			file_ = to_string(setting_keys::log::FILENAME_DEFAULT);
		if (file_.find("\\") == std::wstring::npos) {
			std::string root = to_string(getFolder(cfg_root_));
			std::string::size_type pos = root.find_last_not_of('\\');
			if (pos != std::wstring::npos) {
				//root = root.substr(0, pos);
			}
			file_ = root + "\\" + file_;
		}
	}
	return file_;
}

bool FileLogger::loadModule() {
	return false;
}
bool FileLogger::loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode) {
	//_tzset();
	try {
		std::wstring log_mask, file, root;

		sh::settings_registry settings(nscapi::plugin_singleton->get_core());
		settings.set_alias(_T("log"), alias);

		settings.alias().add_path_to_settings()
			(_T("LOG SECTION"), _T("Configure loggning properties."))
			;

		settings.alias().add_key_to_settings()
			//(_T("debug"), sh::bool_key(&debug_, false),
			//_T("DEBUG LOGGING"), _T("Enable debug logging can help track down errors and find problems but will impact overall performance negativly."))

			(_T("log mask"), sh::wstring_key(&log_mask, false),
			_T("LOG MASK"), _T("The log mask information, error, warning, critical, debug"))

			(_T("root"), sh::wstring_key(&cfg_root_, _T("auto")),
			_T("ROOT"), _T("Set this to use a specific syntax string for all commands (that don't specify one)."))

			(_T("file name"), sh::wstring_key(&cfg_file_),
			_T("FILENAME"), _T("The file to write log data to. If no directory is used this is relative to the NSClient++ binary."))

			(_T("date format"), sh::string_key(&format_, "%Y-%m-%d %H:%M:%S"),
			_T("DATEMASK"), _T("The size of the buffer to use when getting messages this affects the speed and maximum size of messages you can recieve."))
			;

		settings.register_all();
		settings.notify();

		log_mask_ = nscapi::logging::parse(log_mask);

		getFileName();

	} catch (std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Exception caught: ") + to_wstring(e.what()));
		return false;
	} catch (nscapi::nscapi_exception &e) {
		NSC_LOG_ERROR_STD(_T("Failed to register command: ") + e.msg_);
		return false;
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to register command."));
		return false;
	}
	NSC_LOG_MESSAGE_STD(_T("Using logmask: ") + nscapi::logging::to_string(log_mask_));
	init_ = true;
	std::wstring hello = _T("Starting to log for: ") + GET_CORE()->getApplicationName() + _T(" - ") + GET_CORE()->getApplicationVersionString();
	handleMessage(NSCAPI::log, __FILEW__, __LINE__, hello.c_str());
	NSC_LOG_MESSAGE_STD(_T("Log path is: ") + to_wstring(file_));
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
/*
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
	if (hFile == INVALID_HANDLE_VALUE) {
		hFile = ::CreateFile(file.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile != INVALID_HANDLE_VALUE) {
			WORD wBOM = 0xFEFF;
			::WriteFile(hFile, &wBOM, sizeof(WORD), &numberOfBytesWritten, NULL);
	}
	return hFile;
}
*/

void FileLogger::handleMessage(int msgType, const wchar_t* file, int line, const TCHAR* message) {
	if (!init_) {
		std::wcout << _T("Discarding: ") << message << std::endl;
		return;
	}
	if (!nscapi::logging::matches(log_mask_, msgType))
		return;

	std::ofstream stream(file_.c_str(), std::ios::out|std::ios::app|std::ios::ate);
	if (!stream) {
		std::wcout << _T("File could not be opened, Discarding: ") << message << std::endl;
	}
	stream << to_string(get_formated_date()) 
		<< (": ") << to_string(nscapi::plugin_helper::translateMessageType(msgType))
		<< (":") << to_string(std::wstring(file))
		<<(":") << to_string(line) 
		<< (": ") << to_string(std::wstring(message)) << std::endl;
}

std::wstring FileLogger::get_formated_date() {
	std::wstringstream ss;
	boost::posix_time::time_facet *facet = new boost::posix_time::time_facet(format_.c_str());
	ss.imbue(std::locale(std::cout.getloc(), facet));
	ss << boost::posix_time::second_clock::local_time();
	return ss.str();
}

NSC_WRAP_DLL();
NSC_WRAPPERS_MAIN_DEF(gFileLogger);
NSC_WRAPPERS_HANDLE_MSG_DEF(gFileLogger);
NSC_WRAPPERS_IGNORE_CMD_DEF();
