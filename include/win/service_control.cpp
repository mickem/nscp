// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <tchar.h>

#include <error/error.hpp>
#include <iostream>
#include <str/utf8.hpp>
#include <vector>
#include <win/service_control.hpp>
#include <win/windows.hpp>

namespace win_service_control {
/**
 * Installs the service
 *
 * @param szName
 * @param szDisplayName
 * @param szDependencies
 *
 * @date 03-13-2004
 */
void Install(std::wstring szName, std::wstring szDisplayName, std::wstring szDependencies, DWORD dwServiceType, std::wstring args, std::wstring exe) {
  SC_HANDLE schService;
  SC_HANDLE schSCManager;

  if (exe.empty()) {
    TCHAR szPath[512] = {};
    const DWORD copied = GetModuleFileName(NULL, szPath, 512);
    if (copied == 0 || copied >= 512) throw service_control_exception("Could not get module");
    exe = szPath;
  }

  std::wstring bin = _T("\"") + exe + _T("\"");
  if (!args.empty()) bin += L" " + args;
  // lpDependencies is a MULTI_SZ: two terminators, which c_str() alone
  // does not provide.
  std::wstring dependencies = szDependencies;
  dependencies.push_back(L'\0');
  schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
  if (!schSCManager) throw service_control_exception("OpenSCManager failed:" + error::lookup::last_error());
  schService = CreateService(schSCManager,            // SCManager database
                             szName.c_str(),          // name of service
                             szDisplayName.c_str(),   // name to display
                             SERVICE_ALL_ACCESS,      // desired access
                             dwServiceType,           // service type
                             SERVICE_AUTO_START,      // start type
                             SERVICE_ERROR_NORMAL,    // error control type
                             bin.c_str(),             // service's binary
                             NULL,                    // no load ordering group
                             NULL,                    // no tag identifier
                             dependencies.c_str(),    // dependencies (double-NUL terminated)
                             NULL,                    // LocalSystem account
                             NULL);                   // no password

  if (!schService) {
    DWORD err = GetLastError();
    CloseServiceHandle(schSCManager);
    if (err == ERROR_SERVICE_EXISTS) {
      throw service_control_exception("Service already installed!");
    }
    throw service_control_exception("Unable to install service." + error::lookup::last_error(err));
  }
  std::cout << "Service " << utf8::cvt<std::string>(szName) << " (" << utf8::cvt<std::string>(bin) << ") installed..." << std::endl;
  ;
  CloseServiceHandle(schService);
  CloseServiceHandle(schSCManager);
}

void ModifyServiceType(LPCTSTR szName, DWORD dwServiceType) {
  SC_HANDLE schService;
  SC_HANDLE schSCManager;
  TCHAR szPath[512];

  if (GetModuleFileName(NULL, szPath, 512) == 0) throw service_control_exception("Could not get module");

  schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
  if (!schSCManager) throw service_control_exception("OpenSCManager failed: " + error::lookup::last_error());
  schService = OpenService(schSCManager, szName, SERVICE_ALL_ACCESS);
  if (!schService) {
    DWORD err = GetLastError();
    CloseServiceHandle(schSCManager);
    throw service_control_exception("Unable to open service: " + error::lookup::last_error(err));
  }
  BOOL result = ChangeServiceConfig(schService, dwServiceType, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
  CloseServiceHandle(schService);
  CloseServiceHandle(schSCManager);
  if (result != TRUE) throw service_control_exception("Could not change service information");
}

DWORD GetServiceType(LPCTSTR szName) {
  LPQUERY_SERVICE_CONFIG lpqscBuf = (LPQUERY_SERVICE_CONFIG)LocalAlloc(LPTR, 4096);
  if (lpqscBuf == NULL) {
    throw service_control_exception("Could not allocate memory");
  }
  SC_HANDLE schService;
  SC_HANDLE schSCManager;
  TCHAR szPath[512];

  if (GetModuleFileName(NULL, szPath, 512) == 0) throw service_control_exception("Could not get module");

  schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
  if (!schSCManager) throw service_control_exception("OpenSCManager failed.");
  schService = OpenService(schSCManager, szName, SERVICE_ALL_ACCESS);
  if (!schService) {
    DWORD err = GetLastError();
    CloseServiceHandle(schSCManager);
    throw service_control_exception("Unable to open service: " + error::lookup::last_error(err));
  }

  DWORD dwBytesNeeded = 0;
  BOOL success = QueryServiceConfig(schService, lpqscBuf, 4096, &dwBytesNeeded);
  CloseServiceHandle(schService);
  CloseServiceHandle(schSCManager);
  if (success != TRUE) throw service_control_exception("Could not query service information");
  DWORD ret = lpqscBuf->dwServiceType;
  LocalFree(lpqscBuf);
  return ret;
}

/**
 * Stars the service.
 *
 * @param name The name of the service to start
 *
 * @date 03-13-2004
 */
void Start(std::wstring name) {
  SC_HANDLE schService;
  SC_HANDLE schSCManager;
  SERVICE_STATUS ssStatus;

  schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
  if (!schSCManager) throw service_control_exception("OpenSCManager failed: " + error::lookup::last_error());
  schService = OpenService(schSCManager, name.c_str(), SERVICE_ALL_ACCESS);
  if (schService) {
    // try to stop the service
    if (StartService(schService, 0, NULL)) {
      std::cout << "Starting " << utf8::cvt<std::string>(name);
      Sleep(1000);
      while (QueryServiceStatus(schService, &ssStatus)) {
        if (ssStatus.dwCurrentState == SERVICE_START_PENDING) {
          std::cout << ".";
          Sleep(1000);
        } else
          break;
      }
      if (ssStatus.dwCurrentState != SERVICE_RUNNING) {
        CloseServiceHandle(schService);
        CloseServiceHandle(schSCManager);
        throw service_control_exception("Service '" + utf8::cvt<std::string>(name) + "' failed to start.");
      }
    }
    CloseServiceHandle(schService);
  } else {
    std::string err = "OpenService on '" + utf8::cvt<std::string>(name) + "' failed: " + error::lookup::last_error();
    CloseServiceHandle(schSCManager);
    throw service_control_exception(err);
  }
  CloseServiceHandle(schSCManager);
}
bool isInstalled(std::wstring name) {
  SC_HANDLE schService;
  SC_HANDLE schSCManager;
  schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
  if (!schSCManager) throw service_control_exception("OpenSCManager failed: " + error::lookup::last_error());
  schService = OpenService(schSCManager, name.c_str(), SERVICE_ALL_ACCESS);
  if (schService) {
    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
    return true;
  } else {
    CloseServiceHandle(schSCManager);
    return false;
  }
}
std::wstring get_exe_path(std::wstring svc_name) {
  std::wstring ret;
  SC_HANDLE schService;
  SC_HANDLE schSCManager;
  schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
  if (!schSCManager) throw service_control_exception("OpenSCManager failed: " + error::lookup::last_error());
  schService = OpenService(schSCManager, svc_name.c_str(), SERVICE_QUERY_CONFIG);
  if (!schService) {
    CloseServiceHandle(schSCManager);
    throw service_control_exception("Failed to open service: " + utf8::cvt<std::string>(svc_name) + " because " + error::lookup::last_error());
  }
  DWORD dwBytesNeeded = 0;
  DWORD lErr = 0;
  if (QueryServiceConfig(schService, NULL, 0, &dwBytesNeeded) || ((lErr = GetLastError()) != ERROR_INSUFFICIENT_BUFFER)) {
    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
    throw service_control_exception("Failed to query service information: " + utf8::cvt<std::string>(svc_name) + " because " + error::lookup::last_error(lErr));
  }

  LPQUERY_SERVICE_CONFIG lpqscBuf = (LPQUERY_SERVICE_CONFIG)LocalAlloc(LPTR, dwBytesNeeded + 10);
  BOOL bRet = (lpqscBuf != NULL) && (QueryServiceConfig(schService, lpqscBuf, dwBytesNeeded, &dwBytesNeeded) == TRUE);
  if (!bRet)
    lErr = GetLastError();
  else {
    ret = lpqscBuf->lpBinaryPathName ? lpqscBuf->lpBinaryPathName : L"";
  }
  LocalFree(lpqscBuf);
  CloseServiceHandle(schService);
  CloseServiceHandle(schSCManager);
  if (!bRet)
    throw service_control_exception("Failed to query service information: " + utf8::cvt<std::string>(svc_name) + " because " + error::lookup::last_error(lErr));
  return ret;
}

bool isStarted(std::wstring name) {
  SC_HANDLE schService;
  SC_HANDLE schSCManager;
  SERVICE_STATUS ssStatus;
  bool ret = false;

  schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
  if (!schSCManager) throw service_control_exception("OpenSCManager failed: " + error::lookup::last_error());
  schService = OpenService(schSCManager, name.c_str(), SERVICE_ALL_ACCESS);
  if (schService) {
    if (QueryServiceStatus(schService, &ssStatus)) {
      if (ssStatus.dwCurrentState == SERVICE_RUNNING) {
        ret = true;
      } else if (ssStatus.dwCurrentState == SERVICE_START_PENDING) {
        ret = true;
      }
    }
    CloseServiceHandle(schService);
  } else {
    CloseServiceHandle(schSCManager);
    ret = false;
  }
  CloseServiceHandle(schSCManager);
  return ret;
}

/**
 * Stops and removes the service
 *
 * @param name The name of the service to uninstall
 *
 * @date 03-13-2004
 */
void Uninstall(std::wstring name) {
  SC_HANDLE schService;
  SC_HANDLE schSCManager;
  Stop(name);

  schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
  if (!schSCManager) throw service_control_exception("OpenSCManager failed: " + error::lookup::last_error());
  schService = OpenService(schSCManager, name.c_str(), SERVICE_ALL_ACCESS);
  if (schService) {
    if (!DeleteService(schService)) {
      std::string err = "DeleteService failed: " + error::lookup::last_error();
      CloseServiceHandle(schService);
      CloseServiceHandle(schSCManager);
      throw service_control_exception(err);
    }
    CloseServiceHandle(schService);
  } else {
    std::string err = "OpenService failed: " + error::lookup::last_error();
    CloseServiceHandle(schSCManager);
    throw service_control_exception(err);
  }
  CloseServiceHandle(schSCManager);
}

/**
 * Stops the service
 *
 * @param name The name of the serive to stop
 *
 * @date 03-13-2004
 */
void Stop(std::wstring name) {
  SC_HANDLE schService;
  SC_HANDLE schSCManager;
  SERVICE_STATUS ssStatus;

  schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
  if (!schSCManager) throw service_control_exception("OpenSCManager failed: " + error::lookup::last_error());
  schService = OpenService(schSCManager, name.c_str(), SERVICE_ALL_ACCESS);
  if (schService) {
    // try to stop the service
    if (ControlService(schService, SERVICE_CONTROL_STOP, &ssStatus)) {
      std::cout << "Stopping service.";
      Sleep(1000);
      while (QueryServiceStatus(schService, &ssStatus)) {
        if (ssStatus.dwCurrentState == SERVICE_STOP_PENDING) {
          std::cout << ".";
          Sleep(1000);
        } else
          break;
      }
      std::cout << std::endl;
      if (ssStatus.dwCurrentState != SERVICE_STOPPED) {
        CloseServiceHandle(schService);
        CloseServiceHandle(schSCManager);
        throw service_control_exception("Service failed to stop.");
      }
    }
    CloseServiceHandle(schService);
  } else {
    std::string err = "OpenService failed: " + error::lookup::last_error();
    CloseServiceHandle(schSCManager);
    throw service_control_exception(err);
  }
  CloseServiceHandle(schSCManager);
}

void StopNoWait(std::wstring name) {
  SC_HANDLE schService;
  SC_HANDLE schSCManager;
  SERVICE_STATUS ssStatus;

  schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
  if (!schSCManager) throw service_control_exception("OpenSCManager failed.");
  schService = OpenService(schSCManager, name.c_str(), SERVICE_ALL_ACCESS);
  if (schService) {
    // try to stop the service
    ControlService(schService, SERVICE_CONTROL_STOP, &ssStatus);
    CloseServiceHandle(schService);
  } else {
    CloseServiceHandle(schSCManager);
    throw service_control_exception("OpenService failed.");
  }
  CloseServiceHandle(schSCManager);
}

typedef BOOL(WINAPI* PFChangeServiceConfig2)(SC_HANDLE hService, DWORD dwInfoLevel, LPVOID lpInfo);

void SetDescription(std::wstring name, std::wstring desc) {
  PFChangeServiceConfig2 FChangeServiceConfig2;
  HMODULE ADVAPI = ::LoadLibrary(_T("Advapi32"));
  if (!ADVAPI) {
    throw service_control_exception("Couldn't set extended service info (ignore this on NT4).");
  }
#ifdef UNICODE
  FChangeServiceConfig2 = (PFChangeServiceConfig2)::GetProcAddress(ADVAPI, "ChangeServiceConfig2W");
#else
  FChangeServiceConfig2 = (PFChangeServiceConfig2)::GetProcAddress(ADVAPI, _TEXT("ChangeServiceConfig2A"));
#endif
  if (!FChangeServiceConfig2) {
    FreeLibrary(ADVAPI);
    throw service_control_exception("Couldn't set extended service info (ignore this on NT4).");
  }
  SERVICE_DESCRIPTION descr;
  SC_HANDLE schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
  if (!schSCManager) throw service_control_exception("OpenSCManager failed.");
  SC_HANDLE schService = OpenService(schSCManager, name.c_str(), SERVICE_ALL_ACCESS);
  if (!schService) {
    FreeLibrary(ADVAPI);
    CloseServiceHandle(schSCManager);
    throw service_control_exception("OpenService failed.");
  }

  const size_t d_len = desc.length() + 2;
  auto* d = new TCHAR[d_len];
  wcsncpy_s(d, d_len, desc.c_str(), desc.length() + 1);
  descr.lpDescription = d;
  const BOOL bResult = FChangeServiceConfig2(schService, SERVICE_CONFIG_DESCRIPTION, &descr);
  delete[] d;
  FreeLibrary(ADVAPI);
  CloseServiceHandle(schService);
  CloseServiceHandle(schSCManager);
  if (!bResult) throw service_control_exception("ChangeServiceConfig2 failed.");
}
namespace {
// The two handles every configuration change needs, closed on the way out.
struct service_handle {
  SC_HANDLE manager = nullptr;
  SC_HANDLE service = nullptr;

  service_handle(const std::wstring &name, const DWORD access) {
    manager = ::OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!manager) throw service_control_exception("OpenSCManager failed: " + error::lookup::last_error());
    service = ::OpenServiceW(manager, name.c_str(), access);
    if (!service) {
      const DWORD err = ::GetLastError();
      ::CloseServiceHandle(manager);
      throw service_control_exception("OpenService failed: " + error::lookup::last_error(err));
    }
  }
  ~service_handle() {
    if (service) ::CloseServiceHandle(service);
    if (manager) ::CloseServiceHandle(manager);
  }
  service_handle(const service_handle &) = delete;
  service_handle &operator=(const service_handle &) = delete;
};

// Declared here rather than taken from winsvc.h: the SDK only defines these
// when it targets Vista or later, and this file is also built for Windows XP.
// There ChangeServiceConfig2 answers ERROR_INVALID_LEVEL, which is reported
// like any other failure - the feature needs Windows 7 / Server 2008 R2.
const DWORD config_service_sid_info = 5;
const DWORD config_required_privileges_info = 6;
const DWORD sid_type_unrestricted = 1;
struct service_sid_info {
  DWORD sid_type;
};
struct required_privileges_info {
  LPWSTR privileges;
};
}  // namespace

std::wstring GetAccount(const std::wstring &name) {
  const service_handle handle(name, SERVICE_QUERY_CONFIG);
  DWORD needed = 0;
  if (::QueryServiceConfigW(handle.service, nullptr, 0, &needed) || ::GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
    throw service_control_exception("QueryServiceConfig failed: " + error::lookup::last_error());
  }
  std::vector<unsigned char> buffer(needed, 0);
  QUERY_SERVICE_CONFIGW *config = reinterpret_cast<QUERY_SERVICE_CONFIGW *>(buffer.data());
  if (!::QueryServiceConfigW(handle.service, config, needed, &needed)) {
    throw service_control_exception("QueryServiceConfig failed: " + error::lookup::last_error());
  }
  return config->lpServiceStartName ? std::wstring(config->lpServiceStartName) : std::wstring();
}

void SetAccount(const std::wstring &name, const std::wstring &account) {
  const service_handle handle(name, SERVICE_CHANGE_CONFIG);
  // An empty password, not a null one: null means "leave the password alone",
  // and a service that was once given a real account would keep presenting
  // that account's password for one that has none.
  if (!::ChangeServiceConfigW(handle.service, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, nullptr, nullptr, nullptr, nullptr, account.c_str(), L"",
                              nullptr)) {
    throw service_control_exception("ChangeServiceConfig(account) failed: " + error::lookup::last_error());
  }
}

void SetUnrestrictedSid(const std::wstring &name) {
  const service_handle handle(name, SERVICE_CHANGE_CONFIG);
  service_sid_info info = {sid_type_unrestricted};
  if (!::ChangeServiceConfig2W(handle.service, config_service_sid_info, &info)) {
    throw service_control_exception("ChangeServiceConfig2(service sid) failed: " + error::lookup::last_error());
  }
}

void SetRequiredPrivileges(const std::wstring &name, const std::vector<std::wstring> &privileges) {
  const service_handle handle(name, SERVICE_CHANGE_CONFIG);
  // A MULTI_SZ: each name terminated, and one more terminator after the last.
  std::vector<wchar_t> multi;
  for (const std::wstring &privilege : privileges) {
    multi.insert(multi.end(), privilege.begin(), privilege.end());
    multi.push_back(L'\0');
  }
  multi.push_back(L'\0');
  if (privileges.empty()) multi.push_back(L'\0');
  required_privileges_info info = {multi.data()};
  if (!::ChangeServiceConfig2W(handle.service, config_required_privileges_info, &info)) {
    throw service_control_exception("ChangeServiceConfig2(required privileges) failed: " + error::lookup::last_error());
  }
}
}  // namespace win_service_control