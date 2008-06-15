/**************************************************************************
*   Copyright (C) 2004-2008 by Michael Medin <michael@medin.name>         *
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
#pragma once

namespace process {
	class Exception {
		std::wstring error_;
	public:
		Exception(std::wstring error) : error_(error) {}
		std::wstring getMessage() {
			return error_;
		}
	};

#define BUFF_SIZE 4096

	char* createBuffer() {
		return new char[BUFF_SIZE+1];
	}
	void destroyBuffer(char* buffer) {
		delete [] buffer;
	}

	std::string readFromFile(char* buffer, HANDLE hFile) {
		DWORD dwRead = 0;
		DWORD retval = 0;
		std::string str;
		do {
			DWORD retval = ReadFile(hFile, buffer, BUFF_SIZE, &dwRead, NULL);
			if (retval == 0 || dwRead <= 0 || dwRead > BUFF_SIZE)
				return str;
			buffer[dwRead] = 0;
			str += buffer;
		} while (dwRead == BUFF_SIZE);
		return str;
	}

	int executeProcess(std::wstring root_path, std::wstring command, std::wstring &msg, std::wstring &perf, unsigned int timeout) {
		NSCAPI::nagiosReturn result;
		PROCESS_INFORMATION pi;
		STARTUPINFO si;
		HANDLE hChildOutR, hChildOutW, hChildInR, hChildInW;
		SECURITY_ATTRIBUTES sec;
		DWORD dwstate, dwexitcode;
		// Set up members of SECURITY_ATTRIBUTES structure. 
		sec.nLength = sizeof(SECURITY_ATTRIBUTES);
		sec.bInheritHandle = TRUE;
		sec.lpSecurityDescriptor = NULL;

		// Create Pipes
		CreatePipe(&hChildInR, &hChildInW, &sec, 0);
		CreatePipe(&hChildOutR, &hChildOutW, &sec, 0);

		// Set up members of STARTUPINFO structure. 

		ZeroMemory(&si, sizeof(STARTUPINFO));
		si.cb = sizeof(STARTUPINFO);
		si.dwFlags = STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;
		si.hStdInput = hChildInR;
		si.hStdOutput = hChildOutW;
		si.hStdError = hChildOutW;
		si.wShowWindow = SW_HIDE;


		// CreateProcess doesn't work with a const command
		TCHAR *cmd = new TCHAR[command.length()+1];
		wcsncpy_s(cmd, command.length()+1, command.c_str(), command.length());
		cmd[command.length()] = 0;

		// Create the child process.
		HANDLE hWaitEvt = ::CreateEvent(NULL, TRUE, FALSE, NULL);
		BOOL processOK = CreateProcess(NULL, cmd, NULL, NULL, TRUE, 0, NULL, root_path.c_str(), &si, &pi);
		delete [] cmd;
		if (processOK) {
			std::string str;
			HANDLE handles[2];
			handles[0] = pi.hProcess;
			handles[1] = hWaitEvt;
			char *buffer = createBuffer();
			for (unsigned int i=0;i<timeout;i++) {
				DWORD dwAvail = 0;
				if (!::PeekNamedPipe(hChildOutR, NULL, 0, NULL, &dwAvail, NULL))
					break;
				if (dwAvail > 0)
					str += readFromFile(buffer, hChildOutR);
				dwstate = WaitForSingleObject(pi.hProcess, 1000);
				if (dwstate != WAIT_TIMEOUT)
					break;
			}
			CloseHandle(hChildInR);
			CloseHandle(hChildInW);
			CloseHandle(hChildOutW);

			str += readFromFile(buffer, hChildOutR);
			msg = strEx::string_to_wstring(str);
			destroyBuffer(buffer);

			if (dwstate == WAIT_TIMEOUT) {
				TerminateProcess(pi.hProcess, 5);
				msg = _T("Command (") + command + _T(") didn't terminate within the timeout period (") + strEx::itos(timeout) + _T("s)!");
				result = NSCAPI::returnUNKNOWN;
			} else {
				if (msg.empty()) {
					msg = _T("No output available from command (") + command + _T(").");
				} else {
					strEx::token t = strEx::getToken(msg, '|');
					msg = t.first;
					std::wstring::size_type pos = msg.find_last_not_of(_T("\n\r "));
					if (pos != std::wstring::npos) {
						if (pos == msg.size())
							msg = msg.substr(0,pos);
						else
							msg = msg.substr(0,pos+1);
					}
					perf = t.second;
				}
				if (GetExitCodeProcess(pi.hProcess, &dwexitcode) == 0) {
					msg = _T("Failed to get commands (") + command + _T(") return code: ") + error::lookup::last_error();
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
				NSC_LOG_ERROR_STD(command + _T(" is not an .exe file or a valid image (if you run a script you usually need to prefix the command with the interpreter like so: \"command=c:\perl.exe <script>\""));
				msg = _T("ExternalCommands: failed to create process (") + command + _T("): it is not an exe file (check NSC.log for more info) - ") + error::lookup::last_error(error);
			} else {
				msg = _T("ExternalCommands: failed to create process (") + command + _T("): ") + error::lookup::last_error(error);
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
}

