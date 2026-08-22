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

// What could be determined about a directory's protection.
enum class protection {
  // Owned by SYSTEM or Administrators, and no ACE grants anyone else.
  restricted,
  // Someone else has access, or owns it (and so can grant themselves access).
  open,
  // The security descriptor could not be read at all. Distinct from `open` on
  // purpose: the most common reason is that the caller is not privileged
  // enough to read it, which is what a *correctly* locked-down folder looks
  // like from an unelevated process. Treating that as "wide open" produces a
  // frightening warning about a folder that is in fact exactly right.
  unknown,
};

// Read back the owner and the DACL and report which of the three above applies.
// Used by the caller to verify rather than assume - "we set a DACL" and "the
// DACL excludes everyone else" are different claims, and both traps above are
// cases where the first is true and the second is not.
protection inspect_protection(const std::string &path, std::list<std::string> &errors);

// inspect_protection() for callers that have the access to tell the difference
// and only care whether the directory is locked down. Anything other than
// `restricted` is false.
bool is_protected(const std::string &path, std::list<std::string> &errors);

// Make a file or directory inherit its security from its parent, discarding
// whatever descriptor it carried, and propagate that through any subtree
// below it.
//
// This exists for the layout migration: a same-volume rename keeps the file's
// old security descriptor, so nsclient.ini and the fleet identity's private
// key arrive inside the locked-down shared folder still carrying the
// `Users: Read & Execute` ACEs they inherited in Program Files - readable by
// every account on the machine while the folder around them claims otherwise.
// Resetting to inherit-only makes the entry pick up exactly the folder's
// SYSTEM + Administrators ACEs. (A cross-volume move is a copy and needs no
// help: new files inherit from where they are created.)
bool reset_to_inherited(const std::string &path, std::list<std::string> &errors);

}  // namespace windows_acl
}  // namespace nsclient

#endif
