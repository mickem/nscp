// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Windows implementation of check_file_security: owner and DACL of a path via
// GetNamedSecurityInfo + GetAce + LookupAccountSid, plus resolution of a
// service's image path through the service control manager.

#include <Windows.h>
#include <aclapi.h>
#include <sddl.h>

#include <boost/algorithm/string.hpp>
#include <error/error.hpp>
#include <str/utf8.hpp>

#include "check_file_security.hpp"

namespace file_security_source {

using file_security_filter::ace;
using file_security_filter::filter_obj;

namespace {

std::string sid_to_string(PSID sid) {
  LPWSTR text = nullptr;
  if (sid == nullptr || !IsValidSid(sid) || !ConvertSidToStringSidW(sid, &text) || text == nullptr) return {};
  const std::string result = utf8::cvt<std::string>(text);
  LocalFree(text);
  return result;
}

// Resolve a SID to "DOMAIN\name"; empty when it has no account (orphaned SID).
std::string lookup_account(PSID sid) {
  if (sid == nullptr || !IsValidSid(sid)) return {};
  WCHAR name[256] = {0}, domain[256] = {0};
  DWORD name_len = 256, domain_len = 256;
  SID_NAME_USE use = SidTypeUnknown;
  if (!LookupAccountSidW(nullptr, sid, name, &name_len, domain, &domain_len, &use)) return {};
  const std::string account = utf8::cvt<std::string>(name);
  const std::string scope = utf8::cvt<std::string>(domain);
  return scope.empty() ? account : scope + "\\" + account;
}

bool is_file(const std::wstring &path) { return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES; }

// Windows records driver and service images in a handful of shapes; bring them
// back to a plain path so the security descriptor can be read.
std::string normalize_image_path(std::string path) {
  boost::trim(path);
  if (boost::istarts_with(path, "\\??\\")) path = path.substr(4);
  if (boost::istarts_with(path, "\\SystemRoot\\")) {
    WCHAR windows[MAX_PATH] = {0};
    if (GetWindowsDirectoryW(windows, MAX_PATH) > 0) path = utf8::cvt<std::string>(windows) + "\\" + path.substr(12);
  } else if (!path.empty() && path[0] == '\\' && path.find(':') == std::string::npos) {
    // A rooted path such as "\Windows\System32\drivers\x.sys".
    WCHAR windows[MAX_PATH] = {0};
    if (GetWindowsDirectoryW(windows, MAX_PATH) > 0) {
      const std::string win_dir = utf8::cvt<std::string>(windows);
      path = win_dir.substr(0, 2) + path;  // prefix the system drive letter
    }
  }
  return path;
}

// Split the executable out of a service command line: quoted paths are taken
// verbatim, unquoted ones are trimmed from the right until a real file is found
// (the classic "C:\Program Files\..." ambiguity).
std::string image_path_from_command_line(const std::string &command_line) {
  std::string cmd = command_line;
  boost::trim(cmd);
  if (cmd.empty()) return {};
  if (cmd[0] == '"') {
    const std::size_t end = cmd.find('"', 1);
    return normalize_image_path(end == std::string::npos ? cmd.substr(1) : cmd.substr(1, end - 1));
  }
  std::string candidate = normalize_image_path(cmd);
  for (;;) {
    if (is_file(utf8::cvt<std::wstring>(candidate))) return candidate;
    const std::size_t space = candidate.find_last_of(' ');
    if (space == std::string::npos) break;
    candidate = candidate.substr(0, space);
    boost::trim_right(candidate);
  }
  const std::size_t space = cmd.find(' ');
  return normalize_image_path(space == std::string::npos ? cmd : cmd.substr(0, space));
}

}  // namespace

bool supported() { return true; }

std::string service_binary(const std::string &service, std::string &error) {
  const SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
  if (scm == nullptr) {
    error = "Failed to open the service control manager: " + error::lookup::last_error();
    return {};
  }
  const std::wstring wname = utf8::cvt<std::wstring>(service);
  const SC_HANDLE handle = OpenServiceW(scm, wname.c_str(), SERVICE_QUERY_CONFIG);
  if (handle == nullptr) {
    const DWORD code = GetLastError();
    error = code == ERROR_SERVICE_DOES_NOT_EXIST ? "Service not found: " + service
                                                 : "Failed to open service " + service + ": " + error::format::from_system(code);
    CloseServiceHandle(scm);
    return {};
  }

  std::string result;
  DWORD needed = 0;
  QueryServiceConfigW(handle, nullptr, 0, &needed);
  if (needed == 0) {
    error = "Failed to read the configuration of service " + service + ": " + error::lookup::last_error();
  } else {
    std::vector<char> buffer(needed);
    QUERY_SERVICE_CONFIGW *config = reinterpret_cast<QUERY_SERVICE_CONFIGW *>(buffer.data());
    if (!QueryServiceConfigW(handle, config, needed, &needed)) {
      error = "Failed to read the configuration of service " + service + ": " + error::lookup::last_error();
    } else if (config->lpBinaryPathName == nullptr) {
      error = "Service " + service + " has no image path";
    } else {
      result = image_path_from_command_line(utf8::cvt<std::string>(config->lpBinaryPathName));
      if (result.empty()) error = "Could not determine the image path of service " + service;
    }
  }

  CloseServiceHandle(handle);
  CloseServiceHandle(scm);
  return result;
}

void inspect(filter_obj &obj, std::vector<ace> &aces) {
  const std::wstring wpath = utf8::cvt<std::wstring>(obj.path);
  const DWORD attributes = GetFileAttributesW(wpath.c_str());
  if (attributes == INVALID_FILE_ATTRIBUTES) {
    const DWORD code = GetLastError();
    obj.exists = 0;
    obj.readable = 0;
    // "Not there" is reported through exists/state; anything else (a denied
    // parent directory, a dead network path) is worth spelling out.
    if (code != ERROR_FILE_NOT_FOUND && code != ERROR_PATH_NOT_FOUND) obj.error = error::format::from_system(code);
    return;
  }
  obj.exists = 1;
  obj.is_dir = (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ? 1 : 0;

  PSID owner = nullptr;
  PACL dacl = nullptr;
  PSECURITY_DESCRIPTOR descriptor = nullptr;
  const DWORD rc = GetNamedSecurityInfoW(wpath.c_str(), SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, &owner, nullptr, &dacl,
                                         nullptr, &descriptor);
  if (rc != ERROR_SUCCESS) {
    obj.readable = 0;
    obj.error = error::format::from_system(rc);
    if (descriptor != nullptr) LocalFree(descriptor);
    return;
  }
  obj.readable = 1;

  obj.owner_sid = sid_to_string(owner);
  obj.owner = lookup_account(owner);
  if (obj.owner.empty()) obj.owner = obj.owner_sid;

  if (dacl == nullptr) {
    // A NULL DACL is not "no access" but "unrestricted access" - the worst case
    // this check exists to find, so model it as an explicit Everyone/full entry.
    aces.push_back(ace("Everyone", "S-1-1-0", file_security_filter::mask_generic_all, true));
    LocalFree(descriptor);
    return;
  }

  // Walk the ACL entry by entry rather than through GetExplicitEntriesFromAcl:
  // that API only returns the ACEs set directly on the object, and the access a
  // hardening check is looking for is usually inherited from a parent folder.
  ACL_SIZE_INFORMATION size_info = {0};
  if (!GetAclInformation(dacl, &size_info, sizeof(size_info), AclSizeInformation)) {
    obj.error = "Failed to read the access control list: " + error::lookup::last_error();
    LocalFree(descriptor);
    return;
  }
  for (DWORD i = 0; i < size_info.AceCount; ++i) {
    LPVOID raw = nullptr;
    if (!GetAce(dacl, i, &raw)) continue;
    const ACE_HEADER *header = static_cast<ACE_HEADER *>(raw);

    PSID trustee = nullptr;
    ACCESS_MASK mask = 0;
    bool allow = true;
    if (header->AceType == ACCESS_ALLOWED_ACE_TYPE) {
      ACCESS_ALLOWED_ACE *entry = static_cast<ACCESS_ALLOWED_ACE *>(raw);
      trustee = reinterpret_cast<PSID>(&entry->SidStart);
      mask = entry->Mask;
    } else if (header->AceType == ACCESS_DENIED_ACE_TYPE) {
      ACCESS_DENIED_ACE *entry = static_cast<ACCESS_DENIED_ACE *>(raw);
      trustee = reinterpret_cast<PSID>(&entry->SidStart);
      mask = entry->Mask;
      allow = false;
    } else {
      continue;  // audit/alarm entries say nothing about who may write
    }
    // An inherit-only entry does not apply to the object itself; on a folder it
    // still decides what everything inside it gets, so keep those.
    if ((header->AceFlags & INHERIT_ONLY_ACE) != 0 && obj.is_dir == 0) continue;

    std::string name = lookup_account(trustee);
    const std::string sid = sid_to_string(trustee);
    if (name.empty()) name = sid;
    if (name.empty()) continue;
    aces.push_back(ace(name, sid, static_cast<unsigned long>(mask), allow, (header->AceFlags & INHERITED_ACE) != 0));
  }

  LocalFree(descriptor);
}

}  // namespace file_security_source
