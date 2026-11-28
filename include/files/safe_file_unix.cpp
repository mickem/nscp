// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// safe_file on POSIX. The file is opened non-blocking - opening a FIFO with no
// writer would otherwise block the check forever - and the open descriptor is
// fstat'ed, so a path swapped for a link to a device between a check and the
// open is still refused. Anything but a regular file is refused.

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <files/safe_file.hpp>

namespace safe_file {

file::file(file &&other) noexcept : fd_(other.fd_), read_error_(other.read_error_) { other.fd_ = -1; }

file &file::operator=(file &&other) noexcept {
  if (this != &other) {
    close();
    fd_ = other.fd_;
    read_error_ = other.read_error_;
    other.fd_ = -1;
  }
  return *this;
}

file::~file() { close(); }

bool file::is_open() const { return fd_ >= 0; }

void file::close() {
  if (fd_ >= 0) ::close(fd_);
  fd_ = -1;
}

bool file::open(const std::string &path, std::string &error) {
  close();
  const int fd = ::open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC | O_NOCTTY);
  if (fd < 0) {
    error = "Failed to open file: " + path + ": " + std::strerror(errno);
    return false;
  }
  fd_ = fd;
  struct stat st{};
  if (::fstat(fd, &st) != 0) {
    error = "Failed to open file: " + path + ": " + std::strerror(errno);
    close();
    return false;
  }
  if (!S_ISREG(st.st_mode)) {
    error = "Not a regular file: " + path;
    close();
    return false;
  }
  // A regular file never blocks, but clear the flag anyway so nothing that
  // reads through this descriptor has to know it was ever set.
  const int flags = ::fcntl(fd, F_GETFL);
  if (flags >= 0) ::fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
  return true;
}

bool file::read(char *buf, const std::size_t len, std::size_t &got) {
  got = 0;
  for (;;) {
    const ssize_t n = ::read(fd_, buf, len);
    if (n < 0 && errno == EINTR) continue;
    if (n < 0) {
      read_error_ = static_cast<unsigned long>(errno);
      return false;
    }
    got = static_cast<std::size_t>(n);
    return true;
  }
}

std::string file::read_error() const { return std::strerror(static_cast<int>(read_error_)); }

bool file::seek(const std::uint64_t offset) { return ::lseek(fd_, static_cast<off_t>(offset), SEEK_SET) != static_cast<off_t>(-1); }

bool file::size(std::uint64_t &out) {
  struct stat st{};
  if (::fstat(fd_, &st) != 0 || st.st_size < 0) return false;
  out = static_cast<std::uint64_t>(st.st_size);
  return true;
}

}  // namespace safe_file
