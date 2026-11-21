// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// ACL reader for the Unix platforms that have no reader yet (macOS and the BSDs
// keep NFSv4-style ACLs behind acl_get_file(), not in an extended attribute):
// the mode bits are reported and a path is never said to carry an ACL. A
// Darwin reader replaces this file with a check_file_security_darwin.cpp.

#include "check_file_security_unix.hpp"

namespace file_security_posix {

bool read_acl(const std::string & /*path*/, const bool /*default_acl*/, std::vector<unsigned char> &blob, std::string & /*error*/) {
  blob.clear();
  return true;
}

}  // namespace file_security_posix
