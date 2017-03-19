/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#define BUFF_SIZE 4096

#include <string>

#include <buffer.hpp>
#include <Windows.h>
#include <Userenv.h>
#include <NSCAPI.h>
#include <utf8.hpp>
#include <str/xtos.hpp>
#include <error/error.hpp>
#include <handle.hpp>
#include <char_buffer.hpp>
#include <str/xtos.hpp>

#include <iostream>

#include <process/execute_process.hpp>
#include <win_sysinfo/win_sysinfo.hpp>

#include <boost/thread.hpp>

typedef hlp::buffer<char> buffer_type;

struct generic_closer {
	static void close(HANDLE handle) {
		::CloseHandle(handle);
	}
};
typedef hlp::handle<HANDLE, generic_closer> generic_handle;

struct env_closer {
	static void close(LPVOID handle) {
		::DestroyEnvironmentBlock(handle);
	}
};

typedef hlp::handle<LPVOID, env_closer> env_handle;



struct impersonator {
	bool active;
	impersonator(HANDLE token) : active(false){
		if (ImpersonateLoggedOnUser(token)) {
			active = true;
		}
	}
	~impersonator() {
		close();
	}

	void close() {
		if (active) {
			RevertToSelf();
		}
		active = false;
	}

	bool isActive() const {
		return active;
	}

};

static std::string readFromFile(buffer_type &buffer, HANDLE hFile) {
	DWORD dwRead = 0;
	std::string str;
	DWORD chunk_size = buffer.size() - 10;
	do {
		DWORD retval = ReadFile(hFile, buffer, chunk_size, &dwRead, NULL);
		if (retval == 0 || dwRead <= 0 || dwRead > chunk_size)
			return str;
		buffer[dwRead] = 0;
		str += buffer;
	} while (dwRead == chunk_size);
	return str;
}


boost::timed_mutex mutex_;
std::list<HANDLE> pids_;

void process::kill_all() {
	boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!lock.owns_lock())
		return;
	BOOST_FOREACH(const HANDLE &h, pids_) {
		TerminateProcess(h, 5);
	}
}

void register_proc(HANDLE hProcess) {
	boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(1));
	if (!lock.owns_lock())
		return;
	pids_.push_back(hProcess);
}
void remove_proc(HANDLE hProcess) {
	boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(1));
	if (!lock.owns_lock())
		return;
	pids_.remove_if([hProcess](HANDLE hOther) { return hOther == hProcess; });
}
int process::execute_process(process::exec_arguments args, std::string &output) {
	generic_handle hChildOutR, hChildOutW, hChildInR, hChildInW;
	generic_handle pHandle;

	if (!args.session.empty()) {
		if (!windows::winapi::WTSQueryUserToken(windows::winapi::WTSGetActiveConsoleSessionId(), pHandle.ref())) {
			output = "Failed to WTSQueryUserToken: " + error::lookup::last_error();
			return NSCAPI::query_return_codes::returnUNKNOWN;
		}
	} else if (!args.user.empty()) {
		generic_handle tmpHandle;
		if (!LogonUser(utf8::cvt<std::wstring>(args.user).c_str(), utf8::cvt<std::wstring>(args.domain).c_str(), utf8::cvt<std::wstring>(args.password).c_str(), LOGON32_LOGON_INTERACTIVE, LOGON32_PROVIDER_DEFAULT, tmpHandle.ref())) {
			output = "Failed to login as " + args.user + ": " + error::lookup::last_error();
			return NSCAPI::query_return_codes::returnUNKNOWN;
		}

		if (!DuplicateTokenEx(tmpHandle, MAXIMUM_ALLOWED, 0, SecurityImpersonation, TokenPrimary, pHandle.ref())) {
			output = "Failed to duplicate token for " + args.user + ": " + error::lookup::last_error();
			return NSCAPI::query_return_codes::returnUNKNOWN;
		}
	}

	SECURITY_ATTRIBUTES sec;
	sec.nLength = sizeof(SECURITY_ATTRIBUTES);
	sec.bInheritHandle = FALSE;
	sec.lpSecurityDescriptor = NULL;
	if (!args.fork) {
		sec.bInheritHandle = TRUE;
		CreatePipe(hChildInR.ref(), hChildInW.ref(), &sec, 0);
		CreatePipe(hChildOutR.ref(), hChildOutW.ref(), &sec, 0);
	}

	STARTUPINFO si;
	ZeroMemory(&si, sizeof(STARTUPINFOW));
	si.cb = sizeof(STARTUPINFOW);
	if (args.fork) {
		si.dwFlags = STARTF_USESHOWWINDOW;
	} else {
		si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
		si.hStdInput = hChildInR.get();
		si.hStdOutput = hChildOutW.get();
		si.hStdError = hChildOutW.get();
	}
	si.wShowWindow = SW_HIDE;
	if (args.display)
		si.wShowWindow = SW_SHOW;

	hlp::tchar_buffer tmpCmd(utf8::cvt<std::wstring>(args.command));
	tmpCmd[args.command.length()] = 0;

	BOOL processOK = FALSE;
	PROCESS_INFORMATION pi;
	env_handle enviornment;
	if (pHandle) {
		impersonator imp(pHandle);
		if (!imp.isActive()) {
			output = "Failed to impersonate " + args.user + ": " + error::lookup::last_error();
			return NSCAPI::query_return_codes::returnUNKNOWN;
		}

		if (!CreateEnvironmentBlock(enviornment.ref(), pHandle.get(), FALSE)) {
			output = "Failed to create enviornment for " + args.user + ": " + error::lookup::last_error();
			return NSCAPI::query_return_codes::returnUNKNOWN;
		}

		processOK = CreateProcessAsUser(pHandle.get(), NULL, tmpCmd.get(), NULL, NULL, args.fork ? FALSE : TRUE, CREATE_UNICODE_ENVIRONMENT, enviornment.get(), utf8::cvt<std::wstring>(args.root_path).c_str(), &si, &pi);
		if (!processOK) {
			imp.close();
			DWORD error = GetLastError();
			if (error == ERROR_PRIVILEGE_NOT_HELD) {
				processOK = CreateProcessWithLogonW(utf8::cvt<std::wstring>(args.user).c_str(), utf8::cvt<std::wstring>(args.domain).c_str(), utf8::cvt<std::wstring>(args.password).c_str(),
				LOGON_WITH_PROFILE, NULL, tmpCmd.get(), NULL, NULL, utf8::cvt<std::wstring>(args.root_path).c_str(), &si, &pi);
			} else {
				if (error == ERROR_BAD_EXE_FORMAT) {
					output = "Failed to execute " + args.alias + " seems more like a script maybe you need a script executable first: " + error::lookup::last_error(error);
				} else {
					output = "Failed to execute " + args.alias + ": " + error::lookup::last_error(error);
				}
				return NSCAPI::query_return_codes::returnUNKNOWN;
			}
		}
	} else {
		processOK = CreateProcess(NULL, tmpCmd.get(), NULL, NULL, args.fork ? FALSE : TRUE, 0, NULL, utf8::cvt<std::wstring>(args.root_path).c_str(), &si, &pi);
	}

	if (processOK) {
		DWORD dwstate = 0;
		if (args.fork) {
			output = "Command started successfully";
			return NSCAPI::query_return_codes::returnOK;
		}
		register_proc(pi.hProcess);
		DWORD dwAvail = 0;
		std::string str;
		buffer_type buffer(BUFF_SIZE);
		for (unsigned int i = 0; i < args.timeout * 10; i++) {
			if (!::PeekNamedPipe(hChildOutR.get(), NULL, 0, NULL, &dwAvail, NULL)) {
				break;
			}
			if (dwAvail > 0) {
				str += readFromFile(buffer, hChildOutR.get());
			}
			if (dwAvail == 0) {
				dwstate = WaitForSingleObject(pi.hProcess, 100);
				if (dwstate != WAIT_TIMEOUT) {
					break;
				}
			}
		}
		hChildInW.close();
		hChildInR.close();
		hChildOutW.close();

		dwAvail = 0;
		if (::PeekNamedPipe(hChildOutR.get(), NULL, 0, NULL, &dwAvail, NULL) && dwAvail > 0) {
			str += readFromFile(buffer, hChildOutR.get());
		}
		output = utf8::cvt<std::string>(utf8::from_encoding(str, args.encoding));

		remove_proc(pi.hProcess);
		CloseHandle(pi.hThread);
		if (dwstate == WAIT_TIMEOUT) {
			TerminateProcess(pi.hProcess, 5);
			output = "Command " + args.alias + " didn't terminate within the timeout period " + str::xtos(args.timeout) + "s";
			return NSCAPI::query_return_codes::returnUNKNOWN;
		} else {
			NSCAPI::nagiosReturn result;
			DWORD dwexitcode = 0;
			if (GetExitCodeProcess(pi.hProcess, &dwexitcode) == 0) {
				output = "Failed to get commands " + args.alias + " return code: " + error::lookup::last_error();
				result = NSCAPI::query_return_codes::returnUNKNOWN;
			} else {
				result = dwexitcode;
			}
			CloseHandle(pi.hProcess);
			return result;
		}
	} else {
		DWORD error = GetLastError();
		if (error == ERROR_BAD_EXE_FORMAT) {
			output = "Failed to execute " + args.alias + " seems more like a script maybe you need a script executable first: " + error::lookup::last_error(error);
		} else {
			output = "Failed to execute " + args.alias + ": " + error::lookup::last_error(error);
		}
		return NSCAPI::query_return_codes::returnUNKNOWN;
	}
}