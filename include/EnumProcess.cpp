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
#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#include <windows.h>
#include <tchar.h>
#include <iostream>

#include <buffer.hpp>
#include <handle.hpp>
#include <error.hpp>
#include <win_sysinfo/win_defines.hpp>
#include <win_sysinfo/win_sysinfo.hpp>

#include "EnumProcess.h"

#include <Psapi.h>

const int MAX_FILENAME = 256;

void enable_token_privilege(LPTSTR privilege) {
	hlp::generic_handle<> token;
	TOKEN_PRIVILEGES token_privileges;
	DWORD dwSize;
	ZeroMemory(&token_privileges, sizeof(token_privileges));
	token_privileges.PrivilegeCount = 1;
	if ( !OpenProcessToken (GetCurrentProcess(), TOKEN_ALL_ACCESS, token.ref()))
		throw nscp_exception("Failed to open process token: " + error::lookup::last_error());
	if (!LookupPrivilegeValue(NULL, privilege, &token_privileges.Privileges[0].Luid)) { 
		throw nscp_exception("Failed to lookup privilege: " + error::lookup::last_error());
	}
	token_privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	if (!AdjustTokenPrivileges(token, FALSE, &token_privileges, 0, NULL, &dwSize)) { 
		throw nscp_exception("Failed to adjust token privilege: " + error::lookup::last_error());
	}
}

void disable_token_privilege(LPTSTR privilege) {
	hlp::generic_handle<> token;
	TOKEN_PRIVILEGES token_privileges;                 
	DWORD dwSize;                       
	ZeroMemory (&token_privileges, sizeof (token_privileges));
	token_privileges.PrivilegeCount = 1;
	if ( !OpenProcessToken (GetCurrentProcess(), TOKEN_ALL_ACCESS, token.ref()))
		throw nscp_exception("Failed to open process token: " + error::lookup::last_error());
	if (!LookupPrivilegeValue(NULL, privilege, &token_privileges.Privileges[0].Luid))
		throw nscp_exception("Failed to lookup privilege: " + error::lookup::last_error());
	token_privileges.Privileges[0].Attributes = SE_PRIVILEGE_REMOVED;
	if (!AdjustTokenPrivileges(token, FALSE, &token_privileges, 0, NULL, &dwSize))
		throw nscp_exception("Failed to adjust token privilege: " + error::lookup::last_error());
}

namespace process_helper {
	struct find_16bit_container {
		std::list<process_info> *target;
		DWORD pid;
	};
	BOOL CALLBACK Enum16Proc( DWORD dwThreadId, WORD hMod16, WORD hTask16, PSZ pszModName, PSZ pszFileName, LPARAM lpUserDefined )
	{
		find_16bit_container *container = reinterpret_cast<find_16bit_container*>(lpUserDefined);
		process_info pEntry;
		pEntry.pid = container->pid;
		std::string tmp = pszFileName;
		pEntry.command_line = tmp;
		std::string::size_type pos = tmp.find_last_of("\\");
		if (pos != std::string::npos)
			pEntry.filename = tmp.substr(++pos);
		else
			pEntry.filename = tmp;
		container->target->push_back(pEntry);
		return FALSE;
	}


	struct enum_data {
		error_reporter * error_interface;
		std::vector<DWORD> crashed_pids;

	};

	BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam ) {
		enum_data *data = reinterpret_cast<enum_data*>(lParam);
		DWORD pid;
		GetWindowThreadProcessId(hwnd, &pid);
		if (GetWindow(hwnd, GW_OWNER) != NULL)
			return TRUE;
		PDWORD result;
		if (!SendMessageTimeout(hwnd, WM_NULL, 0, 0, SMTO_ABORTIFHUNG, 500, reinterpret_cast<PDWORD_PTR>(&result))) {
			if (data->error_interface!=NULL)
				data->error_interface->report_debug("pid: " + strEx::s::xtos(pid) + " was hung");
			data->crashed_pids.push_back(pid);
		}
		return TRUE;
	}

	std::vector<DWORD> find_crashed_pids(error_reporter *error_interface) {
		enum_data data;
		data.error_interface = error_interface;
		if(!EnumWindows(&EnumWindowsProc, reinterpret_cast<LPARAM>(&data))) {
			if (error_interface)
				error_interface->report_error("Failed to enumerate windows: " + utf8::cvt<std::string>(error::lookup::last_error()));
		}
		return data.crashed_pids;
	}



	void nscpGetCommandLine(HANDLE hProcess, LPVOID pebAddress, process_info &entry) {
		windows::winapi::UNICODE_STRING commandLine;
		LPVOID rtlUserProcParamsAddress;
#ifdef _WIN64
		if (!ReadProcessMemory(hProcess, (PCHAR)pebAddress + 0x20, &rtlUserProcParamsAddress, sizeof(LPVOID), NULL))
			entry.set_error("Could not read the address of ProcessParameters: " + error::lookup::last_error());
		if (!ReadProcessMemory(hProcess, (PCHAR)rtlUserProcParamsAddress + 0x70, &commandLine, sizeof(commandLine), NULL))
			entry.set_error("Could not read command line: " + error::lookup::last_error());
#else
		if (!entry.wow64)
			return;
		if (!ReadProcessMemory(hProcess, (PCHAR)pebAddress + 0x10, &rtlUserProcParamsAddress, sizeof(LPVOID), NULL))
			entry.set_error("Could not read the address of ProcessParameters: " + error::lookup::last_error());
		if (!ReadProcessMemory(hProcess, (PCHAR)rtlUserProcParamsAddress + 0x40, &commandLine, sizeof(commandLine), NULL))
			entry.set_error("Could not read command line: " + error::lookup::last_error());
#endif

		/* allocate memory to hold the command line */
		wchar_t *commandLineContents = new wchar_t[commandLine.Length+2];
		memset(commandLineContents, 0, commandLine.Length);

		/* read the command line */
		if (!ReadProcessMemory(hProcess, commandLine.Buffer, commandLineContents, commandLine.Length, NULL)) {
			delete [] commandLineContents;
			throw nscp_exception("Could not read command line string: " + error::lookup::last_error());
		}

		commandLineContents[(commandLine.Length/sizeof(WCHAR))] = '\0';
		entry.command_line = utf8::cvt<std::string>(commandLineContents);
		delete [] commandLineContents;
	}


	process_info describe_pid(DWORD pid, bool deep_scan) {
		process_info entry;
		entry.pid = pid;
		entry.started = true;
		// Open process to get filename
		DWORD openArgs = PROCESS_QUERY_INFORMATION|PROCESS_VM_READ;
 		if (deep_scan)
 			openArgs |= PROCESS_VM_OPERATION;

		unsigned long ReturnLength = 0;

		hlp::generic_handle<> handle(OpenProcess(openArgs, FALSE, pid));
		if (!handle) {
			handle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
			if (!handle) {
				entry.set_error("Failed to open process " + strEx::s::xtos(pid) + ": " + error::lookup::last_error());
				entry.unreadable = true;
				return entry;
			}
		}


		hlp::buffer<wchar_t> nmbuffer(MAX_PATH);
		DWORD len = GetProcessImageFileName(handle, nmbuffer, nmbuffer.size());
		if (len > 0) {
			nmbuffer[len] = 0;
			std::string tmp =utf8::cvt<std::string>(std::wstring(nmbuffer.get())); 
			entry.filename = tmp;
			std::size_t pos = tmp.find_last_of('\\');
			if (pos != std::string::npos)
				entry.exe = tmp.substr(pos+1);
			else
				entry.exe = tmp;
		}

		LPVOID PebBaseAddress = NULL;
		windows::winapi::PROCESS_EXTENDED_BASIC_INFORMATION pebi;
		if (windows::winapi::NtQueryInformationProcess(handle, windows::winapi::ProcessBasicInformation, &pebi, sizeof(windows::winapi::PROCESS_EXTENDED_BASIC_INFORMATION), NULL) >= 0) {
			PebBaseAddress = pebi.BasicInfo.PebBaseAddress;
			//entry.handleCount = handleCount;
		}

		if (deep_scan) {


			DWORD handleCount;
			if (GetProcessHandleCount(handle, &handleCount)) {
				entry.handleCount = handleCount;
			}
			entry.gdiHandleCount = GetGuiResources(handle, GR_GDIOBJECTS);
			entry.userHandleCount = GetGuiResources(handle, GR_USEROBJECTS);

			FILETIME creationTime;
			FILETIME exitTime;
			FILETIME kernelTime;
			FILETIME userTime;
			if (GetProcessTimes(handle, &creationTime, &exitTime, &kernelTime, &userTime)) {
				entry.creation_time = format::filetime_to_time((creationTime.dwHighDateTime * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)creationTime.dwLowDateTime);
				entry.kernel_time = ((kernelTime.dwHighDateTime * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)kernelTime.dwLowDateTime)/10000000;
				entry.user_time = ((userTime.dwHighDateTime * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)userTime.dwLowDateTime)/10000000;
			}


			SIZE_T minimumWorkingSetSize;
			SIZE_T maximumWorkingSetSize;
			if (GetProcessWorkingSetSize(handle, &minimumWorkingSetSize, &maximumWorkingSetSize)) {

			}

			IO_COUNTERS ioc;
			memset(&ioc, 0, sizeof(ioc));
			if (windows::winapi::NtQueryInformationProcess(handle, windows::winapi::ProcessIoCounters, &ioc, sizeof(IO_COUNTERS), &ReturnLength) >= 0) {
				entry.otherOperationCount = ioc.OtherOperationCount;
				entry.otherTransferCount = ioc.OtherTransferCount;
				entry.readOperationCount = ioc.ReadOperationCount;
				entry.readTransferCount = ioc.ReadTransferCount;
				entry.writeOperationCount = ioc.WriteOperationCount;
				entry.writeTransferCount = ioc.WriteTransferCount;
			}
			typedef LONG NTSTATUS;
			windows::winapi::VM_COUNTERS vmc;
			memset(&vmc, 0, sizeof(vmc));
			if (windows::winapi::NtQueryInformationProcess(handle, windows::winapi::ProcessVmCounters, &vmc, sizeof(windows::winapi::VM_COUNTERS), &ReturnLength) >= 0) {
				entry.PeakVirtualSize = vmc.PeakVirtualSize;
				entry.VirtualSize = vmc.VirtualSize;
				entry.PageFaultCount = vmc.PageFaultCount;
				entry.PeakWorkingSetSize = vmc.PeakWorkingSetSize;
				entry.WorkingSetSize = vmc.WorkingSetSize;
				entry.QuotaPeakPagedPoolUsage = vmc.QuotaPeakPagedPoolUsage;
				entry.QuotaPagedPoolUsage = vmc.QuotaPagedPoolUsage;
				entry.QuotaPeakNonPagedPoolUsage = vmc.QuotaPeakNonPagedPoolUsage;
				entry.QuotaNonPagedPoolUsage = vmc.QuotaNonPagedPoolUsage;
				entry.PagefileUsage = vmc.PagefileUsage;
				entry.PeakPagefileUsage = vmc.PeakPagefileUsage;
			}
			// 	KERNEL_USER_TIMES kut;
			// 	memset(&kut, 0x00, sizeof(kut));
			// 	ReturnLength = 0;
			// 	reVal = nscpNtQueryInformationProcess(hProc, ProcessTimes, &kut, sizeof(KERNEL_USER_TIMES), &ReturnLength);

			// 	PROCESS_CYCLE_TIME_INFORMATION pct;
			// 	memset(&pct, 0x00, sizeof(pct));
			// 	ReturnLength = 0;
			// 	reVal = nscpNtQueryInformationProcess(hProc, ProcessCycleTime, &pct, sizeof(PROCESS_CYCLE_TIME_INFORMATION), &ReturnLength);


			bool osIsWin64 = windows::winapi::IsWow64(GetCurrentProcess());
			entry.wow64 = windows::winapi::IsWow64(handle, !osIsWin64);


			if (PebBaseAddress)
				nscpGetCommandLine(handle, PebBaseAddress, entry);
		}


		HMODULE hMod;
		DWORD size;
		// Get the first module (the process itself)
		if(EnumProcessModules(handle, &hMod, sizeof(hMod), &size)) {
			TCHAR buffer[MAX_FILENAME+1];
			if(!GetModuleFileNameEx(handle, hMod, reinterpret_cast<LPTSTR>(&buffer), MAX_FILENAME)) {
				CloseHandle(handle);
				throw nscp_exception("Failed to find name for: " + strEx::s::xtos(pid) + ": " + error::lookup::last_error());
			} else {
				std::wstring path = buffer;
				std::wstring::size_type pos = path.find_last_of(_T("\\"));
				if (pos != std::wstring::npos) {
					path = path.substr(++pos);
				}
				entry.filename = utf8::cvt<std::string>(path);
			}
		}
		return entry;
	}

	process_list enumerate_processes(bool expand_command_line, bool find_16bit, error_reporter *error_interface, unsigned int buffer_size) {
		try {
			enable_token_privilege(SE_DEBUG_NAME);
		} catch (nscp_exception &e) {
			if (error_interface!=NULL)
				error_interface->report_warning(e.reason());
		} 

		std::list<process_info> ret;
		DWORD *dwPIDs = new DWORD[buffer_size+1];
		DWORD cbNeeded = 0;
		BOOL OK = EnumProcesses(dwPIDs, buffer_size*sizeof(DWORD), &cbNeeded);
		if (cbNeeded >= DEFAULT_BUFFER_SIZE*sizeof(DWORD)) {
			delete [] dwPIDs;
			if (error_interface!=NULL)
				error_interface->report_debug("Need larger buffer: " + strEx::s::xtos(buffer_size));
			return enumerate_processes(expand_command_line, find_16bit, error_interface, buffer_size * 10); 
		}
		if (!OK) {
			delete [] dwPIDs;
			throw nscp_exception("Failed to enumerate process: " + error::lookup::last_error());
		}
		unsigned int process_count = cbNeeded/sizeof(DWORD);
		for (unsigned int i = 0;i <process_count; ++i) {
			if (dwPIDs[i] == 0)
				continue;
			process_info entry;
			entry.hung = false;
			try {
				try {
					entry = describe_pid(dwPIDs[i], expand_command_line);
				} catch (const nscp_exception &e) {
					if (error_interface!=NULL)
						error_interface->report_debug(e.reason());
					if (expand_command_line) {
						try {
					entry = describe_pid(dwPIDs[i], false);
						} catch (const nscp_exception &e) {
							if (error_interface!=NULL)
								error_interface->report_debug(e.reason());
						}
					}
				}
	// 			if (error_interface!=NULL)
	// 				error_interface->report_debug_exit(_T("describe_pid"));
				if (find_16bit) {
					if(stricmp(entry.filename.get().substr(0,9).c_str(), "NTVDM.EXE") == 0) {
						find_16bit_container container;
						container.target = &ret;
						container.pid = entry.pid;
						windows::winapi::VDMEnumTaskWOWEx(container.pid, (windows::winapi::tTASKENUMPROCEX)&Enum16Proc, (LPARAM) &container);
					}
				}
				ret.push_back(entry);
			} catch (const nscp_exception &e) {
				if (error_interface!=NULL)
					error_interface->report_error("Exception describing PID: " + strEx::s::xtos(dwPIDs[i]) + ": " + e.reason());
			} catch (...) {
				if (error_interface!=NULL)
					error_interface->report_error("Unknown exception describing PID: " + strEx::s::xtos(dwPIDs[i]));
			}
		}

		std::vector<DWORD> hung_pids = find_crashed_pids(error_interface);
		for (process_list::iterator entry = ret.begin(); entry != ret.end(); ++entry) {
			if (std::find(hung_pids.begin(), hung_pids.end(), entry->pid) != hung_pids.end())
				(*entry).hung = true;
			else
				(*entry).hung = false;
		}

		delete [] dwPIDs;
		return ret;
	}
}