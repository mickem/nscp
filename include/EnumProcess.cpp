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

#include <win_sysinfo/win_defines.hpp>
#include <win_sysinfo/win_sysinfo.hpp>

#include "EnumProcess.h"

#include <buffer.hpp>
#include <handle.hpp>
#include <nsclient/nsclient_exception.hpp>
#include <utf8.hpp>
#include <str/xtos.hpp>
#include <error/error.hpp>
#include <str/format.hpp>

#include <map>
#include <string>
#include <iostream>

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#include <windows.h>
#include <tchar.h>

#include <Psapi.h>

const int MAX_FILENAME = 256;

struct generic_closer {
	static void close(HANDLE handle) {
		CloseHandle(handle);
	}
};
typedef hlp::handle<HANDLE, generic_closer> generic_handle;

namespace process_helper {
	void enable_token_privilege(LPTSTR privilege, bool enable) {
		generic_handle token;
		TOKEN_PRIVILEGES token_privileges;
		LUID luid;
		if (!LookupPrivilegeValue(NULL, privilege, &luid))
			throw nsclient::nsclient_exception("Failed to lookup privilege: " + error::lookup::last_error());
		ZeroMemory(&token_privileges, sizeof(TOKEN_PRIVILEGES));
		token_privileges.PrivilegeCount = 1;
		token_privileges.Privileges[0].Luid = luid;
		if (enable)
			token_privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
		else
			token_privileges.Privileges[0].Attributes = 0;
		if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, token.ref()))
			throw nsclient::nsclient_exception("Failed to open process token: " + error::lookup::last_error());
		if (!AdjustTokenPrivileges(token.get(), FALSE, &token_privileges, sizeof(TOKEN_PRIVILEGES), (PTOKEN_PRIVILEGES)NULL, (PDWORD)NULL))
			throw nsclient::nsclient_exception("Failed to adjust token privilege: " + error::lookup::last_error());
	}

	struct find_16bit_container {
		std::list<process_info> *target;
		DWORD pid;
	};
	BOOL CALLBACK Enum16Proc(DWORD, WORD, WORD, PSZ, PSZ pszFileName, LPARAM lpUserDefined) {
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

	BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
		enum_data *data = reinterpret_cast<enum_data*>(lParam);
		if (!IsWindowVisible(hwnd))
			return TRUE;
		if (GetWindow(hwnd, GW_OWNER) != NULL)
			return TRUE;
		if (IsHungAppWindow(hwnd)) {
			DWORD pid = 0;
			GetWindowThreadProcessId(hwnd, &pid);
			data->crashed_pids.push_back(pid);
		}
		return TRUE;
	}

	std::vector<DWORD> find_crashed_pids(error_reporter *error_interface) {
		enum_data data;
		data.error_interface = error_interface;
		if (!EnumWindows(&EnumWindowsProc, reinterpret_cast<LPARAM>(&data))) {
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
		wchar_t *commandLineContents = new wchar_t[commandLine.Length + 2];
		memset(commandLineContents, 0, commandLine.Length);

		/* read the command line */
		if (!ReadProcessMemory(hProcess, commandLine.Buffer, commandLineContents, commandLine.Length, NULL)) {
			delete[] commandLineContents;
			throw nsclient::nsclient_exception("Could not read command line string: " + error::lookup::last_error());
		}

		commandLineContents[(commandLine.Length / sizeof(WCHAR))] = '\0';
		entry.command_line = utf8::cvt<std::string>(commandLineContents);
		delete[] commandLineContents;
	}

	process_info describe_pid(DWORD pid, bool deep_scan, bool ignore_unreadable) {
		process_info entry;
		entry.pid = pid;
		entry.started = true;
		// Open process to get filename
		DWORD openArgs = PROCESS_QUERY_INFORMATION | PROCESS_VM_READ;
		if (deep_scan)
			openArgs |= PROCESS_VM_OPERATION;

		unsigned long ReturnLength = 0;

		generic_handle handle(OpenProcess(openArgs, FALSE, pid));
		if (!handle) {
			handle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
			if (!handle) {
				DWORD err = GetLastError();
				entry.unreadable = true;
				if (!ignore_unreadable || err != ERROR_ACCESS_DENIED)
					entry.set_error("Failed to open process " + str::xtos(pid) + ": " + error::lookup::last_error());
				return entry;
			}
		}

		hlp::buffer<wchar_t> nmbuffer(MAX_PATH);
		DWORD len = GetProcessImageFileName(handle, nmbuffer, nmbuffer.size());
		if (len > 0) {
			nmbuffer[len] = 0;
			std::string tmp = utf8::cvt<std::string>(std::wstring(nmbuffer.get()));
			entry.filename = tmp;
			std::size_t pos = tmp.find_last_of('\\');
			if (pos != std::string::npos)
				entry.exe = tmp.substr(pos + 1);
			else
				entry.exe = tmp;
		}

		LPVOID PebBaseAddress = NULL;
		windows::winapi::PROCESS_BASIC_INFORMATION pbi;
		memset(&pbi, 0, sizeof(pbi));
		if (windows::winapi::NtQueryInformationProcess(handle, windows::winapi::ProcessBasicInformation, &pbi, sizeof(windows::winapi::PROCESS_BASIC_INFORMATION), &ReturnLength) >= 0) {
			PebBaseAddress = pbi.PebBaseAddress;
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
				entry.kernel_time_raw = (kernelTime.dwHighDateTime * ((unsigned long long)MAXDWORD + 1)) + (unsigned long long)kernelTime.dwLowDateTime;
				entry.user_time_raw = (userTime.dwHighDateTime * ((unsigned long long)MAXDWORD + 1)) + (unsigned long long)userTime.dwLowDateTime;
				entry.kernel_time = entry.kernel_time_raw / 10000000;
				entry.user_time = entry.user_time_raw / 10000000;
				entry.creation_time = str::format::filetime_to_time((creationTime.dwHighDateTime * ((unsigned long long)MAXDWORD + 1)) + (unsigned long long)creationTime.dwLowDateTime);
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
		if (EnumProcessModules(handle, &hMod, sizeof(hMod), &size)) {
			TCHAR buffer[MAX_FILENAME + 1];
			if (!GetModuleFileNameEx(handle, hMod, reinterpret_cast<LPTSTR>(&buffer), MAX_FILENAME)) {
				CloseHandle(handle);
				throw nsclient::nsclient_exception("Failed to find name for: " + str::xtos(pid) + ": " + error::lookup::last_error());
			} else {
				std::wstring path = buffer;
				entry.filename = utf8::cvt<std::string>(path);
				std::wstring::size_type pos = path.find_last_of(_T("\\"));
				if (pos != std::wstring::npos) {
					path = path.substr(++pos);
				}
				entry.exe = utf8::cvt<std::string>(path);
			}
		}
		return entry;
	}

	process_list enumerate_processes(bool ignore_unreadable, bool find_16bit, bool deep_scan, error_reporter *error_interface, unsigned int buffer_size) {
		try {
			enable_token_privilege(SE_DEBUG_NAME, true);
		} catch (const nsclient::nsclient_exception &e) {
			if (error_interface != NULL)
				error_interface->report_warning(e.reason());
		}

		std::list<process_info> ret;
		DWORD *dwPIDs = new DWORD[buffer_size + 1];
		DWORD cbNeeded = 0;
		BOOL OK = EnumProcesses(dwPIDs, buffer_size*sizeof(DWORD), &cbNeeded);
		if (cbNeeded >= DEFAULT_BUFFER_SIZE*sizeof(DWORD)) {
			delete[] dwPIDs;
			if (error_interface != NULL)
				error_interface->report_debug("Need larger buffer: " + str::xtos(buffer_size));
			return enumerate_processes(ignore_unreadable, find_16bit, deep_scan, error_interface, buffer_size * 10);
		}
		if (!OK) {
			delete[] dwPIDs;
			throw nsclient::nsclient_exception("Failed to enumerate process: " + error::lookup::last_error());
		}
		unsigned int process_count = cbNeeded / sizeof(DWORD);
		for (unsigned int i = 0; i < process_count; ++i) {
			if (dwPIDs[i] == 0)
				continue;
			process_info entry;
			entry.hung = false;
			try {
				try {
					entry = describe_pid(dwPIDs[i], deep_scan, ignore_unreadable);
				} catch (const nsclient::nsclient_exception &e) {
					if (deep_scan) {
						try {
							entry = describe_pid(dwPIDs[i], false, ignore_unreadable);
						} catch (const nsclient::nsclient_exception &e) {
							if (error_interface != NULL)
								error_interface->report_debug(e.reason());
						}
					} else {
						if (error_interface != NULL)
							error_interface->report_debug(e.reason());
					}
				}
				if (find_16bit) {
					if (stricmp(entry.filename.get().substr(0, 9).c_str(), "NTVDM.EXE") == 0) {
						find_16bit_container container;
						container.target = &ret;
						container.pid = entry.pid;
						windows::winapi::VDMEnumTaskWOWEx(container.pid, (windows::winapi::tTASKENUMPROCEX)&Enum16Proc, (LPARAM)&container);
					}
				}
				if (ignore_unreadable && entry.unreadable)
					continue;
				ret.push_back(entry);
			} catch (const nsclient::nsclient_exception &e) {
				if (error_interface != NULL)
					error_interface->report_error("Exception describing PID: " + str::xtos(dwPIDs[i]) + ": " + e.reason());
			} catch (...) {
				if (error_interface != NULL)
					error_interface->report_error("Unknown exception describing PID: " + str::xtos(dwPIDs[i]));
			}
		}

		std::vector<DWORD> hung_pids = find_crashed_pids(error_interface);
		for (process_list::iterator entry = ret.begin(); entry != ret.end(); ++entry) {
			if (std::find(hung_pids.begin(), hung_pids.end(), entry->pid) != hung_pids.end())
				(*entry).hung = true;
			else
				(*entry).hung = false;
		}

		delete[] dwPIDs;

		try {
			enable_token_privilege(SE_DEBUG_NAME, false);
		} catch (const nsclient::nsclient_exception &e) {
			if (error_interface != NULL)
				error_interface->report_warning(e.reason());
		}

		return ret;
	}

	typedef std::map<DWORD, process_info> process_map;
	process_map get_process_data(bool ignore_unreadable, error_reporter *error_interface, unsigned int buffer_size = DEFAULT_BUFFER_SIZE) {
		process_map ret;
		DWORD *dwPIDs = new DWORD[buffer_size + 1];
		DWORD cbNeeded = 0;
		BOOL OK = EnumProcesses(dwPIDs, buffer_size*sizeof(DWORD), &cbNeeded);
		if (cbNeeded >= DEFAULT_BUFFER_SIZE*sizeof(DWORD)) {
			delete[] dwPIDs;
			if (error_interface != NULL)
				error_interface->report_debug("Need larger buffer: " + str::xtos(buffer_size));
			return get_process_data(ignore_unreadable, error_interface, buffer_size * 10);
		}
		if (!OK) {
			delete[] dwPIDs;
			throw nsclient::nsclient_exception("Failed to enumerate process: " + error::lookup::last_error());
		}
		unsigned int process_count = cbNeeded / sizeof(DWORD);
		for (unsigned int i = 0; i < process_count; ++i) {
			if (dwPIDs[i] == 0)
				continue;
			process_info entry;
			entry.hung = false;
			try {
				try {
					entry = describe_pid(dwPIDs[i], true, ignore_unreadable);
				} catch (const nsclient::nsclient_exception &e) {
					if (!ignore_unreadable && error_interface != NULL)
						error_interface->report_debug(e.reason());
					continue;
				}
				ret[dwPIDs[i]] = entry;
			} catch (const nsclient::nsclient_exception &e) {
				if (error_interface != NULL)
					error_interface->report_error("Exception describing PID: " + str::xtos(dwPIDs[i]) + ": " + e.reason());
			} catch (...) {
				if (error_interface != NULL)
					error_interface->report_error("Unknown exception describing PID: " + str::xtos(dwPIDs[i]));
			}
		}
		delete[] dwPIDs;
		return ret;
	}

	process_list enumerate_processes_delta(bool ignore_unreadable, error_reporter *error_interface) {
		process_list ret;
		try {
			enable_token_privilege(SE_DEBUG_NAME, true);
		} catch (const nsclient::nsclient_exception &e) {
			if (error_interface != NULL)
				error_interface->report_error(e.reason());
		}

		unsigned long long kernel_time = 0;
		unsigned long long user_time = 0;
		unsigned long long idle_time = 0;
		FILETIME idleTime;
		FILETIME kernelTime;
		FILETIME userTime;
		if (GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
			kernel_time = (kernelTime.dwHighDateTime * ((unsigned long long)MAXDWORD + 1)) + (unsigned long long)kernelTime.dwLowDateTime;
			user_time = (userTime.dwHighDateTime * ((unsigned long long)MAXDWORD + 1)) + (unsigned long long)userTime.dwLowDateTime;
			idle_time = (idleTime.dwHighDateTime * ((unsigned long long)MAXDWORD + 1)) + (unsigned long long)idleTime.dwLowDateTime;
		}

		process_map p1 = get_process_data(ignore_unreadable, error_interface);
		Sleep(1000);
		if (GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
			kernel_time = (kernelTime.dwHighDateTime * ((unsigned long long)MAXDWORD + 1)) + (unsigned long long)kernelTime.dwLowDateTime - kernel_time;
			user_time = (userTime.dwHighDateTime * ((unsigned long long)MAXDWORD + 1)) + (unsigned long long)userTime.dwLowDateTime - user_time;
			idle_time = (idleTime.dwHighDateTime * ((unsigned long long)MAXDWORD + 1)) + (unsigned long long)idleTime.dwLowDateTime - idle_time;
		}
		long long total_time = kernel_time + user_time + idle_time;

		process_map p2 = get_process_data(ignore_unreadable, error_interface);
		BOOST_FOREACH(process_map::value_type v1, p1) {
			process_map::iterator v2 = p2.find(v1.first);
			if (v2 == p2.end()) {
				if (error_interface != NULL)
					error_interface->report_debug("process died: " + str::xtos(v1.first));
				continue;
			}
			v2->second -= v1.second;
			v2->second.make_cpu_delta(kernel_time, user_time, total_time);
			ret.push_back(v2->second);
		}

		try {
			enable_token_privilege(SE_DEBUG_NAME, false);
		} catch (const nsclient::nsclient_exception &e) {
			if (error_interface != NULL)
				error_interface->report_error(e.reason());
		}
		return ret;
	}

	boost::shared_ptr<process_helper::process_info> process_info::get_total() {
		return boost::shared_ptr<process_helper::process_info>(new process_info("total"));
	}

}