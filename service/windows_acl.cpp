// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "windows_acl.hpp"

#ifdef WIN32

#include <windows.h>
// windows.h has to come first; accctrl/aclapi depend on its types.
#include <accctrl.h>
#include <aclapi.h>
#include <sddl.h>

#include <memory>
#include <str/utf8.hpp>
#include <vector>

namespace nsclient {
namespace windows_acl {

namespace {

struct local_free {
  void operator()(void *p) const {
    if (p != nullptr) ::LocalFree(p);
  }
};

std::string last_error(const char *what) { return std::string(what) + " failed: GetLastError=" + std::to_string(::GetLastError()); }

// Well-known SIDs, built rather than parsed so they are correct on any locale:
// a machine whose Administrators group is called something else still matches.
bool build_sid(const WELL_KNOWN_SID_TYPE type, std::vector<unsigned char> &storage, PSID &out, std::list<std::string> &errors) {
  DWORD size = SECURITY_MAX_SID_SIZE;
  storage.assign(size, 0);
  if (!::CreateWellKnownSid(type, nullptr, storage.data(), &size)) {
    errors.emplace_back(last_error("CreateWellKnownSid"));
    return false;
  }
  out = storage.data();
  return true;
}

}  // namespace

bool protect_directory(const std::string &path, std::list<std::string> &errors) {
  std::vector<unsigned char> system_sid_bytes, admin_sid_bytes;
  PSID system_sid = nullptr;
  PSID admin_sid = nullptr;
  if (!build_sid(WinLocalSystemSid, system_sid_bytes, system_sid, errors)) return false;
  if (!build_sid(WinBuiltinAdministratorsSid, admin_sid_bytes, admin_sid, errors)) return false;

  // Full control for both, inherited by everything created underneath - the
  // subfolders (security\, fleet\, log\) must not need separate treatment.
  EXPLICIT_ACCESS_W access[2] = {};
  for (int i = 0; i < 2; i++) {
    access[i].grfAccessPermissions = GENERIC_ALL;
    access[i].grfAccessMode = SET_ACCESS;
    access[i].grfInheritance = CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE;
    access[i].Trustee.TrusteeForm = TRUSTEE_IS_SID;
    access[i].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
  }
  access[0].Trustee.ptstrName = static_cast<LPWSTR>(system_sid);
  access[1].Trustee.ptstrName = static_cast<LPWSTR>(admin_sid);

  PACL raw_acl = nullptr;
  if (::SetEntriesInAclW(2, access, nullptr, &raw_acl) != ERROR_SUCCESS) {
    errors.emplace_back(last_error("SetEntriesInAcl"));
    return false;
  }
  const std::unique_ptr<void, local_free> acl(raw_acl);

  const std::wstring wide = utf8::cvt<std::wstring>(path);
  // PROTECTED_DACL_SECURITY_INFORMATION is the flag that breaks inheritance.
  // Without it the inherited "Users: Read & Execute" from %ProgramData%
  // survives next to the two ACEs above and the folder stays world-readable.
  const DWORD result = ::SetNamedSecurityInfoW(const_cast<LPWSTR>(wide.c_str()), SE_FILE_OBJECT,
                                               DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION, nullptr, nullptr, raw_acl, nullptr);
  if (result != ERROR_SUCCESS) {
    errors.emplace_back("SetNamedSecurityInfo failed: error=" + std::to_string(result));
    return false;
  }
  return true;
}

bool is_protected(const std::string &path, std::list<std::string> &errors) {
  PACL dacl = nullptr;
  PSECURITY_DESCRIPTOR raw_descriptor = nullptr;
  const std::wstring wide = utf8::cvt<std::wstring>(path);
  const DWORD result = ::GetNamedSecurityInfoW(wide.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, &dacl, nullptr, &raw_descriptor);
  if (result != ERROR_SUCCESS) {
    errors.emplace_back("GetNamedSecurityInfo failed: error=" + std::to_string(result));
    return false;
  }
  const std::unique_ptr<void, local_free> descriptor(raw_descriptor);
  if (dacl == nullptr) {
    // A NULL DACL grants everyone everything - the opposite of protected.
    errors.emplace_back("the directory has no DACL, which grants full access to everyone");
    return false;
  }

  std::vector<unsigned char> system_sid_bytes, admin_sid_bytes;
  PSID system_sid = nullptr;
  PSID admin_sid = nullptr;
  if (!build_sid(WinLocalSystemSid, system_sid_bytes, system_sid, errors)) return false;
  if (!build_sid(WinBuiltinAdministratorsSid, admin_sid_bytes, admin_sid, errors)) return false;

  bool clean = true;
  for (DWORD i = 0; i < dacl->AceCount; i++) {
    void *entry = nullptr;
    if (!::GetAce(dacl, i, &entry)) {
      errors.emplace_back(last_error("GetAce"));
      return false;
    }
    const ACE_HEADER *header = static_cast<ACE_HEADER *>(entry);
    if (header->AceType != ACCESS_ALLOWED_ACE_TYPE) continue;
    ACCESS_ALLOWED_ACE *allowed = static_cast<ACCESS_ALLOWED_ACE *>(entry);
    const PSID sid = reinterpret_cast<PSID>(&allowed->SidStart);
    if (::EqualSid(sid, system_sid) || ::EqualSid(sid, admin_sid)) continue;

    std::unique_ptr<char, local_free> text;
    char *raw_text = nullptr;
    const std::string who = ::ConvertSidToStringSidA(sid, &raw_text) ? (text.reset(raw_text), std::string(raw_text)) : std::string("<unknown sid>");
    errors.emplace_back("unexpected grant to " + who);
    clean = false;
  }
  return clean;
}

}  // namespace windows_acl
}  // namespace nsclient

#endif
