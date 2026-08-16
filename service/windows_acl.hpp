// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#ifdef WIN32

#include <list>
#include <string>

namespace nsclient {
namespace windows_acl {

// Lock a directory down to SYSTEM and the local Administrators group, and stop
// it inheriting anything else.
//
// This exists for the modern layout's shared folder under %ProgramData%, which
// holds the configuration (web and target passwords), the fleet identity's
// private key and the agent's log. %ProgramData% itself grants
// `Users: Read & Execute` by inheritance, so a folder created there is
// world-readable unless inheritance is explicitly broken - which makes a naive
// move out of Program Files a downgrade rather than a fix.
//
// Breaking inheritance is the part that actually does the work: adding two
// explicit ACEs while leaving inheritance on leaves the inherited
// `Users: Read` in place alongside them.
//
// Returns true when the directory ends up with the intended DACL. On failure
// `errors` describes what went wrong; the caller decides how loud to be.
bool protect_directory(const std::string &path, std::list<std::string> &errors);

// Read back the DACL and report whether any ACE grants access to a principal
// other than SYSTEM or Administrators. Used by the caller to verify rather than
// assume - "we set a DACL" and "the DACL excludes everyone else" are different
// claims, and the inheritance trap above is exactly the case where the first is
// true and the second is not.
bool is_protected(const std::string &path, std::list<std::string> &errors);

}  // namespace windows_acl
}  // namespace nsclient

#endif
