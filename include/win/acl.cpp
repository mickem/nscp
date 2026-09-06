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

struct close_handle {
  void operator()(void *h) const {
    if (h != nullptr && h != INVALID_HANDLE_VALUE) ::CloseHandle(h);
  }
};
typedef std::unique_ptr<void, close_handle> unique_handle;

// Open the entry itself - never whatever a junction or symbolic link at that
// name points to.
//
// FILE_FLAG_OPEN_REPARSE_POINT is what makes the open land on the entry rather
// than on its target, so the object that gets secured (or read back) is always
// the one that was named: a link swapped in after a caller's check can at
// worst have us act on the link itself, never on something we did not name.
// FILE_FLAG_BACKUP_SEMANTICS is required to open a directory at all, and with
// SeRestorePrivilege (enabled by protect_directory) it opens one whose DACL
// would otherwise shut us out.
//
// Deliberately no GetFileInformationByHandle() to classify what was opened:
// that would want read access on a handle protect_directory() opens for
// writing only, and the callers establish what the entry is beforehand with
// is_reparse_point(), which needs no access to the object at all.
unique_handle open_no_follow(const std::wstring &wide, const DWORD access, std::list<std::string> &errors) {
  unique_handle handle(::CreateFileW(wide.c_str(), access, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                                     FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
  if (handle.get() == INVALID_HANDLE_VALUE) {
    errors.emplace_back(last_error("CreateFile"));
    return unique_handle();
  }
  return handle;
}

}  // namespace

bool is_reparse_point(const std::string &path, std::list<std::string> &errors) {
  const std::wstring wide = utf8::cvt<std::wstring>(path);
  // Attributes, not a handle: readable through the parent's list permission,
  // which %ProgramData% grants everyone, so an unelevated process gets the
  // same answer as the service for a folder it cannot otherwise open.
  const DWORD attributes = ::GetFileAttributesW(wide.c_str());
  if (attributes == INVALID_FILE_ATTRIBUTES) {
    errors.emplace_back(last_error("GetFileAttributes"));
    return false;
  }
  return (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}

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

  // Held but not enabled by default, and what lets us open and re-own a
  // directory whose current DACL denies us - the case below depends on it.
  // Spelled out rather than SE_TAKE_OWNERSHIP_NAME / SE_RESTORE_NAME: those are
  // TEXT() macros and only widen under a UNICODE build, which this is not
  // required to be.
  enable_privilege(L"SeTakeOwnershipPrivilege");
  enable_privilege(L"SeRestorePrivilege");

  // A junction or symbolic link where the folder should be is refused, not
  // secured: %ProgramData% lets any local account create one under our name
  // before we first run, and securing it would apply the owner and DACL to
  // wherever it points while the link itself stays that account's to remove
  // and replace. Checked from attributes, which need no access to the object.
  if (is_reparse_point(path, errors)) {
    errors.emplace_back("refusing to secure " + path + ": it is a junction or symbolic link, not a real directory");
    return false;
  }

  // Ownership and the DACL through two separate opens of the entry itself, so
  // the object secured is provably the one named and checked rather than a
  // link's target - and each open asks only for the rights its own call needs.
  //
  // Ownership first, and separately: %ProgramData% grants Users create-folder
  // plus an inherit-only "CREATOR OWNER: Full", so a standard user can
  // pre-create C:\ProgramData\NSClient++ before we ever run and own it - and an
  // owner keeps implicit READ_CONTROL | WRITE_DAC whatever the DACL says, so a
  // DACL fixed under a foreign owner is one that account can put straight back
  // (and read the configuration and the fleet private key through) while we
  // report the folder as restricted. Taking ownership first is also what lets
  // the second open succeed against a DACL that shuts us out - the case this
  // function exists for - because those two rights come with the ownership we
  // just took. A combined OWNER|DACL request on one handle is atomic in the
  // wrong direction: it would need every right up front and fix nothing when
  // only the ownership change is available.
  bool owner_ok = false;
  {
    const unique_handle handle = open_no_follow(wide, WRITE_OWNER, errors);
    if (handle) {
      const DWORD result = ::SetSecurityInfo(handle.get(), SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION, admin_sid, nullptr, nullptr, nullptr);
      owner_ok = result == ERROR_SUCCESS;
      if (!owner_ok) errors.emplace_back("SetSecurityInfo(owner) failed: error=" + std::to_string(result));
    }
  }

  // PROTECTED_DACL_SECURITY_INFORMATION is the flag that breaks inheritance.
  // Without it the inherited "Users: Read & Execute" from %ProgramData%
  // survives next to the two ACEs above and the folder stays world-readable.
  //
  // READ_CONTROL as well as WRITE_DAC, and not an over-request to trim: making
  // the DACL protected means converting the inherited ACEs that are there into
  // explicit ones, so the call reads the descriptor before it writes one.
  // Without it every protect_directory() call answers ERROR_ACCESS_DENIED.
  // Attempted even when the ownership change above failed, so the more
  // restrictive of the two still gets applied.
  {
    const unique_handle handle = open_no_follow(wide, READ_CONTROL | WRITE_DAC, errors);
    if (!handle) return false;
    const DWORD result =
        ::SetSecurityInfo(handle.get(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION, nullptr, nullptr, raw_acl, nullptr);
    if (result != ERROR_SUCCESS) {
      errors.emplace_back("SetSecurityInfo failed: error=" + std::to_string(result));
      return false;
    }
  }

  // An owner we do not control can undo everything above at any time, so a
  // failed ownership change is still a failure even with the DACL applied.
  if (!owner_ok) return false;
  return true;
}

protection inspect_protection(const std::string &path, std::list<std::string> &errors) {
  PACL dacl = nullptr;
  PSID owner = nullptr;
  PSECURITY_DESCRIPTOR raw_descriptor = nullptr;
  const std::wstring wide = utf8::cvt<std::wstring>(path);

  // A junction where the folder should be is "open" by definition: whoever
  // created the link controls where it points, whatever its target's DACL
  // says. Decided before the open, from attributes that need no access to the
  // object, so an unelevated caller gets this answer too.
  {
    std::list<std::string> attribute_errors;
    if (is_reparse_point(path, attribute_errors)) {
      errors.emplace_back("the entry is a junction or symbolic link rather than a real directory or file");
      return protection::open;
    }
  }

  // Files are a fair question too - the layout migration checks the entries it
  // moved (nsclient.ini, the fleet private key) - so nothing here insists on a
  // directory.
  const unique_handle handle = open_no_follow(wide, READ_CONTROL, errors);
  if (!handle) {
    // Not "open": we learned nothing. Opening for READ_CONTROL needs exactly
    // that right, which a folder restricted to SYSTEM and Administrators
    // deliberately denies everyone else - so this is the expected answer for an
    // unelevated caller looking at a folder that is working exactly as intended.
    return protection::unknown;
  }
  const DWORD result =
      ::GetSecurityInfo(handle.get(), SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, &owner, nullptr, &dacl, nullptr, &raw_descriptor);
  if (result != ERROR_SUCCESS) {
    errors.emplace_back("GetSecurityInfo failed: error=" + std::to_string(result));
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
