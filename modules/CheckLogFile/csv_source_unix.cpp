// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// check_csv's local reader on POSIX. Only a regular file is read, and never
// more than max_bytes of it.
//
// stat() alone cannot bound the read: a character device (/dev/zero,
// /dev/urandom) and a FIFO both report a size of 0, and a file in /proc does
// too while holding content. So the file is opened first - non-blocking, or
// opening a FIFO with no writer would block the check forever - then the open
// descriptor is examined (a path examined separately could be swapped for a
// link to a device in between), and the limit is enforced on every chunk read.

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

#include "check_csv.hpp"

namespace check_csv_command {

namespace {

struct fd_closer {
  int fd;
  explicit fd_closer(const int fd) : fd(fd) {}
  ~fd_closer() { ::close(fd); }
  fd_closer(const fd_closer &) = delete;
  fd_closer &operator=(const fd_closer &) = delete;
};

bool too_large(const std::string &path, const std::uint64_t max_bytes, std::string &error) {
  error = "CSV is larger than max-size (" + std::to_string(max_bytes) + " bytes): " + path;
  return false;
}

bool failed(const std::string &what, const std::string &path, const int err, std::string &error) {
  error = what + " " + path + ": " + std::strerror(err);
  return false;
}

}  // namespace

bool read_local_file(const std::string &path, const std::uint64_t max_bytes, std::string &bytes, std::string &error) {
  const int fd = ::open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC | O_NOCTTY);
  if (fd < 0) return failed("Failed to open", path, errno, error);
  const fd_closer closer(fd);

  struct stat st{};
  if (::fstat(fd, &st) != 0) return failed("Failed to read the size of", path, errno, error);
  if (!S_ISREG(st.st_mode)) {
    error = "Not a regular file: " + path;
    return false;
  }
  if (max_bytes > 0 && static_cast<std::uint64_t>(st.st_size) > max_bytes) return too_large(path, max_bytes, error);

  bytes.clear();
  bytes.reserve(static_cast<std::size_t>(st.st_size));
  char buffer[65536];
  for (;;) {
    const ssize_t got = ::read(fd, buffer, sizeof(buffer));
    if (got == 0) return true;
    if (got < 0 && errno == EINTR) continue;
    if (got < 0) return failed("Failed to read", path, errno, error);
    bytes.append(buffer, static_cast<std::size_t>(got));
    // A file that grew after the fstat, or one that reports no size at all.
    if (max_bytes > 0 && bytes.size() > max_bytes) return too_large(path, max_bytes, error);
  }
}

}  // namespace check_csv_command
