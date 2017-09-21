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

#pragma once

#include <vector>

#include <boost/foreach.hpp>
#include <buffer.hpp>
#include <win_sysinfo/win_defines.hpp>

#define WINDOWS_ANCIENT 0
#define WINDOWS_XP 51
#define WINDOWS_SERVER_2003 52
#define WINDOWS_VISTA 60
#define WINDOWS_7 61
#define WINDOWS_8 62
#define WINDOWS_81 63
#define WINDOWS_10 100
#define WINDOWS_NEW MAXLONG

#define WINDOWS_HAS_CONSOLE_HOST (WindowsVersion >= WINDOWS_7)
#define WINDOWS_HAS_CYCLE_TIME (WindowsVersion >= WINDOWS_VISTA)
#define WINDOWS_HAS_IFILEDIALOG (WindowsVersion >= WINDOWS_VISTA)
#define WINDOWS_HAS_IMAGE_FILE_NAME_BY_PROCESS_ID (WindowsVersion >= WINDOWS_VISTA)
#define WINDOWS_HAS_IMMERSIVE (WindowsVersion >= WINDOWS_8)
#define WINDOWS_HAS_LIMITED_ACCESS (WindowsVersion >= WINDOWS_VISTA)
#define WINDOWS_HAS_PSSUSPENDRESUMEPROCESS (WindowsVersion >= WINDOWS_VISTA)
#define WINDOWS_HAS_SERVICE_TAGS (WindowsVersion >= WINDOWS_VISTA)
#define WINDOWS_HAS_UAC (WindowsVersion >= WINDOWS_VISTA)

namespace windows {
	struct system_info {
		struct load_entry {
			double idle;
			double total;
			double kernel;
			int core;
			load_entry() : idle(0.0), total(0.0), kernel(0.0), core(-1) {}
			void add(const load_entry &other) {
				idle += other.idle;
				total += other.total;
				kernel += other.kernel;
			}
			void normalize(double value) {
				idle /= value;
				total /= value;
				kernel /= value;
			}
		};

		struct cpu_load {
			unsigned long cores;
			std::vector<load_entry> core;
			load_entry total;
			cpu_load() : cores(0) {}
			void add(const cpu_load &n) {
				total.add(n.total);
				cores = max(cores, n.cores);
				core.resize(cores);
				for (unsigned long i = 0; i < n.cores; ++i) {
					core[i].add(n.core[i]);
				}
			}
			void normalize(double value) {
				total.normalize(value);
				BOOST_FOREACH(load_entry &c, core) {
					c.normalize(value);
				}
			}
		};

		struct memory_entry {
			unsigned long long total;
			unsigned long long avail;
		};
		struct memory_usage {
			memory_entry physical;
			memory_entry pagefile;
			memory_entry virtual_memory;
		};

		struct pagefile_info {
			long long size;
			long long usage;
			long long peak_usage;
			std::string name;
			pagefile_info(const std::string name = "") : size(0), usage(0), peak_usage(0), name(name) {}
			void add(const pagefile_info &other) {
				size += other.size;
				usage += other.usage;
				peak_usage += other.peak_usage;
			}
		};

		static std::vector<pagefile_info> get_pagefile_info();

		static std::string get_version_string();
		static unsigned long get_version();
		static OSVERSIONINFOEX* get_versioninfo();
		static long get_numberOfProcessorscores();
		static std::vector<std::string> get_suite_list();
		static long long get_suite_i();

		static cpu_load get_cpu_load();
		static memory_usage get_memory();
		static hlp::buffer<BYTE, windows::winapi::SYSTEM_PROCESS_INFORMATION*> get_system_process_information(int size = 0x32000);
	};

	namespace winapi {
		typedef BOOL(*tTASKENUMPROCEX)(DWORD dwThreadId, WORD hMod16, WORD hTask16, PSZ pszModName, PSZ pszFileName, LPARAM lpUserDefined);

		BOOL WTSQueryUserToken(ULONG   SessionId, PHANDLE phToken);
		DWORD WTSGetActiveConsoleSessionId();

		BOOL EnumServicesStatusEx(SC_HANDLE hSCManager, SC_ENUM_TYPE InfoLevel, DWORD dwServiceType, DWORD dwServiceState, LPBYTE lpServices, DWORD cbBufSize, LPDWORD pcbBytesNeeded, LPDWORD lpServicesReturned, LPDWORD lpResumeHandle, LPCTSTR pszGroupName);
		BOOL QueryServiceConfig2(SC_HANDLE hService, DWORD dwInfoLevel, LPBYTE lpBuffer, DWORD cbBufSize, LPDWORD pcbBytesNeeded);
		BOOL QueryServiceStatusEx(SC_HANDLE hService, SC_STATUS_TYPE InfoLevel, LPBYTE lpBuffer, DWORD cbBufSize, LPDWORD pcbBytesNeeded);
		bool IsWow64(HANDLE hProcess, bool def = false);
		DWORD GetProcessImageFileName(HANDLE hProcess, LPWSTR lpImageFileName, DWORD nSize);
		LONG NtQueryInformationProcess(HANDLE ProcessHandle, DWORD ProcessInformationClass, PVOID ProcessInformation, DWORD ProcessInformationLength, PDWORD ReturnLength);

		INT VDMEnumTaskWOWEx(DWORD dwProcessId, tTASKENUMPROCEX fp, LPARAM lparam);
	}
};