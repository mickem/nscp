// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The Windows half of the `system` and `user` nodes: the OS version and machine
// identity from the Win32 API, and logon sessions from Remote Desktop Services
// (WTS), which - unlike WMI - needs no service running and distinguishes a
// console session from an RDP one.

#include <Windows.h>
#include <WtsApi32.h>

#include <str/utf8.hpp>
#include <str/xtos.hpp>
#include <string>

#include "ncpa_sources.hpp"

namespace ncpa {
namespace sysinfo {

namespace {

std::string computer_name() {
  wchar_t buffer[MAX_COMPUTERNAME_LENGTH + 1] = {};
  DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
  if (GetComputerNameW(buffer, &size) == 0) return "";
  return utf8::cvt<std::string>(std::wstring(buffer, size));
}

// RtlGetVersion rather than GetVersionEx: since Windows 8.1 the latter lies to
// any process without a matching compatibility manifest, and reporting 6.2 on
// a Server 2022 box would make every fleet-wide OS report wrong.
RTL_OSVERSIONINFOW os_version() {
  RTL_OSVERSIONINFOW info = {};
  info.dwOSVersionInfoSize = sizeof(info);
  const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
  if (ntdll != nullptr) {
    typedef LONG(WINAPI * fRtlGetVersion)(PRTL_OSVERSIONINFOW);
    const auto get_version = reinterpret_cast<fRtlGetVersion>(GetProcAddress(ntdll, "RtlGetVersion"));
    if (get_version != nullptr) get_version(&info);
  }
  return info;
}

std::string architecture() {
  SYSTEM_INFO info = {};
  GetNativeSystemInfo(&info);
  switch (info.wProcessorArchitecture) {
    case PROCESSOR_ARCHITECTURE_AMD64:
      return "AMD64";
    case PROCESSOR_ARCHITECTURE_ARM:
      return "ARM";
    case PROCESSOR_ARCHITECTURE_ARM64:
      return "ARM64";
    case PROCESSOR_ARCHITECTURE_INTEL:
      return "x86";
    default:
      return "";
  }
}

std::string processor_name() {
  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &key) != ERROR_SUCCESS) return "";
  wchar_t buffer[256] = {};
  DWORD size = sizeof(buffer);
  DWORD type = 0;
  std::string out;
  if (RegQueryValueExW(key, L"ProcessorNameString", nullptr, &type, reinterpret_cast<LPBYTE>(buffer), &size) == ERROR_SUCCESS && type == REG_SZ) {
    out = utf8::cvt<std::string>(std::wstring(buffer));
  }
  RegCloseKey(key);
  return out;
}

}  // namespace

system_info gather() {
  system_info out;
  out.system = "Windows";
  out.node = computer_name();

  const RTL_OSVERSIONINFOW version = os_version();
  out.release = str::xtos(version.dwMajorVersion) + "." + str::xtos(version.dwMinorVersion);
  out.version = out.release + "." + str::xtos(version.dwBuildNumber);
  out.machine = architecture();
  out.processor = processor_name();

  TIME_ZONE_INFORMATION tz = {};
  const DWORD tz_result = GetTimeZoneInformation(&tz);
  if (tz_result == TIME_ZONE_ID_DAYLIGHT) {
    out.timezone = utf8::cvt<std::string>(std::wstring(tz.DaylightName));
  } else if (tz_result != TIME_ZONE_ID_INVALID) {
    out.timezone = utf8::cvt<std::string>(std::wstring(tz.StandardName));
  }
  return out;
}

std::vector<std::string> logged_on_users() {
  std::vector<std::string> out;
  PWTS_SESSION_INFOW sessions = nullptr;
  DWORD count = 0;
  if (WTSEnumerateSessionsW(WTS_CURRENT_SERVER_HANDLE, 0, 1, &sessions, &count) == 0) return out;

  for (DWORD i = 0; i < count; ++i) {
    LPWSTR buffer = nullptr;
    DWORD bytes = 0;
    if (WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, sessions[i].SessionId, WTSUserName, &buffer, &bytes) != 0 && buffer != nullptr) {
      const std::wstring user(buffer);
      // Every machine has a Services session with no user; NCPA counts logons,
      // not sessions, so an empty name is not a user.
      if (!user.empty()) out.push_back(utf8::cvt<std::string>(user));
    }
    if (buffer != nullptr) WTSFreeMemory(buffer);
  }
  WTSFreeMemory(sessions);
  return out;
}

}  // namespace sysinfo
}  // namespace ncpa
