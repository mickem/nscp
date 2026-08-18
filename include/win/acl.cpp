// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <win/acl.hpp>

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

// The printable form of a SID, for error messages that have to name a
// principal we did not expect to find.
std::string describe_sid(const PSID sid) {
  char *raw_text = nullptr;
  if (!::ConvertSidToStringSidA(sid, &raw_text)) return "<unknown sid>";
  const std::unique_ptr<char, local_free> text(raw_text);
  return std::string(raw_text);
}

// Enable a privilege the process holds but that is not enabled by default.
// Best effort: the caller carries on either way, because the operation it
// guards usually succeeds without the privilege - it is only needed when the
// directory's current owner has denied us outright, which is exactly the case
// worth surviving.
bool enable_privilege(const wchar_t *name) {
  HANDLE raw_token = nullptr;
  if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &raw_token)) return false;

  TOKEN_PRIVILEGES privileges = {};
  privileges.PrivilegeCount = 1;
  privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
  bool enabled = false;
  if (::LookupPrivilegeValueW(nullptr, name, &privileges.Privileges[0].Luid)) {
    // AdjustTokenPrivileges reports success even when it changed nothing, so
    // the actual answer is in GetLastError.
    enabled = ::AdjustTokenPrivileges(raw_token, FALSE, &privileges, 0, nullptr, nullptr) != 0 && ::GetLastError() != ERROR_NOT_ALL_ASSIGNED;
  }
  ::CloseHandle(raw_token);
  return enabled;
}

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

  // Ownership first, and separately from the DACL.
  //
  // %ProgramData% grants Users create-folder plus an inherit-only
  // "CREATOR OWNER: Full", so a standard user can pre-create
  // C:\ProgramData\NSClient++ before we ever run and become its owner. An owner
  // keeps implicit READ_CONTROL | WRITE_DAC no matter what the DACL says, so
  // fixing only the DACL leaves the excluded account able to put its access
  // straight back - and read the configuration (passwords) and the fleet
  // private key - while we report the folder as restricted.
  //
  // Two calls rather than one: a combined OWNER|DACL request is atomic in the
  // wrong direction, failing the DACL fix on a machine where only the ownership
  // change is refused. These privileges are held but not enabled by default,
  // and are what lets us take a directory whose current DACL denies us.
  // Spelled out rather than SE_TAKE_OWNERSHIP_NAME / SE_RESTORE_NAME: those are
  // TEXT() macros and only widen under a UNICODE build, which this is not
  // required to be.
  enable_privilege(L"SeTakeOwnershipPrivilege");
  enable_privilege(L"SeRestorePrivilege");
  const DWORD owner_result =
      ::SetNamedSecurityInfoW(const_cast<LPWSTR>(wide.c_str()), SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION, admin_sid, nullptr, nullptr, nullptr);

  // PROTECTED_DACL_SECURITY_INFORMATION is the flag that breaks inheritance.
  // Without it the inherited "Users: Read & Execute" from %ProgramData%
  // survives next to the two ACEs above and the folder stays world-readable.
  const DWORD result = ::SetNamedSecurityInfoW(const_cast<LPWSTR>(wide.c_str()), SE_FILE_OBJECT,
                                               DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION, nullptr, nullptr, raw_acl, nullptr);
  if (result != ERROR_SUCCESS) {
    errors.emplace_back("SetNamedSecurityInfo failed: error=" + std::to_string(result));
    return false;
  }
  if (owner_result != ERROR_SUCCESS) {
    // Reported after the DACL attempt so the more restrictive of the two still
    // gets applied, but still a failure: an owner we do not control can undo
    // everything above at any time.
    errors.emplace_back("SetNamedSecurityInfo(owner) failed: error=" + std::to_string(owner_result));
    return false;
  }
  return true;
}

protection inspect_protection(const std::string &path, std::list<std::string> &errors) {
  PACL dacl = nullptr;
  PSID owner = nullptr;
  PSECURITY_DESCRIPTOR raw_descriptor = nullptr;
  const std::wstring wide = utf8::cvt<std::wstring>(path);
  const DWORD result =
      ::GetNamedSecurityInfoW(wide.c_str(), SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, &owner, nullptr, &dacl, nullptr,
                              &raw_descriptor);
  if (result != ERROR_SUCCESS) {
    // Not "open": we learned nothing. Reading a security descriptor needs
    // READ_CONTROL, which a folder restricted to SYSTEM and Administrators
    // deliberately denies everyone else - so this is the expected answer for an
    // unelevated caller looking at a folder that is working exactly as intended.
    errors.emplace_back("GetNamedSecurityInfo failed: error=" + std::to_string(result));
    return protection::unknown;
  }
  const std::unique_ptr<void, local_free> descriptor(raw_descriptor);
  if (dacl == nullptr) {
    // A NULL DACL grants everyone everything - the opposite of protected.
    errors.emplace_back("the directory has no DACL, which grants full access to everyone");
    return protection::open;
  }

  std::vector<unsigned char> system_sid_bytes, admin_sid_bytes;
  PSID system_sid = nullptr;
  PSID admin_sid = nullptr;
  if (!build_sid(WinLocalSystemSid, system_sid_bytes, system_sid, errors)) return protection::unknown;
  if (!build_sid(WinBuiltinAdministratorsSid, admin_sid_bytes, admin_sid, errors)) return protection::unknown;

  bool clean = true;

  // The owner is as load-bearing as the ACEs: it carries implicit
  // READ_CONTROL | WRITE_DAC, so a directory owned by anyone else is one whose
  // DACL can be rewritten by that account whenever it likes. A clean-looking
  // DACL under a foreign owner is precisely the state left behind by
  // pre-creating the folder, so it must not read as protected.
  if (owner == nullptr) {
    errors.emplace_back("the directory has no owner");
    clean = false;
  } else if (!::EqualSid(owner, system_sid) && !::EqualSid(owner, admin_sid)) {
    errors.emplace_back("owned by " + describe_sid(owner) + ", who keeps implicit WRITE_DAC and can undo this");
    clean = false;
  }
  for (DWORD i = 0; i < dacl->AceCount; i++) {
    void *entry = nullptr;
    if (!::GetAce(dacl, i, &entry)) {
      // Half an ACL read tells us nothing about the half we did not reach.
      errors.emplace_back(last_error("GetAce"));
      return protection::unknown;
    }
    const ACE_HEADER *header = static_cast<ACE_HEADER *>(entry);
    if (header->AceType != ACCESS_ALLOWED_ACE_TYPE) continue;
    ACCESS_ALLOWED_ACE *allowed = static_cast<ACCESS_ALLOWED_ACE *>(entry);
    const PSID sid = reinterpret_cast<PSID>(&allowed->SidStart);
    if (::EqualSid(sid, system_sid) || ::EqualSid(sid, admin_sid)) continue;

    errors.emplace_back("unexpected grant to " + describe_sid(sid));
    clean = false;
  }
  return clean ? protection::restricted : protection::open;
}

bool is_protected(const std::string &path, std::list<std::string> &errors) { return inspect_protection(path, errors) == protection::restricted; }

bool reset_to_inherited(const std::string &path, std::list<std::string> &errors) {
  // An *empty* ACL, not a NULL one: NULL grants everyone everything, while an
  // empty one grants nothing of its own. Combined with UNPROTECTED (re-enable
  // inheritance) the inherited ACEs become the only entries.
  //
  // TreeReset rather than SetNamedSecurityInfo: the plain set does propagate
  // the parent's inheritable ACEs through the subtree, but it leaves each
  // descendant's *explicit* ACEs in place - so a file that carried its own
  // grant (rather than an inherited one) at the source would keep it inside
  // the locked-down folder. KeepExplicit=FALSE is the whole point: every entry
  // under `path` ends up with purely inherited access from its new parent.
  ACL empty = {};
  if (!::InitializeAcl(&empty, sizeof(empty), ACL_REVISION)) {
    errors.emplace_back(last_error("InitializeAcl"));
    return false;
  }
  const std::wstring wide = utf8::cvt<std::wstring>(path);
  const DWORD result = ::TreeResetNamedSecurityInfoW(const_cast<LPWSTR>(wide.c_str()), SE_FILE_OBJECT,
                                                     DACL_SECURITY_INFORMATION | UNPROTECTED_DACL_SECURITY_INFORMATION, nullptr, nullptr, &empty,
                                                     nullptr, FALSE, nullptr, ProgressInvokeNever, nullptr);
  if (result != ERROR_SUCCESS) {
    errors.emplace_back("TreeResetNamedSecurityInfo(reset to inherited) failed: error=" + std::to_string(result));
    return false;
  }
  return true;
}

}  // namespace windows_acl
}  // namespace nsclient

#endif
