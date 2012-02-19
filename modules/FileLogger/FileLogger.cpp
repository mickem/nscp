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

//#include <config.h>

#include <string>
#include <iostream>
#include <fstream>

#include <boost/date_time.hpp>
#include <boost/filesystem.hpp>

#include "FileLogger.h"

#include <utils.h>

#include <settings/client/settings_client.hpp>
#include <settings/macros.h>
#include <protobuf/plugin.pb.h>

namespace sh = nscapi::settings_helper;

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
			wchar_t buf[MAX_PATH+1];
			_SHGetSpecialFolderPath(NULL, buf, CSIDL_COMMON_APPDATA, FALSE);
			return buf;
		}
#endif
	}
	return GET_CORE()->getBasePath();
}
std::string FileLogger::getFileName() {
	if (file_.empty()) {
		file_ = utf8::cvt<std::string>(cfg_file_);
		if (file_.empty())
			file_ = "nsclient.log";
		if (file_.find("\\") == std::wstring::npos) {
			std::string root = utf8::cvt<std::string>(getFolder(cfg_root_));
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

		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias(_T("file logger"), alias);

		settings.alias().add_path_to_settings()
			(_T("LOG SECTION"), _T("Configure loggning properties."))
			;

		settings.alias().add_parent(_T("/settings/log")).add_key_to_settings()
			(_T("level"), sh::wstring_key(&log_mask, _T("info")),
			_T("LOG LEVEL"), _T("The log level info, error, warning, critical, debug"))

			(_T("root"), sh::wstring_key(&cfg_root_, _T("auto")),
			_T("ROOT"), _T("Set this to use a specific syntax string for all commands (that don't specify one)."))

			(_T("file name"), sh::wstring_key(&cfg_file_),
			_T("FILENAME"), _T("The file to write log data to. If no directory is used this is relative to the NSClient++ binary."))

			(_T("date format"), sh::string_key(&format_, "%Y-%m-%d %H:%M:%S"),
			_T("DATEMASK"), _T("The size of the buffer to use when getting messages this affects the speed and maximum size of messages you can recieve."))

			(_T("max size"), sh::int_key(&max_size_, 0),
			_T("MAXIMUM FILE SIZE"), _T("When file size reaches this it will be truncated to 50% if set to 0 (default) truncation will be disabled"))
			;

		settings.register_all();
		settings.notify();

		log_mask_ = nscapi::logging::parse(log_mask);

		getFileName();

	} catch (nscapi::nscapi_exception &e) {
		NSC_LOG_ERROR_STD(_T("Failed to register command: ") + utf8::cvt<std::wstring>(e.what()));
		return false;
	} catch (std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Exception caught: ") + utf8::cvt<std::wstring>(e.what()));
		return false;
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to register command."));
		return false;
	}
	NSC_LOG_MESSAGE_STD(_T("Using logmask: ") + nscapi::logging::to_string(log_mask_));
	init_ = true;
	std::string hello = "Starting to log for: " + utf8::cvt<std::string>(GET_CORE()->getApplicationName()) + " - " + utf8::cvt<std::string>(GET_CORE()->getApplicationVersionString());
	log(NSCAPI::log_level::log, __FILE__, __LINE__, hello);
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

void FileLogger::handleMessageRAW(std::string data) {
	try {
		Plugin::LogEntry message;
		message.ParseFromString(data);

		for (int i=0;i<message.entry_size();i++) {
			Plugin::LogEntry::Entry msg = message.entry(i);
			log(msg.level(), msg.file(), msg.line(), msg.message());
		}
	} catch (std::exception &e) {
		std::cout << "Failed to parse data from: " << strEx::strip_hex(data) << e.what() <<  std::endl;;
	} catch (...) {
		std::cout << "Failed to parse data from: " << strEx::strip_hex(data) << std::endl;;
	}
}
void FileLogger::log(int msgType, const std::string file, int line, std::string message) {
	if (!init_) {
		std::wcout << _T("Discarding: ") << utf8::cvt<std::wstring>(message) << std::endl;
		return;
	}
	if (!nscapi::logging::matches(log_mask_, msgType))
		return;

	if (max_size_ != 0 &&  boost::filesystem::exists(file_.c_str()) && boost::filesystem::file_size(file_.c_str()) > max_size_) {
		int target_size = max_size_*0.7;
		char *tmpBuffer = new char[target_size+1];
		try {
			std::ifstream ifs(file_.c_str());
			ifs.seekg(-target_size, std::ios_base::end);  // One call to find it. . .
			ifs.read(tmpBuffer, target_size);
			ifs.close();
			std::ofstream ofs(file_.c_str(), std::ios::trunc);
			ofs.write(tmpBuffer, target_size);
		} catch (...) {
			std::cout << "Failed to truncate log file: " << file_ << std::endl;;
		}
		delete [] tmpBuffer;
	}
	std::ofstream stream(file_.c_str(), std::ios::out|std::ios::app|std::ios::ate);
	if (!stream) {
		std::wcout << _T("File could not be opened, Discarding: ") << utf8::cvt<std::wstring>(message) << std::endl;
	}
	stream << utf8::cvt<std::string>(get_formated_date()) 
		<< (": ") << utf8::cvt<std::string>(nscapi::plugin_helper::translateMessageType(msgType))
		<< (":") << file
		<<(":") << line
		<< (": ") << message << std::endl;
}

std::wstring FileLogger::get_formated_date() {
	std::wstringstream ss;
	boost::posix_time::time_facet *facet = new boost::posix_time::time_facet(format_.c_str());
	ss.imbue(std::locale(std::cout.getloc(), facet));
	ss << boost::posix_time::second_clock::local_time();
	return ss.str();
}

NSC_WRAP_DLL();
NSC_WRAPPERS_MAIN_DEF(FileLogger);
NSC_WRAPPERS_HANDLE_MSG_DEF();
NSC_WRAPPERS_IGNORE_CMD_DEF();
