// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#ifdef WIN32

#include <list>
#include <string>

namespace nsclient {
namespace windows_acl {

// Lock a directory down to SYSTEM and the local Administrators group: take
// ownership, grant those two, and stop it inheriting anything else.
//
// This exists for the modern layout's shared folder under %ProgramData%, which
// holds the configuration (web and target passwords), the fleet identity's
// private key and the agent's log. %ProgramData% itself grants
// `Users: Read & Execute` by inheritance, so a folder created there is
// world-readable unless inheritance is explicitly broken - which makes a naive
// move out of Program Files a downgrade rather than a fix.
//
// Two separate things have to be true, and each is a trap on its own:
//
//  - Inheritance must be broken. Adding two explicit ACEs while leaving
//    inheritance on leaves the inherited `Users: Read` in place beside them.
//  - The directory must be *owned* by SYSTEM or Administrators. %ProgramData%
//    also grants Users create-folder plus an inherit-only `CREATOR OWNER: Full`,
//    so a standard user can pre-create our folder before we first run and own
//    it. An owner keeps implicit READ_CONTROL | WRITE_DAC regardless of the
//    DACL, so an unowned fix is one the excluded account can simply undo.
//
// Returns true when the directory ends up owned by Administrators and with the
// intended DACL. On failure `errors` describes what went wrong; the caller
// decides how loud to be.
bool protect_directory(const std::string &path, std::list<std::string> &errors);

// Read back the owner and the DACL and report whether the directory is in the
// state protect_directory aims for: owned by SYSTEM or Administrators, with no
// ACE granting anyone else. Used by the caller to verify rather than assume -
// "we set a DACL" and "the DACL excludes everyone else" are different claims,
// and both traps above are cases where the first is true and the second is not.
bool is_protected(const std::string &path, std::list<std::string> &errors);

}  // namespace windows_acl
}  // namespace nsclient

#endif
