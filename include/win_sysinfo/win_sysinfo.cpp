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

#include <windows.h>

#include <win_sysinfo/win_defines.hpp>
#include <win_sysinfo/win_sysinfo.hpp>

#include <boost/scoped_array.hpp>
#include <error/error.hpp>
#include <nsclient/nsclient_exception.hpp>
#include <buffer.hpp>
#include <utf8.hpp>

namespace windows {
#define STATUS_SUCCESS                          ((NTSTATUS)0x00000000L)
#define STATUS_INFO_LENGTH_MISMATCH             ((NTSTATUS)0xC0000004L)
#define STATUS_BUFFER_OVERFLOW                  ((NTSTATUS)0x80000005L)
#define STATUS_ACCESS_VIOLATION                 ((NTSTATUS)0xC0000005L)

	//////////////////////////////////////////////////////////////////////////
	namespace winapi {
		typedef BOOL(WINAPI *tEnumServicesStatusEx)(SC_HANDLE hSCManager, SC_ENUM_TYPE InfoLevel, DWORD dwServiceType, DWORD dwServiceState, LPBYTE lpServices, DWORD cbBufSize, LPDWORD pcbBytesNeeded, LPDWORD lpServicesReturned, LPDWORD lpResumeHandle, LPCTSTR pszGroupName);
		typedef BOOL(WINAPI *tQueryServiceConfig2)(SC_HANDLE hService, DWORD dwInfoLevel, LPBYTE lpBuffer, DWORD cbBufSize, LPDWORD pcbBytesNeeded);
		typedef BOOL(WINAPI *tQueryServiceStatusEx)(SC_HANDLE hService, SC_STATUS_TYPE InfoLevel, LPBYTE lpBuffer, DWORD cbBufSize, LPDWORD pcbBytesNeeded);
		typedef INT(WINAPI *tVDMEnumTaskWOWEx)(DWORD dwProcessId, tTASKENUMPROCEX fp, LPARAM lparam);
		typedef LONG(NTAPI *tNtQueryInformationProcess)(HANDLE ProcessHandle, DWORD ProcessInformationClass, PVOID ProcessInformation, DWORD ProcessInformationLength, PDWORD ReturnLength);
		typedef BOOL(WINAPI *tIsWow64Process) (HANDLE ProcessHandle, PBOOL);
		typedef DWORD(WINAPI *tGetProcessImageFileName)(HANDLE hProcess, LPWSTR lpImageFileName, DWORD nSize);
		typedef LONG(NTAPI *tNtQuerySystemInformation)(SYSTEM_INFORMATION_CLASS SystemInformationClass, PVOID SystemInformation, ULONG SystemInformationLength, PULONG ReturnLength);
		typedef DWORD(WINAPI *tWTSGetActiveConsoleSessionId)();

		typedef BOOL(*tWTSQueryUserToken)(ULONG   SessionId,PHANDLE phToken);


		tEnumServicesStatusEx pEnumServicesStatusEx = NULL;
		tQueryServiceConfig2 pQueryServiceConfig2 = NULL;
		tQueryServiceStatusEx pQueryServiceStatusEx = NULL;
		tVDMEnumTaskWOWEx pVDMEnumTaskWOWEx = NULL;
		tNtQueryInformationProcess pNtQueryInformationProcess = NULL;
		tIsWow64Process pIsWow64Process = NULL;
		tGetProcessImageFileName pGetProcessImageFileName = NULL;
		tNtQuerySystemInformation pNtQuerySystemInformation = NULL;
		tWTSQueryUserToken pWTSQueryUserToken = NULL;
		tWTSGetActiveConsoleSessionId pWTSGetActiveConsoleSessionId = NULL;

		BOOL WTSQueryUserToken(ULONG   SessionId, PHANDLE phToken) {
			if (pWTSQueryUserToken == NULL) {
				HMODULE hMod = ::LoadLibrary(L"Wtsapi32.dll");
				if (hMod == NULL)
					throw nsclient::nsclient_exception("Failed to load: Wtsapi32: " + error::lookup::last_error());
				pWTSQueryUserToken = reinterpret_cast<tWTSQueryUserToken>(GetProcAddress(hMod, "WTSQueryUserToken"));
				if (pWTSQueryUserToken == NULL)
					throw nsclient::nsclient_exception("Failed to load: WTSQueryUserToken: " + error::lookup::last_error());
			}
			return pWTSQueryUserToken(SessionId, phToken);
		}

		DWORD WTSGetActiveConsoleSessionId() {
			if (pWTSGetActiveConsoleSessionId == NULL) {
				HMODULE hMod = ::LoadLibrary(L"Kernel32.dll");
				if (hMod == NULL)
					throw nsclient::nsclient_exception("Failed to load: Kernel32: " + error::lookup::last_error());
				pWTSGetActiveConsoleSessionId = reinterpret_cast<tWTSGetActiveConsoleSessionId>(GetProcAddress(hMod, "WTSGetActiveConsoleSessionId"));
				if (pWTSGetActiveConsoleSessionId == NULL)
					throw nsclient::nsclient_exception("Failed to load: WTSGetActiveConsoleSessionId: " + error::lookup::last_error());
			}
			return pWTSGetActiveConsoleSessionId();
		}


		BOOL EnumServicesStatusEx(SC_HANDLE hSCManager, SC_ENUM_TYPE InfoLevel, DWORD dwServiceType, DWORD dwServiceState, LPBYTE lpServices, DWORD cbBufSize, LPDWORD pcbBytesNeeded, LPDWORD lpServicesReturned, LPDWORD lpResumeHandle, LPCTSTR pszGroupName) {
			if (pEnumServicesStatusEx == NULL) {
				HMODULE hMod = ::LoadLibrary(L"Advapi32.dll");
				if (hMod == NULL)
					throw nsclient::nsclient_exception("Failed to load: Advapi32: " + error::lookup::last_error());
				pEnumServicesStatusEx = reinterpret_cast<tEnumServicesStatusEx>(GetProcAddress(hMod, "EnumServicesStatusExW"));
				if (pEnumServicesStatusEx == NULL)
					throw nsclient::nsclient_exception("Failed to load: EnumServicesStatusEx: " + error::lookup::last_error());
			}
			return pEnumServicesStatusEx(hSCManager, InfoLevel, dwServiceType, dwServiceState, lpServices, cbBufSize, pcbBytesNeeded, lpServicesReturned, lpResumeHandle, pszGroupName);
		}
		BOOL QueryServiceConfig2(SC_HANDLE hService, DWORD dwInfoLevel, LPBYTE lpBuffer, DWORD cbBufSize, LPDWORD pcbBytesNeeded) {
			if (pQueryServiceConfig2 == NULL) {
				HMODULE hMod = ::LoadLibrary(L"Advapi32.dll");
				if (hMod == NULL)
					throw nsclient::nsclient_exception("Failed to load: Advapi32: " + error::lookup::last_error());
				pQueryServiceConfig2 = reinterpret_cast<tQueryServiceConfig2>(GetProcAddress(hMod, "QueryServiceConfig2W"));
				if (pQueryServiceConfig2 == NULL)
					throw nsclient::nsclient_exception("Failed to load: QueryServiceConfig2: " + error::lookup::last_error());
			}
			return pQueryServiceConfig2(hService, dwInfoLevel, lpBuffer, cbBufSize, pcbBytesNeeded);
		}
		BOOL QueryServiceStatusEx(SC_HANDLE hService, SC_STATUS_TYPE InfoLevel, LPBYTE lpBuffer, DWORD cbBufSize, LPDWORD pcbBytesNeeded) {
			if (pQueryServiceStatusEx == NULL) {
				HMODULE hMod = ::LoadLibrary(L"Advapi32.dll");
				if (hMod == NULL)
					throw nsclient::nsclient_exception("Failed to load: Advapi32: " + error::lookup::last_error());
				pQueryServiceStatusEx = reinterpret_cast<tQueryServiceStatusEx>(GetProcAddress(hMod, "QueryServiceStatusEx"));
				if (pQueryServiceStatusEx == NULL)
					throw nsclient::nsclient_exception("Failed to load: QueryServiceStatusEx: " + error::lookup::last_error());
			}
			return pQueryServiceStatusEx(hService, InfoLevel, lpBuffer, cbBufSize, pcbBytesNeeded);
		}

		bool IsWow64(HANDLE hProcess, bool def) {
			if (pIsWow64Process == NULL)
				pIsWow64Process = reinterpret_cast<tIsWow64Process>(GetProcAddress(GetModuleHandle(L"kernel32"), "IsWow64Process"));
			if (pIsWow64Process == NULL)
				return def;
			BOOL bIsWow64 = FALSE;
			if (!pIsWow64Process(hProcess, &bIsWow64))
				return def;
			return bIsWow64 ? true : false;
		}

		DWORD GetProcessImageFileName(HANDLE hProcess, LPWSTR lpImageFileName, DWORD nSize) {
			if (pGetProcessImageFileName == NULL)
				pGetProcessImageFileName = reinterpret_cast<tGetProcessImageFileName>(GetProcAddress(GetModuleHandleA("PSAPI"), "GetProcessImageFileNameW"));
			if (pGetProcessImageFileName == NULL)
				throw nsclient::nsclient_exception("Failed to load GetProcessImageFileName: " + error::lookup::last_error());
			return pGetProcessImageFileName(hProcess, lpImageFileName, nSize);
		}
		LONG NtQueryInformationProcess(HANDLE ProcessHandle, DWORD ProcessInformationClass, PVOID ProcessInformation, DWORD ProcessInformationLength, PDWORD ReturnLength) {
			if (pNtQueryInformationProcess == NULL)
				pNtQueryInformationProcess = reinterpret_cast<tNtQueryInformationProcess>(GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryInformationProcess"));
			if (pNtQueryInformationProcess == NULL)
				throw nsclient::nsclient_exception("Failed to load NtQueryInformationProcess");
			return pNtQueryInformationProcess(ProcessHandle, ProcessInformationClass, ProcessInformation, ProcessInformationLength, ReturnLength);
		}
		INT VDMEnumTaskWOWEx(DWORD dwProcessId, tTASKENUMPROCEX fp, LPARAM lparam) {
			if (pVDMEnumTaskWOWEx == NULL)
				pVDMEnumTaskWOWEx = reinterpret_cast<tVDMEnumTaskWOWEx>(GetProcAddress(GetModuleHandleA("VDMDBG"), "VDMEnumTaskWOWEx"));
			if (pVDMEnumTaskWOWEx == NULL)
				throw nsclient::nsclient_exception("Failed to load NtQueryInformationProcess");
			return pVDMEnumTaskWOWEx(dwProcessId, fp, lparam);
		}

		LONG NtQuerySystemInformation(SYSTEM_INFORMATION_CLASS SystemInformationClass, PVOID SystemInformation, ULONG SystemInformationLength, PULONG ReturnLength) {
			if (pNtQuerySystemInformation == NULL)
				pNtQuerySystemInformation = reinterpret_cast<tNtQuerySystemInformation>(GetProcAddress(LoadLibrary(L"Ntdll"), "NtQuerySystemInformation"));
			if (pNtQuerySystemInformation == NULL)
				throw nsclient::nsclient_exception("Failed to load: NtQuerySystemInformation: " + error::lookup::last_error());
			return pNtQuerySystemInformation(SystemInformationClass, SystemInformation, SystemInformationLength, ReturnLength);
		}
	}

	winapi::SYSTEM_BASIC_INFORMATION g_systemBasicInformation;
	unsigned long g_windowsVersion;
	// ACCESS_MASK ProcessQueryAccess;
	// ACCESS_MASK ProcessAllAccess;
	// ACCESS_MASK ThreadQueryAccess;
	// ACCESS_MASK ThreadSetAccess;
	// ACCESS_MASK ThreadAllAccess;
	//RTL_OSVERSIONINFOEXW g_versionInfo;
	RTL_OSVERSIONINFOEXW g_versionInfo;
	void QuerySystemInformation() {
		winapi::NtQuerySystemInformation(winapi::SystemBasicInformation, &g_systemBasicInformation, sizeof(winapi::SYSTEM_BASIC_INFORMATION), NULL);
	}

	// RTL_OSVERSIONINFOEXW is defined in winnt.h
	bool GetOsVersion(RTL_OSVERSIONINFOEXW* pk_OsVer) {
		typedef LONG(WINAPI* tRtlGetVersion)(RTL_OSVERSIONINFOEXW*);

		memset(pk_OsVer, 0, sizeof(RTL_OSVERSIONINFOEXW));
		pk_OsVer->dwOSVersionInfoSize = sizeof(RTL_OSVERSIONINFOEXW);

		HMODULE h_NtDll = GetModuleHandleW(L"ntdll.dll");
		tRtlGetVersion f_RtlGetVersion = (tRtlGetVersion)GetProcAddress(h_NtDll, "RtlGetVersion");

		if (!f_RtlGetVersion)
			return FALSE; // This will never happen (all processes load ntdll.dll)

		LONG Status = f_RtlGetVersion(pk_OsVer);
		return Status == 0; // STATUS_SUCCESS;
	}

	void GetVersion() {
		ULONG majorVersion;
		ULONG minorVersion;


		if (GetOsVersion(&g_versionInfo)) {
			majorVersion = g_versionInfo.dwMajorVersion;
			minorVersion = g_versionInfo.dwMinorVersion;

			if (majorVersion == 5 && minorVersion < 1 || majorVersion < 5) {
				g_windowsVersion = WINDOWS_ANCIENT;
			}
			/* Windows XP */
			else if (majorVersion == 5 && minorVersion == 1) {
				g_windowsVersion = WINDOWS_XP;
			}
			/* Windows Server 2003 */
			else if (majorVersion == 5 && minorVersion == 2) {
				g_windowsVersion = WINDOWS_SERVER_2003;
			}
			/* Windows Vista, Windows Server 2008 */
			else if (majorVersion == 6 && minorVersion == 0) {
				g_windowsVersion = WINDOWS_VISTA;
			}
			/* Windows 7, Windows Server 2008 R2 */
			else if (majorVersion == 6 && minorVersion == 1) {
				g_windowsVersion = WINDOWS_7;
			}
			/* Windows 8 */
			else if (majorVersion == 6 && minorVersion == 2) {
				g_windowsVersion = WINDOWS_8;
			}
			/* Windows 8.1 */
			else if (majorVersion == 6 && minorVersion == 3) {
				g_windowsVersion = WINDOWS_81;
			}
			/* Windows 8.1 */
			else if (majorVersion == 10 && minorVersion == 0) {
				g_windowsVersion = WINDOWS_10;
			} else if (majorVersion == 10 && minorVersion > 0 || majorVersion > 10) {
				g_windowsVersion = WINDOWS_NEW;
			}
		}
	}

	bool g_hasVersion = false;
	bool g_hasBasicInfo = false;

	boost::scoped_array<unsigned long long> g_CPUIdleTimeOld;
	boost::scoped_array<unsigned long long> g_CPUTotalTimeOld;
	boost::scoped_array<unsigned long long> g_CPUKernelTimeOld;

	void init_old_buffer(boost::scoped_array<unsigned long long> &array, const std::size_t size) {
		if (!array) {
			array.reset(new unsigned long long[size]);
			for (std::size_t i = 0; i < size; i++) {
				array[i] = 0;
			}
		}
	}
	unsigned long system_info::get_version() {
		if (!g_hasVersion) {
			GetVersion();
			g_hasVersion = true;
		}
		return g_windowsVersion;
	}

	OSVERSIONINFOEX* system_info::get_versioninfo() {
		if (!g_hasVersion) {
			GetVersion();
			g_hasVersion = true;
		}
		return &g_versionInfo;
	}

	std::string system_info::get_version_string() {
		if (!g_hasVersion) {
			GetVersion();
			g_hasVersion = true;
		}
		ULONG majorVersion;
		ULONG minorVersion;
		BYTE type;
		majorVersion = g_versionInfo.dwMajorVersion;
		minorVersion = g_versionInfo.dwMinorVersion;
		type = g_versionInfo.wProductType;
		if (majorVersion == 5 && minorVersion < 1 || majorVersion < 5)
			return "Pre XP";
		else if (majorVersion == 5 && minorVersion == 1)
			return "Windows XP";
		else if (majorVersion == 5 && minorVersion == 2)
			return "Windows Server 2003";
		else if (majorVersion == 6 && minorVersion == 0 && type != VER_NT_WORKSTATION)
			return "Windows 2008";
		else if (majorVersion == 6 && minorVersion == 0)
			return "Windows Vista";
		else if (majorVersion == 6 && minorVersion == 1 && type != VER_NT_WORKSTATION)
			return "Windows 2008R2";
		else if (majorVersion == 6 && minorVersion == 1)
			return "Windows 7";
		else if (majorVersion == 6 && minorVersion == 2 && type != VER_NT_WORKSTATION)
			return "Windows 2012";
		else if (majorVersion == 6 && minorVersion == 2)
			return "Windows 8";
		else if (majorVersion == 6 && minorVersion == 3 && type != VER_NT_WORKSTATION)
			return "Windows Server 2012 R2";
		else if (majorVersion == 6 && minorVersion == 3)
			return "Windows 8.1";
		else if (majorVersion == 10 && minorVersion == 0 && type != VER_NT_WORKSTATION)
			return "Windows Server 2016";
		else if (majorVersion == 10 && minorVersion == 0)
			return "Windows 10";
		else if (type != VER_NT_WORKSTATION)
			return "Post Windows 2016";
		else
			return "Post Windows 10";
	}
	std::vector<std::string> system_info::get_suite_list() {
		if (!g_hasVersion) {
			GetVersion();
			g_hasVersion = true;
		}
		std::vector<std::string> ret;
		if ((g_versionInfo.wSuiteMask&VER_SUITE_BACKOFFICE) == VER_SUITE_BACKOFFICE)
			ret.push_back("Microsoft BackOffice");
		if ((g_versionInfo.wSuiteMask&VER_SUITE_BLADE) == VER_SUITE_BLADE)
			ret.push_back("Web Edition");
		if ((g_versionInfo.wSuiteMask&VER_SUITE_COMPUTE_SERVER) == VER_SUITE_COMPUTE_SERVER)
			ret.push_back("Compute Cluster Edition");
		if ((g_versionInfo.wSuiteMask&VER_SUITE_DATACENTER) == VER_SUITE_DATACENTER)
			ret.push_back("Datacenter Edition");
		if ((g_versionInfo.wSuiteMask&VER_SUITE_ENTERPRISE) == VER_SUITE_ENTERPRISE)
			ret.push_back("Enterprise Edition");
		if ((g_versionInfo.wSuiteMask&VER_SUITE_EMBEDDEDNT) == VER_SUITE_EMBEDDEDNT)
			ret.push_back("Embedded");
		if ((g_versionInfo.wSuiteMask&VER_SUITE_PERSONAL) == VER_SUITE_PERSONAL)
			ret.push_back("Home Edition");
		if ((g_versionInfo.wSuiteMask&VER_SUITE_SINGLEUSERTS) == VER_SUITE_SINGLEUSERTS)
			ret.push_back("Remote Desktop Support");
		if ((g_versionInfo.wSuiteMask&VER_SUITE_SMALLBUSINESS) == VER_SUITE_SMALLBUSINESS)
			ret.push_back("Small Business Server");
		if ((g_versionInfo.wSuiteMask&VER_SUITE_STORAGE_SERVER) == VER_SUITE_STORAGE_SERVER)
			ret.push_back("Storage Server");
		if ((g_versionInfo.wSuiteMask&VER_SUITE_TERMINAL) == VER_SUITE_TERMINAL)
			ret.push_back("Terminal Services");
		if ((g_versionInfo.wSuiteMask&VER_SUITE_WH_SERVER) == VER_SUITE_WH_SERVER)
			ret.push_back("Home Server");
		return ret;
	}

	long long system_info::get_suite_i() {
		if (!g_hasVersion) {
			GetVersion();
			g_hasVersion = true;
		}
		return g_versionInfo.wSuiteMask;
	}


	long system_info::get_numberOfProcessorscores() {
		if (!g_hasBasicInfo) {
			QuerySystemInformation();
			g_hasBasicInfo = true;
		}
		return g_systemBasicInformation.NumberOfProcessors;
	}

	hlp::buffer<BYTE, winapi::SYSTEM_PROCESS_INFORMATION*>  system_info::get_system_process_information(int size) {
		hlp::buffer<BYTE, winapi::SYSTEM_PROCESS_INFORMATION*> buffer(size);
		unsigned long bufferSize;
		LONG r = winapi::NtQuerySystemInformation(winapi::SystemProcessInformation, (BYTE*)buffer, buffer.size(), &bufferSize);
		if (r == 0)
			return buffer;
		if (r == STATUS_INFO_LENGTH_MISMATCH)
			return get_system_process_information(bufferSize * 10);
		if (r == STATUS_ACCESS_VIOLATION)
			throw nsclient::nsclient_exception("Access violation");
		throw nsclient::nsclient_exception("Failed to enumerate processes: unknown error");
	}

	system_info::cpu_load system_info::get_cpu_load() {
		int cores = get_numberOfProcessorscores();
		init_old_buffer(g_CPUIdleTimeOld, cores);
		init_old_buffer(g_CPUTotalTimeOld, cores);
		init_old_buffer(g_CPUKernelTimeOld, cores);

		boost::scoped_array<winapi::SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION> buffer(new winapi::SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION[cores]);
		if (winapi::NtQuerySystemInformation(winapi::SystemProcessorPerformanceInformation, &buffer[0], sizeof(winapi::SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION) * cores, NULL) != 0) {
			throw nsclient::nsclient_exception("Whoops");
		}

		cpu_load result;
		result.cores = cores;
		result.core.resize(cores);
		result.total.idle = result.total.kernel = result.total.total = 0.0;

		for (int i = 0; i < cores; i++) {
			unsigned long long CPUIdleTime = buffer[i].IdleTime.QuadPart;
			unsigned long long CPUKernelTime = buffer[i].KernelTime.QuadPart - buffer[i].IdleTime.QuadPart;
			unsigned long long CPUTotalTime = buffer[i].KernelTime.QuadPart + buffer[i].UserTime.QuadPart;

			unsigned long long CPUIdleTimeDiff = CPUIdleTime - g_CPUIdleTimeOld[i];
			unsigned long long CPUKernelTimeDiff = CPUKernelTime - g_CPUKernelTimeOld[i];
			unsigned long long CPUTotalTimeDiff = CPUTotalTime - g_CPUTotalTimeOld[i];

			if (CPUTotalTimeDiff != 0) {
				result.core[i].core = i;
				result.core[i].idle = static_cast<double>(((CPUIdleTimeDiff * 100) / CPUTotalTimeDiff));
				result.core[i].kernel = static_cast<double>(((CPUKernelTimeDiff * 100) / CPUTotalTimeDiff));
				result.core[i].total = 100.0 - result.core[i].idle;
				result.total.idle += result.core[i].idle;
				result.total.kernel += result.core[i].kernel;
				result.total.total += result.core[i].total;
			} else {
				result.core[i].idle = 0;
				result.core[i].kernel = 0;
				result.core[i].total = 0;
			}
			g_CPUTotalTimeOld[i] = CPUTotalTime;
			g_CPUIdleTimeOld[i] = CPUIdleTime;
			g_CPUKernelTimeOld[i] = CPUKernelTime;
		}
		result.total.idle /= result.cores;
		result.total.kernel /= result.cores;
		result.total.total /= result.cores;
		return result;
	}

	class CheckMemory {
	public:
		CheckMemory() : hKernel32(NULL), FEGlobalMemoryStatusEx(NULL), FEGlobalMemoryStatus(NULL) {
			hKernel32 = ::LoadLibrary(L"Kernel32");
			if (hKernel32) {
				FEGlobalMemoryStatusEx = (PFGlobalMemoryStatusEx)::GetProcAddress(hKernel32, "GlobalMemoryStatusEx");
				FEGlobalMemoryStatus = (PFGlobalMemoryStatus)::GetProcAddress(hKernel32, "GlobalMemoryStatus");
			}
		}
		virtual ~CheckMemory() {
			if (hKernel32) FreeLibrary(hKernel32);
		}

		system_info::memory_usage getMemoryStatus() {
			system_info::memory_usage ret;
			if (FEGlobalMemoryStatusEx != NULL) {
				MEMORYSTATUSEX buffer;
				buffer.dwLength = sizeof(buffer);
				if (!FEGlobalMemoryStatusEx(&buffer))
					throw nsclient::nsclient_exception("GlobalMemoryStatusEx failed: " + error::lookup::last_error());
				ret.physical.total = buffer.ullTotalPhys;
				ret.physical.avail = buffer.ullAvailPhys;
				ret.virtual_memory.total = buffer.ullTotalVirtual;
				ret.virtual_memory.avail = buffer.ullAvailVirtual;
				ret.pagefile.total = buffer.ullTotalPageFile;
				ret.pagefile.avail = buffer.ullAvailPageFile;
				return ret;
			} else if (FEGlobalMemoryStatus != NULL) {
				MEMORYSTATUS buffer;
				buffer.dwLength = sizeof(buffer);
				if (!FEGlobalMemoryStatus(&buffer))
					throw nsclient::nsclient_exception("GlobalMemoryStatus failed: " + error::lookup::last_error());
				ret.physical.total = buffer.dwTotalPhys;
				ret.physical.avail = buffer.dwAvailPhys;
				ret.virtual_memory.total = buffer.dwTotalVirtual;
				ret.virtual_memory.avail = buffer.dwAvailVirtual;
				ret.pagefile.total = buffer.dwTotalPageFile;
				ret.pagefile.avail = buffer.dwAvailPageFile;
				return ret;
			} else {
				throw nsclient::nsclient_exception("Failed to check memory: No method found");
			}
		}
	private:
		typedef BOOL(WINAPI *PFGlobalMemoryStatusEx)(LPMEMORYSTATUSEX lpBuffer);
		typedef BOOL(WINAPI *PFGlobalMemoryStatus)(LPMEMORYSTATUS lpBuffer);
		HMODULE hKernel32;
		PFGlobalMemoryStatusEx	FEGlobalMemoryStatusEx;
		PFGlobalMemoryStatus	FEGlobalMemoryStatus;
	};

	CheckMemory g_memory_checker;
	system_info::memory_usage system_info::get_memory() {
		return g_memory_checker.getMemoryStatus();
	}

	std::vector<system_info::pagefile_info> system_info::get_pagefile_info() {
		std::vector<system_info::pagefile_info> ret;

		SYSTEM_INFO si;
		GetSystemInfo(&si);

		hlp::buffer<BYTE, LPVOID> buffer(4096);
		DWORD retLen = 0;
		NTSTATUS status = windows::winapi::NtQuerySystemInformation(windows::winapi::SystemPageFileInformation, buffer, buffer.size(), &retLen);
		if (status == STATUS_INFO_LENGTH_MISMATCH) {
			buffer.resize(retLen + 10);
			status = windows::winapi::NtQuerySystemInformation(windows::winapi::SystemPageFileInformation, buffer, buffer.size(), &retLen);
		}
		if (status != STATUS_SUCCESS)
			throw nsclient::nsclient_exception("Failed to get pagefile info");
		if (retLen == 0) {
			return ret;
		}
		ULONG offset = 0;
		while (true) {
			windows::winapi::SYSTEM_PAGEFILE_INFORMATION *info = buffer.get_t<windows::winapi::SYSTEM_PAGEFILE_INFORMATION*>(offset);
			system_info::pagefile_info data(utf8::cvt<std::string>(std::wstring(info->PageFileName.Buffer)));
			data.peak_usage = static_cast<long long>(info->PeakUsage)*static_cast<long long>(si.dwPageSize);
			data.usage = static_cast<long long>(info->TotalInUse)*static_cast<long long>(si.dwPageSize);
			data.size = static_cast<long long>(info->TotalSize)*static_cast<long long>(si.dwPageSize);
			ret.push_back(data);
			if (info->NextEntryOffset == 0)
				break;
			offset += info->NextEntryOffset;
		}
		return ret;
	}
}