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
#include "CheckNSCP.h"

#include <file_helpers.hpp>
#include <unicode_char.hpp>

#include <config.h>

#include <settings/client/settings_client.hpp>
namespace sh = nscapi::settings_helper;

bool CheckNSCP::loadModule() {
	return false;
}
bool CheckNSCP::loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode) {
	try {

		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias(_T("crash"), alias);

		settings.alias().add_path_to_settings()
			(_T("CRASH SECTION"), _T("Configure crash handling properties."))
			;

		settings.alias().add_key_to_settings()
			(_T("archive folder"), sh::wpath_key(&crashFolder, CRASH_ARCHIVE_FOLDER),
			CRASH_ARCHIVE_FOLDER_KEY, _T("The archive folder for crash dunpes."))
			;

		settings.register_all();
		settings.notify();

		register_command(_T("check_nscp"), _T("Check the internal healt of NSClient++."));
	} catch (nscapi::nscapi_exception &e) {
		NSC_LOG_ERROR_STD(_T("Failed to register command: ") + utf8::cvt<std::wstring>(e.what()));
		return false;
	} catch (std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Exception: ") + utf8::cvt<std::wstring>(e.what()));
		return false;
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to register command."));
		return false;
	}
	return true;
}
bool CheckNSCP::unloadModule() {
	return true;
}
bool CheckNSCP::hasCommandHandler() {
	return true;
}
bool CheckNSCP::hasMessageHandler() {
	return true;
}
std::string CheckNSCP::render(int msgType, const std::string file, int line, std::string message) {
	return message;
}
void CheckNSCP::handleMessage(int msgType, const std::string file, int line, std::string message) {
	if (msgType > NSCAPI::log_level::error)
		return;
	std::string err = render(msgType, file, line, message);
	{
		boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
		if (!lock.owns_lock())
			return;
		errors_.push_back(err);
	}
}


int CheckNSCP::get_crashes(std::wstring &last_crash) {
#ifdef WIN32
	NSC_DEBUG_MSG(_T("Crash folder is: ") + crashFolder);
	if (!file_helpers::checks::is_directory(crashFolder)) {
		return 0;
	}
	WIN32_FIND_DATA wfd;
	FILETIME previous;
	previous.dwHighDateTime = 0;
	previous.dwLowDateTime = 0;
	int count = 0;
	std::wstring find = crashFolder + _T("\\*.txt");
	HANDLE hFind = FindFirstFile(find.c_str(), &wfd);
	std::wstring last_file;
	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			if (CompareFileTime(&wfd.ftCreationTime, &previous) == 1) {
				previous = wfd.ftCreationTime;
				last_file = wfd.cFileName;
			}
			count++;
		} while (FindNextFile(hFind, &wfd));
		FindClose(hFind);
	}
	if (count > 0)
		last_crash = last_file;
	return count;
#else
	return 0;
#endif
}

std::size_t CheckNSCP::get_errors(std::wstring &last_error) {
	boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!lock.owns_lock())
		return 1;
	if (errors_.empty())
		return 0;
	last_error = utf8::cvt<std::wstring>(errors_.front());
	return errors_.size();
}

NSCAPI::nagiosReturn CheckNSCP::check_nscp( std::list<std::wstring> arguments, std::wstring & msg, std::wstring & perf )
{
	std::wstring last_crash;
	int crash_count = get_crashes(last_crash);
	if (crash_count > 0){
		std::wstring tmp = strEx::itos(crash_count) + _T(" crash(es), last crash: ") + last_crash;
		strEx::append_list(msg, tmp, _T(", "));
	}

	std::wstring last_error;
	int err_count = get_errors(last_error);
	if (err_count > 0) {
		std::wstring tmp = strEx::itos(err_count) + _T(" error(s), last error: ") + last_error;
		strEx::append_list(msg, tmp, _T(", "));
	}

	if (msg.empty())
		msg = _T("OK: 0 crash(es), 0 error(s)");
	else
		msg = _T("ERROR: ") + msg;

	return (err_count > 0 || crash_count > 0) ? NSCAPI::returnCRIT:NSCAPI::returnOK;
}

NSCAPI::nagiosReturn CheckNSCP::handleCommand(const std::wstring &target, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &message, std::wstring &perf) {
	if (command == _T("check_nscp")) {
		return check_nscp(arguments, message, perf);
	}
	return NSCAPI::returnIgnored;
}

NSC_WRAP_DLL()
NSC_WRAPPERS_MAIN_DEF(CheckNSCP)
NSC_WRAPPERS_HANDLE_MSG_DEF()
NSC_WRAPPERS_HANDLE_CMD_DEF()
