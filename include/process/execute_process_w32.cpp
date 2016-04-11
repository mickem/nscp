#define BUFF_SIZE 4096

#include <string>

#include <buffer.hpp>
#include <Windows.h>
#include <NSCAPI.h>
#include <utf8.hpp>
#include <strEx.h>
#include <error.hpp>

#include <iostream>

#include <process/execute_process.hpp>
#include <win_sysinfo/win_sysinfo.hpp>

#include <boost/thread.hpp>

typedef hlp::buffer<char> buffer_type;

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
	NSCAPI::nagiosReturn result;
	PROCESS_INFORMATION pi;
	STARTUPINFO si;
	HANDLE hChildOutR, hChildOutW, hChildInR, hChildInW;
	SECURITY_ATTRIBUTES sec;
	DWORD dwstate = 0, dwexitcode;
	// Set up members of SECURITY_ATTRIBUTES structure.
	sec.nLength = sizeof(SECURITY_ATTRIBUTES);
	sec.bInheritHandle = FALSE;
	sec.lpSecurityDescriptor = NULL;

	// Create Pipes
	if (!args.fork) {
		sec.bInheritHandle = TRUE;
		CreatePipe(&hChildInR, &hChildInW, &sec, 0);
		CreatePipe(&hChildOutR, &hChildOutW, &sec, 0);
	}

	// Set up members of STARTUPINFO structure.

	ZeroMemory(&si, sizeof(STARTUPINFOW));
	si.cb = sizeof(STARTUPINFOW);
	if (args.fork) {
		si.dwFlags = STARTF_USESHOWWINDOW;
	} else {
		si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
		si.hStdInput = hChildInR;
		si.hStdOutput = hChildOutW;
		si.hStdError = hChildOutW;
	}
	si.wShowWindow = SW_HIDE;
	if (args.display)
		si.wShowWindow = SW_SHOW;

	// CreateProcess doesn't work with a const command
	TCHAR *cmd = new TCHAR[args.command.length() + 1];
	wcsncpy(cmd, utf8::cvt<std::wstring>(args.command).c_str(), args.command.length());
	cmd[args.command.length()] = 0;

	// Create the child process.
	//HANDLE hWaitEvt = ::CreateEvent(NULL, TRUE, FALSE, NULL);
	BOOL processOK = FALSE;
	if (!args.session.empty()) {
		HANDLE pHandle;
		if (!windows::winapi::WTSQueryUserToken(windows::winapi::WTSGetActiveConsoleSessionId(), &pHandle)) {
			output = "Failed to WTSQueryUserToken: " + error::lookup::last_error();
			return 0;
		}

		processOK = CreateProcessAsUser(pHandle, NULL, cmd, NULL, NULL, NULL, 0, NULL, utf8::cvt<std::wstring>(args.root_path).c_str(), &si, &pi);
		CloseHandle(pHandle);
	} else if (!args.user.empty()) {
		processOK = CreateProcessWithLogonW(utf8::cvt<std::wstring>(args.user).c_str(), utf8::cvt<std::wstring>(args.domain).c_str(), utf8::cvt<std::wstring>(args.password).c_str(),
			LOGON_WITH_PROFILE, NULL, cmd, NULL, NULL, utf8::cvt<std::wstring>(args.root_path).c_str(), &si, &pi);
	} else {
		processOK = CreateProcess(NULL, cmd, NULL, NULL, args.fork?FALSE:TRUE, 0, NULL, utf8::cvt<std::wstring>(args.root_path).c_str(), &si, &pi);
	}

	delete[] cmd;
	if (processOK) {
		if (args.fork) {
			output = "Command started successfully";
			return NSCAPI::query_return_codes::returnOK;
		}
		register_proc(pi.hProcess);
		DWORD dwAvail = 0;
		std::string str;
		buffer_type buffer(BUFF_SIZE);
		for (unsigned int i = 0; i < args.timeout * 10; i++) {
			if (!::PeekNamedPipe(hChildOutR, NULL, 0, NULL, &dwAvail, NULL))
				break;
			if (dwAvail > 0)
				str += readFromFile(buffer, hChildOutR);
			if (dwAvail == 0) {
				dwstate = WaitForSingleObject(pi.hProcess, 100);
				if (dwstate != WAIT_TIMEOUT)
					break;
			}
		}
		CloseHandle(hChildInR);
		CloseHandle(hChildInW);
		CloseHandle(hChildOutW);

		dwAvail = 0;
		if (::PeekNamedPipe(hChildOutR, NULL, 0, NULL, &dwAvail, NULL) && dwAvail > 0)
			str += readFromFile(buffer, hChildOutR);
		output = utf8::cvt<std::string>(utf8::from_encoding(str, args.encoding));

		remove_proc(pi.hProcess);
		if (dwstate == WAIT_TIMEOUT) {
			TerminateProcess(pi.hProcess, 5);
			output = "Command " + args.alias + " didn't terminate within the timeout period " + strEx::s::xtos(args.timeout) + "s";
			result = NSCAPI::query_return_codes::returnUNKNOWN;
		} else {
			if (GetExitCodeProcess(pi.hProcess, &dwexitcode) == 0) {
				output = "Failed to get commands " + args.alias + " return code: " + error::lookup::last_error();
				result = NSCAPI::query_return_codes::returnUNKNOWN;
			} else {
				result = dwexitcode;
			}
		}
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
		CloseHandle(hChildOutR);
	} else {
		DWORD error = GetLastError();
		if (error == ERROR_BAD_EXE_FORMAT) {
			output = "Failed to execute " + args.alias + " seems more like a script maybe you need a script executable first: " + error::lookup::last_error(error);
		} else {
			output = "Failed to execute " + args.alias + ": " + error::lookup::last_error(error);
		}
		result = NSCAPI::query_return_codes::returnUNKNOWN;
		if (!args.fork) {
			CloseHandle(hChildInR);
			CloseHandle(hChildInW);
			CloseHandle(hChildOutW);
			CloseHandle(pi.hThread);
			CloseHandle(pi.hProcess);
			CloseHandle(hChildOutR);
		}
	}
	return result;
}