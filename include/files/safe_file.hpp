// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <cstdint>
#include <files/device_path.hpp>
#include <files/device_streambuf.hpp>
#include <memory>
#include <streambuf>
#include <string>

// The way a check reads a file whose name it did not choose.
//
// A path a caller passes (`check_logfile file=`, `check_csv file=`, a
// `check_files` checksum) is opened with the agent's privileges, so the open
// has to be as careful as the access gate in front of it:
//
//   * Only a regular file is read. A FIFO, a character device (/dev/zero),
//     a named pipe, the console or a raw volume is refused - opened first,
//     non-blocking where the OS has that, and the *open handle* examined, so a
//     path swapped for a link to a device in between is still caught.
//   * On Windows the open asks for SECURITY_IDENTIFICATION, so even a pipe the
//     name check below does not recognise cannot impersonate the agent, and
//     names in the device namespace (`\\.\...`, `\\host\pipe\...`, `CON`) are
//     refused before anything is opened (see device_path.hpp).
//   * Every share flag is passed on Windows, so a log the writing program
//     holds open is read without getting in its way, and the UTF-8 name is
//     converted to UTF-16, so names outside the ANSI code page work.
//   * read_all() enforces its byte limit while reading, not from the size the
//     file reports: /proc files report 0, and a file can grow.
//
// It does not decide *which* files may be read: that is the module's
// check::access::path_policy, whose resolve() runs first and whose result is
// what gets opened. resolve() refuses device paths too, before it stats them.
//
// Built from safe_file_win.cpp or safe_file_unix.cpp (${NSCP_SAFE_FILE_CPP}).

namespace safe_file {

class file {
 public:
  file() = default;
  file(file &&other) noexcept;
  file &operator=(file &&other) noexcept;
  file(const file &) = delete;
  file &operator=(const file &) = delete;
  ~file();

  // Open `path` (UTF-8) for reading. Returns false with `error` set to a
  // message naming the path:
  //   "Failed to open file: <path>: <reason>"
  //   "Not a regular file: <path>"
  //   "Refusing to open <path>: it names a device or a pipe, not a file"
  bool open(const std::string &path, std::string &error);
  bool is_open() const;
  void close();

  // The device interface device_streambuf reads through.
  bool read(char *buf, std::size_t len, std::size_t &got);
  bool seek(std::uint64_t offset);
  bool size(std::uint64_t &out);

 private:
#ifdef _WIN32
  void *handle_ = nullptr;  // a HANDLE; nullptr when closed
#else
  int fd_ = -1;
#endif
};

#ifdef _WIN32
namespace detail {
// The CreateFileW call file::open() makes once the name has passed
// is_win32_device_path(), before the handle is examined: a HANDLE as void*,
// nullptr on failure, which the caller closes. Exposed so the unit test can
// connect to a pipe through it and read the impersonation level the pipe's
// server is given.
void *create_for_read(const std::string &path);
}  // namespace detail
#endif

// open() wrapped in a seekable std::streambuf; nullptr (and `error`) on
// failure. Wrap the result in a std::istream.
inline std::unique_ptr<std::streambuf> open_stream(const std::string &path, std::string &error) {
  file f;
  if (!f.open(path, error)) return nullptr;
  return std::unique_ptr<std::streambuf>(new device_streambuf<file>(std::move(f)));
}

enum class read_status { ok, failed, too_large };

// Read the whole of `path` into `out`, but never more than `max_bytes` of it
// (0 = no limit). `too_large` leaves `error` empty: the caller knows which
// setting to name in the message.
inline read_status read_all(const std::string &path, const std::uint64_t max_bytes, std::string &out, std::string &error) {
  out.clear();
  error.clear();
  file f;
  if (!f.open(path, error)) return read_status::failed;
  std::uint64_t size = 0;
  if (f.size(size)) {
    if (max_bytes > 0 && size > max_bytes) return read_status::too_large;
    out.reserve(static_cast<std::size_t>(size));
  }
  char buffer[65536];
  for (;;) {
    std::size_t got = 0;
    if (!f.read(buffer, sizeof(buffer), got)) {
      error = "Failed to read " + path;
      return read_status::failed;
    }
    if (got == 0) return read_status::ok;
    out.append(buffer, got);
    if (max_bytes > 0 && out.size() > max_bytes) return read_status::too_large;
  }
}

}  // namespace safe_file
