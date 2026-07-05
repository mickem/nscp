// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <sstream>
#include <stdexcept>
#include <utility>
#include <win/windows.hpp>

namespace win_service_control {
class service_control_exception : public std::exception {
 public:
  std::string error_;
  explicit service_control_exception(std::string error) : error_(std::move(error)) {}
  service_control_exception(std::string error, const int code) : error_(std::move(error)) {
    std::stringstream ss;
    ss << ": ";
    ss << code;
    error_ += ss.str();
  }
  const char* what() const noexcept override { return error_.c_str(); }
};
void Install(std::wstring, std::wstring, std::wstring, DWORD = SERVICE_WIN32_OWN_PROCESS, std::wstring args = std::wstring(),
             std::wstring exe = std::wstring());
void ModifyServiceType(LPCTSTR szName, DWORD dwServiceType);
void Uninstall(std::wstring);
void Start(std::wstring);
bool isStarted(std::wstring);
bool isInstalled(std::wstring name);
void Stop(std::wstring);
void StopNoWait(std::wstring);
void SetDescription(std::wstring, std::wstring);
DWORD GetServiceType(LPCTSTR szName);
std::wstring get_exe_path(std::wstring svc_name);
}  // namespace win_service_control