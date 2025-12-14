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

#include "EnumProcess.h"

#include <tchar.h>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/make_shared.hpp>
#include <buffer.hpp>
#include <error/error.hpp>
#include <handle.hpp>
#include <map>
#include <nsclient/nsclient_exception.hpp>
#include <str/format.hpp>
#include <str/xtos.hpp>
#include <string>
#include <utf8.hpp>
#include <win/psapi.hpp>
#include <win/windows.hpp>
#include <win_sysinfo/win_defines.hpp>
#include <win_sysinfo/win_sysinfo.hpp>

constexpr int MAX_FILENAME = 256;

struct generic_closer {
  static void close(HANDLE handle) { CloseHandle(handle); }
};
typedef hlp::handle<HANDLE, generic_closer> generic_handle;

namespace process_helper {
void enable_token_privilege(LPTSTR privilege, bool enable) {
  generic_handle token;
  TOKEN_PRIVILEGES token_privileges;
  LUID luid;
  if (!LookupPrivilegeValue(nullptr, privilege, &luid)) throw nsclient::nsclient_exception("Failed to lookup privilege: " + error::lookup::last_error());
  ZeroMemory(&token_privileges, sizeof(TOKEN_PRIVILEGES));
  token_privileges.PrivilegeCount = 1;
  token_privileges.Privileges[0].Luid = luid;
  if (enable)
    token_privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
  else
    token_privileges.Privileges[0].Attributes = 0;
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, token.ref()))
    throw nsclient::nsclient_exception("Failed to open process token: " + error::lookup::last_error());
  if (!AdjustTokenPrivileges(token.get(), FALSE, &token_privileges, sizeof(TOKEN_PRIVILEGES), (PTOKEN_PRIVILEGES) nullptr, (PDWORD) nullptr))
    throw nsclient::nsclient_exception("Failed to adjust token privilege: " + error::lookup::last_error());
}

struct find_16bit_container {
  std::list<process_info> *target;
  DWORD pid;
};
BOOL CALLBACK Enum16Proc(DWORD, WORD, WORD, PSZ, const PSZ pszFileName, LPARAM lpUserDefined) {
  auto *container = reinterpret_cast<find_16bit_container *>(lpUserDefined);
  process_info pEntry;
  pEntry.pid = container->pid;
  const std::string tmp = pszFileName;
  pEntry.command_line = tmp;
  std::string::size_type pos = tmp.find_last_of('\\');
  if (pos != std::string::npos)
    pEntry.filename = tmp.substr(++pos);
  else
    pEntry.filename = tmp;
  container->target->push_back(pEntry);
  return FALSE;
}

struct enum_data {
  error_reporter *error_interface{};
  std::vector<DWORD> crashed_pids;
};

BOOL CALLBACK EnumWindowsProc(const HWND hwnd, const LPARAM lParam) {
  const auto data = reinterpret_cast<enum_data *>(lParam);
  if (!IsWindowVisible(hwnd)) return TRUE;
  if (GetWindow(hwnd, GW_OWNER) != nullptr) return TRUE;
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
    if (error_interface) error_interface->report_error("Failed to enumerate windows: " + utf8::cvt<std::string>(error::lookup::last_error()));
  }
  return data.crashed_pids;
}

void nscpGetCommandLine(const HANDLE hProcess, const LPVOID pebAddress, process_info &entry) {
  windows::winapi::UNICODE_STRING commandLine;
  LPVOID rtlUserProcParamsAddress;
#ifdef _WIN64
  if (!ReadProcessMemory(hProcess, static_cast<PCHAR>(pebAddress) + 0x20, &rtlUserProcParamsAddress, sizeof(LPVOID), nullptr))
    entry.set_error("Could not read the address of ProcessParameters: " + error::lookup::last_error());
  if (!ReadProcessMemory(hProcess, static_cast<PCHAR>(rtlUserProcParamsAddress) + 0x70, &commandLine, sizeof(commandLine), nullptr))
    entry.set_error("Could not read command line: " + error::lookup::last_error());
#else
  if (!entry.wow64) return;
  if (!ReadProcessMemory(hProcess, (PCHAR)pebAddress + 0x10, &rtlUserProcParamsAddress, sizeof(LPVOID), NULL))
    entry.set_error("Could not read the address of ProcessParameters: " + error::lookup::last_error());
  if (!ReadProcessMemory(hProcess, (PCHAR)rtlUserProcParamsAddress + 0x40, &commandLine, sizeof(commandLine), NULL))
    entry.set_error("Could not read command line: " + error::lookup::last_error());
#endif

  /* allocate memory to hold the command line */
  auto *commandLineContents = new wchar_t[commandLine.Length + 2];
  memset(commandLineContents, 0, commandLine.Length);

  /* read the command line */
  if (!ReadProcessMemory(hProcess, commandLine.Buffer, commandLineContents, commandLine.Length, nullptr)) {
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
  if (deep_scan) openArgs |= PROCESS_VM_OPERATION;

  unsigned long ReturnLength = 0;

  generic_handle handle(OpenProcess(openArgs, FALSE, pid));
  if (!handle) {
    handle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!handle) {
      DWORD err = GetLastError();
      entry.unreadable = true;
      if (!ignore_unreadable || err != ERROR_ACCESS_DENIED) entry.set_error("Failed to open process " + str::xtos(pid) + ": " + error::lookup::last_error());
      return entry;
    }
  }

  hlp::buffer<wchar_t> buffer(MAX_PATH);
  DWORD len = GetProcessImageFileName(handle, buffer, static_cast<DWORD>(buffer.size()));
  if (len > 0) {
    buffer[len] = 0;
    auto tmp = utf8::cvt<std::string>(std::wstring(buffer.get()));
    entry.filename = tmp;
    std::size_t pos = tmp.find_last_of('\\');
    if (pos != std::string::npos)
      entry.exe = tmp.substr(pos + 1);
    else
      entry.exe = tmp;
  }

  LPVOID PebBaseAddress = nullptr;
  windows::winapi::PROCESS_BASIC_INFORMATION pbi = {};
  if (windows::winapi::NtQueryInformationProcess(handle, windows::winapi::ProcessBasicInformation, &pbi, sizeof(windows::winapi::PROCESS_BASIC_INFORMATION),
                                                 &ReturnLength) >= 0) {
    PebBaseAddress = pbi.PebBaseAddress;
    // entry.handleCount = handleCount;
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
      entry.kernel_time_raw =
          (kernelTime.dwHighDateTime * (static_cast<unsigned long long>(MAXDWORD) + 1)) + static_cast<unsigned long long>(kernelTime.dwLowDateTime);
      entry.user_time_raw =
          (userTime.dwHighDateTime * (static_cast<unsigned long long>(MAXDWORD) + 1)) + static_cast<unsigned long long>(userTime.dwLowDateTime);
      entry.kernel_time = entry.kernel_time_raw / 10000000;
      entry.user_time = entry.user_time_raw / 10000000;
      entry.creation_time = str::format::filetime_to_time(creationTime.dwHighDateTime * (static_cast<unsigned long long>(MAXDWORD) + 1) +
                                                          static_cast<unsigned long long>(creationTime.dwLowDateTime));
    }

    SIZE_T minimumWorkingSetSize;
    SIZE_T maximumWorkingSetSize;
    if (GetProcessWorkingSetSize(handle, &minimumWorkingSetSize, &maximumWorkingSetSize)) {
    }

    IO_COUNTERS ioc = {};
    if (windows::winapi::NtQueryInformationProcess(handle, windows::winapi::ProcessIoCounters, &ioc, sizeof(IO_COUNTERS), &ReturnLength) >= 0) {
      entry.otherOperationCount = ioc.OtherOperationCount;
      entry.otherTransferCount = ioc.OtherTransferCount;
      entry.readOperationCount = ioc.ReadOperationCount;
      entry.readTransferCount = ioc.ReadTransferCount;
      entry.writeOperationCount = ioc.WriteOperationCount;
      entry.writeTransferCount = ioc.WriteTransferCount;
    }
    windows::winapi::VM_COUNTERS vmc = {};
    if (windows::winapi::NtQueryInformationProcess(handle, windows::winapi::ProcessVmCounters, &vmc, sizeof(windows::winapi::VM_COUNTERS), &ReturnLength) >=
        0) {
      entry.PeakVirtualSize = vmc.PeakVirtualSize;
      entry.VirtualSize = vmc.VirtualSize;
      entry.PageFaultCount = vmc.PageFaultCount;
      entry.PeakWorkingSetSize = vmc.PeakWorkingSetSize;
      entry.WorkingSetSize = vmc.WorkingSetSize;
      entry.QuotaPeakPagedPoolUsage = vmc.QuotaPeakPagedPoolUsage;
      entry.QuotaPagedPoolUsage = vmc.QuotaPagedPoolUsage;
      entry.QuotaPeakNonPagedPoolUsage = vmc.QuotaPeakNonPagedPoolUsage;
      entry.QuotaNonPagedPoolUsage = vmc.QuotaNonPagedPoolUsage;
      entry.PageFileUsage = vmc.PagefileUsage;
      entry.PeakPageFileUsage = vmc.PeakPagefileUsage;
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

    if (PebBaseAddress) nscpGetCommandLine(handle, PebBaseAddress, entry);
  }

  HMODULE hMod;
  DWORD size;
  // Get the first module (the process itself)
  if (EnumProcessModules(handle, &hMod, sizeof(hMod), &size)) {
    TCHAR buffer2[MAX_FILENAME + 1];
    if (!GetModuleFileNameEx(handle, hMod, reinterpret_cast<LPTSTR>(&buffer2), MAX_FILENAME)) {
      CloseHandle(handle);
      throw nsclient::nsclient_exception("Failed to find name for: " + str::xtos(pid) + ": " + error::lookup::last_error());
    } else {
      std::wstring path = buffer2;
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
    if (error_interface != nullptr) error_interface->report_warning(e.reason());
  }

  std::list<process_info> ret;
  auto *dwPIDs = new DWORD[buffer_size + 1];
  DWORD cbNeeded = 0;
  const BOOL OK = EnumProcesses(dwPIDs, buffer_size * sizeof(DWORD), &cbNeeded);
  if (cbNeeded >= DEFAULT_BUFFER_SIZE * sizeof(DWORD)) {
    delete[] dwPIDs;
    if (error_interface != nullptr) error_interface->report_debug("Need larger buffer: " + str::xtos(buffer_size));
    return enumerate_processes(ignore_unreadable, find_16bit, deep_scan, error_interface, buffer_size * 10);
  }
  if (!OK) {
    delete[] dwPIDs;
    throw nsclient::nsclient_exception("Failed to enumerate process: " + error::lookup::last_error());
  }
  unsigned int process_count = cbNeeded / sizeof(DWORD);
  for (unsigned int i = 0; i < process_count; ++i) {
    if (dwPIDs[i] == 0) continue;
    process_info entry;
    entry.hung = false;
    try {
      try {
        entry = describe_pid(dwPIDs[i], deep_scan, ignore_unreadable);
      } catch (const nsclient::nsclient_exception &e) {
        if (deep_scan) {
          try {
            entry = describe_pid(dwPIDs[i], false, ignore_unreadable);
          } catch (const nsclient::nsclient_exception &e2) {
            if (error_interface != nullptr) error_interface->report_debug(e2.reason());
          }
        } else {
          if (error_interface != nullptr) error_interface->report_debug(e.reason());
        }
      }
      if (find_16bit) {
        auto filename = entry.filename.get();
        if (filename.length() >= 9 && boost::algorithm::iequals(filename.substr(0, 9), "NTVDM.EXE")) {
          find_16bit_container container{};
          container.target = &ret;
          container.pid = entry.pid.get();
          windows::winapi::VDMEnumTaskWOWEx(container.pid, (windows::winapi::tTASKENUMPROCEX)&Enum16Proc, reinterpret_cast<LPARAM>(&container));
        }
      }
      if (ignore_unreadable && entry.unreadable) continue;
      ret.push_back(entry);
    } catch (const nsclient::nsclient_exception &e) {
      if (error_interface != nullptr) error_interface->report_error("Exception describing PID: " + str::xtos(dwPIDs[i]) + ": " + e.reason());
    } catch (...) {
      if (error_interface != nullptr) error_interface->report_error("Unknown exception describing PID: " + str::xtos(dwPIDs[i]));
    }
  }

  std::vector<DWORD> hung_pids = find_crashed_pids(error_interface);
  for (auto entry = ret.begin(); entry != ret.end(); ++entry) {
    if (std::find(hung_pids.begin(), hung_pids.end(), entry->pid) != hung_pids.end())
      entry->hung = true;
    else
      entry->hung = false;
  }

  delete[] dwPIDs;

  try {
    enable_token_privilege(SE_DEBUG_NAME, false);
  } catch (const nsclient::nsclient_exception &e) {
    if (error_interface != nullptr) error_interface->report_warning(e.reason());
  }

  return ret;
}

typedef std::map<DWORD, process_info> process_map;
process_map get_process_data(bool ignore_unreadable, error_reporter *error_interface, const unsigned int buffer_size = DEFAULT_BUFFER_SIZE) {
  process_map ret;
  const auto dwPIDs = new DWORD[buffer_size + 1];
  DWORD cbNeeded = 0;
  const BOOL OK = EnumProcesses(dwPIDs, buffer_size * sizeof(DWORD), &cbNeeded);
  if (cbNeeded >= DEFAULT_BUFFER_SIZE * sizeof(DWORD)) {
    delete[] dwPIDs;
    if (error_interface != nullptr) error_interface->report_debug("Need larger buffer: " + str::xtos(buffer_size));
    return get_process_data(ignore_unreadable, error_interface, buffer_size * 10);
  }
  if (!OK) {
    delete[] dwPIDs;
    throw nsclient::nsclient_exception("Failed to enumerate process: " + error::lookup::last_error());
  }
  unsigned int process_count = cbNeeded / sizeof(DWORD);
  for (unsigned int i = 0; i < process_count; ++i) {
    if (dwPIDs[i] == 0) continue;
    process_info entry;
    entry.hung = false;
    try {
      try {
        entry = describe_pid(dwPIDs[i], true, ignore_unreadable);
      } catch (const nsclient::nsclient_exception &e) {
        if (!ignore_unreadable && error_interface != nullptr) error_interface->report_debug(e.reason());
        continue;
      }
      ret[dwPIDs[i]] = entry;
    } catch (const nsclient::nsclient_exception &e) {
      if (error_interface != nullptr) error_interface->report_error("Exception describing PID: " + str::xtos(dwPIDs[i]) + ": " + e.reason());
    } catch (...) {
      if (error_interface != nullptr) error_interface->report_error("Unknown exception describing PID: " + str::xtos(dwPIDs[i]));
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
    if (error_interface != nullptr) error_interface->report_error(e.reason());
  }

  unsigned long long kernel_time = 0;
  unsigned long long user_time = 0;
  unsigned long long idle_time = 0;
  FILETIME idleTime;
  FILETIME kernelTime;
  FILETIME userTime;
  if (GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
    kernel_time = (kernelTime.dwHighDateTime * (static_cast<unsigned long long>(MAXDWORD) + 1)) + static_cast<unsigned long long>(kernelTime.dwLowDateTime);
    user_time = (userTime.dwHighDateTime * (static_cast<unsigned long long>(MAXDWORD) + 1)) + static_cast<unsigned long long>(userTime.dwLowDateTime);
    idle_time = (idleTime.dwHighDateTime * (static_cast<unsigned long long>(MAXDWORD) + 1)) + static_cast<unsigned long long>(idleTime.dwLowDateTime);
  }

  const process_map p1 = get_process_data(ignore_unreadable, error_interface);
  Sleep(1000);
  if (GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
    kernel_time =
        (kernelTime.dwHighDateTime * (static_cast<unsigned long long>(MAXDWORD) + 1)) + static_cast<unsigned long long>(kernelTime.dwLowDateTime) - kernel_time;
    user_time =
        (userTime.dwHighDateTime * (static_cast<unsigned long long>(MAXDWORD) + 1)) + static_cast<unsigned long long>(userTime.dwLowDateTime) - user_time;
    idle_time =
        (idleTime.dwHighDateTime * (static_cast<unsigned long long>(MAXDWORD) + 1)) + static_cast<unsigned long long>(idleTime.dwLowDateTime) - idle_time;
  }
  const unsigned long long total_time = kernel_time + user_time + idle_time;

  process_map p2 = get_process_data(ignore_unreadable, error_interface);
  for (const auto &v1 : p1) {
    auto v2 = p2.find(v1.first);
    if (v2 == p2.end()) {
      if (error_interface != nullptr) error_interface->report_debug("process died: " + str::xtos(v1.first));
      continue;
    }
    v2->second -= v1.second;
    v2->second.make_cpu_delta(kernel_time, user_time, total_time);
    ret.push_back(v2->second);
  }

  try {
    enable_token_privilege(SE_DEBUG_NAME, false);
  } catch (const nsclient::nsclient_exception &e) {
    if (error_interface != nullptr) error_interface->report_error(e.reason());
  }
  return ret;
}

boost::shared_ptr<process_info> process_info::get_total() { return boost::make_shared<process_info>("total"); }

}  // namespace process_helper