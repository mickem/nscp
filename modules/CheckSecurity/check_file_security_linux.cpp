// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Linux reader for the POSIX ACL of a path. The kernel keeps it in two extended
// attributes, which is all libacl reads as well - going to the attribute
// directly keeps the module free of a libacl dependency.

#include <sys/xattr.h>

#include <cerrno>
#include <cstring>

#include "check_file_security_unix.hpp"

namespace file_security_posix {

bool read_acl(const std::string &path, const bool default_acl, std::vector<unsigned char> &blob, std::string &error) {
  const char *name = default_acl ? "system.posix_acl_default" : "system.posix_acl_access";
  blob.clear();
  // The value can change size between the sizing call and the read; a couple
  // of attempts is plenty for an attribute nobody rewrites in a loop.
  for (int attempt = 0; attempt < 3; ++attempt) {
    const ssize_t needed = ::getxattr(path.c_str(), name, nullptr, 0);
    if (needed == 0) return true;
    if (needed > 0) {
      blob.resize(static_cast<std::size_t>(needed));
      const ssize_t got = ::getxattr(path.c_str(), name, blob.data(), blob.size());
      if (got >= 0) {
        blob.resize(static_cast<std::size_t>(got));
        return true;
      }
      if (errno == ERANGE) continue;
    }
    const int code = errno;
    blob.clear();
    // No ACL on the path, or a file system that has no such thing (procfs,
    // vfat, an old NFS): the mode bits are then the whole story.
    if (code == ENODATA || code == ENOTSUP || code == EOPNOTSUPP || code == ENOSYS) return true;
    error = std::string("Failed to read the POSIX ACL: ") + std::strerror(code);
    return false;
  }
  error = "Failed to read the POSIX ACL: it kept changing";
  return false;
}

}  // namespace file_security_posix
