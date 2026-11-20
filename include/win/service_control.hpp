// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
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

// The account an installed service is registered to run as, as the service
// control manager spells it: `LocalSystem`, `NT SERVICE\name`, `.\user`.
std::wstring GetAccount(const std::wstring& name);
// Register the service to run as `account`. For the accounts that have no
// password - LocalSystem, LocalService, NetworkService and a virtual account
// (`NT SERVICE\name`) - which are the only ones this is meant for.
void SetAccount(const std::wstring& name, const std::wstring& account);
// Give the service an unrestricted service SID: its token then carries
// `NT SERVICE\name`, so an ACL can name this one service.
void SetUnrestrictedSid(const std::wstring& name);
// Declare the privileges the service needs. Every privilege not listed is
// removed from its token when it starts. An empty list removes the
// declaration, which gives the service back its account's defaults.
void SetRequiredPrivileges(const std::wstring& name, const std::vector<std::wstring>& privileges);
}  // namespace win_service_control