// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "icinga_agent_service.hpp"

#include <stdexcept>

#ifdef WIN32
#include <win/windows.hpp>
#else
#include <sys/wait.h>

#include <cstdio>
#endif

namespace icinga_agent {

#ifdef WIN32

namespace {
struct service_handle {
  SC_HANDLE handle;
  explicit service_handle(SC_HANDLE handle) : handle(handle) {}
  service_handle(const service_handle &) = delete;
  service_handle &operator=(const service_handle &) = delete;
  ~service_handle() {
    if (handle != nullptr) CloseServiceHandle(handle);
  }
};

std::runtime_error failure(const std::string &what, const DWORD code) {
  if (code == ERROR_ACCESS_DENIED) return std::runtime_error(what + ": access denied (run from an elevated prompt)");
  return std::runtime_error(what + ": Windows error " + std::to_string(code));
}
}  // namespace

std::string stop_icinga_service() {
  const service_handle manager(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
  if (manager.handle == nullptr) throw failure("Cannot open the service manager", GetLastError());
  const service_handle service(OpenServiceW(manager.handle, L"icinga2", SERVICE_STOP | SERVICE_QUERY_STATUS | SERVICE_CHANGE_CONFIG));
  if (service.handle == nullptr) {
    const DWORD code = GetLastError();
    if (code == ERROR_SERVICE_DOES_NOT_EXIST) throw std::runtime_error("There is no icinga2 service on this host");
    throw failure("Cannot open the icinga2 service", code);
  }
  if (!ChangeServiceConfigW(service.handle, SERVICE_NO_CHANGE, SERVICE_DISABLED, SERVICE_NO_CHANGE, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                            nullptr)) {
    throw failure("Cannot disable the icinga2 service", GetLastError());
  }
  SERVICE_STATUS status = {};
  if (!QueryServiceStatus(service.handle, &status)) throw failure("The icinga2 service is disabled, but its state cannot be read", GetLastError());
  if (status.dwCurrentState == SERVICE_STOPPED) return "The icinga2 service was not running; it is now disabled.";
  if (!ControlService(service.handle, SERVICE_CONTROL_STOP, &status)) {
    const DWORD code = GetLastError();
    if (code != ERROR_SERVICE_NOT_ACTIVE) throw failure("The icinga2 service is disabled, but could not be stopped", code);
  }
  // Icinga 2 waits for its running checks before it exits.
  for (int i = 0; i < 240; ++i) {
    if (!QueryServiceStatus(service.handle, &status)) throw failure("The icinga2 service is disabled, but its state cannot be read", GetLastError());
    if (status.dwCurrentState == SERVICE_STOPPED) return "The icinga2 service is stopped and disabled.";
    Sleep(250);
  }
  throw std::runtime_error("The icinga2 service is disabled, but did not stop within a minute");
}

#else

std::string stop_icinga_service() {
  // A fixed command: nothing from the command line or the configuration.
  FILE *pipe = popen("systemctl disable --now icinga2 2>&1", "r");
  if (pipe == nullptr) throw std::runtime_error("Cannot run systemctl to stop the icinga2 service");
  std::string output;
  char buffer[256];
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) output += buffer;
  const int status = pclose(pipe);
  while (!output.empty() && (output.back() == '\n' || output.back() == '\r')) output.pop_back();
  if (status == -1 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    throw std::runtime_error("`systemctl disable --now icinga2` failed" + (output.empty() ? std::string() : ": " + output));
  }
  return "The icinga2 service is stopped and disabled.";
}

#endif

}  // namespace icinga_agent
