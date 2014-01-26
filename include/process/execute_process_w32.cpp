#define BUFF_SIZE 4096

#include <string>

#include <buffer.hpp>
#include <Windows.h>
#include <NSCAPI.h>
#include <utf8.hpp>
#include <strEx.h>
#include <error.hpp>

#include <process/execute_process.hpp>

typedef hlp::buffer<char> buffer_type;

static std::string readFromFile(buffer_type &buffer, HANDLE hFile) {
	DWORD dwRead = 0;
	std::string str;
	do {
		DWORD retval = ReadFile(hFile, buffer, static_cast<DWORD>(buffer.size()), &dwRead, NULL);
		if (retval == 0 || dwRead <= 0 || dwRead > buffer.size())
			return str;
		buffer[dwRead] = 0;
		str += buffer;
	} while (dwRead == BUFF_SIZE);
	return str;
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
	sec.bInheritHandle = TRUE;
	sec.lpSecurityDescriptor = NULL;

	// Create Pipes
	CreatePipe(&hChildInR, &hChildInW, &sec, 0);
	CreatePipe(&hChildOutR, &hChildOutW, &sec, 0);

	// Set up members of STARTUPINFO structure. 

	ZeroMemory(&si, sizeof(STARTUPINFOW));
	si.cb = sizeof(STARTUPINFOW);
	si.dwFlags = STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;
	si.hStdInput = hChildInR;
	si.hStdOutput = hChildOutW;
	si.hStdError = hChildOutW;
	si.wShowWindow = SW_HIDE;


	// CreateProcess doesn't work with a const command
	TCHAR *cmd = new TCHAR[args.command.length()+1];
	wcsncpy(cmd, utf8::cvt<std::wstring>(args.command).c_str(), args.command.length());
	cmd[args.command.length()] = 0;

	// Create the child process.
	//HANDLE hWaitEvt = ::CreateEvent(NULL, TRUE, FALSE, NULL);
	BOOL processOK = FALSE;
	if (!args.user.empty()) {
		processOK = CreateProcessWithLogonW(utf8::cvt<std::wstring>(args.user).c_str(), utf8::cvt<std::wstring>(args.domain).c_str(), utf8::cvt<std::wstring>(args.password).c_str(), 
			LOGON_WITH_PROFILE, NULL, cmd, NULL, NULL, utf8::cvt<std::wstring>(args.root_path).c_str(), &si, &pi);
	} else {
		processOK = CreateProcess(NULL, cmd, NULL, NULL, TRUE, 0, NULL, utf8::cvt<std::wstring>(args.root_path).c_str(), &si, &pi);
	}

	delete [] cmd;
	if (processOK) {
		DWORD dwAvail = 0;
		std::string str;
		//HANDLE handles[2];
		//handles[0] = pi.hProcess;
		//handles[1] = hWaitEvt;
		buffer_type buffer(BUFF_SIZE);
		for (unsigned int i=0;i<args.timeout*10;i++) {
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

		if (dwstate == WAIT_TIMEOUT) {
			TerminateProcess(pi.hProcess, 5);
			output = "Command " + args.alias + " didn't terminate within the timeout period " + strEx::s::xtos(args.timeout) + "s";
			result = NSCAPI::returnUNKNOWN;
		} else {
			if (GetExitCodeProcess(pi.hProcess, &dwexitcode) == 0) {
				output = "Failed to get commands " + args.alias + " return code: " + utf8::cvt<std::string>(error::lookup::last_error());
				result = NSCAPI::returnUNKNOWN;
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
			output = "Failed to execute " + args.alias + " seems more like a script maybe you need a script executable first: " + utf8::cvt<std::string>(error::lookup::last_error(error));
		} else {
			output = "Failed to execute " + args.alias + ": " + utf8::cvt<std::string>(error::lookup::last_error(error));
		}
		result = NSCAPI::returnUNKNOWN;
		CloseHandle(hChildInR);
		CloseHandle(hChildInW);
		CloseHandle(hChildOutW);
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
		CloseHandle(hChildOutR);
	}
	return result;
}

