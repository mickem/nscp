// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// check_logfile's reader on Windows. See log_file.hpp for why this is not a
// std::ifstream.

#include <windows.h>

#include <error/error.hpp>
#include <str/utf8.hpp>

#include "device_streambuf.hpp"
#include "log_file.hpp"

namespace check_logfile {

namespace {

class handle_device {
 public:
  explicit handle_device(HANDLE h) : h_(h) {}
  handle_device(handle_device &&other) noexcept : h_(other.h_) { other.h_ = INVALID_HANDLE_VALUE; }
  handle_device &operator=(handle_device &&) = delete;
  handle_device(const handle_device &) = delete;
  handle_device &operator=(const handle_device &) = delete;
  ~handle_device() {
    if (h_ != INVALID_HANDLE_VALUE) CloseHandle(h_);
  }

  bool read(char *buf, std::size_t len, std::size_t &got) {
    const DWORD want = len > 0x40000000 ? 0x40000000 : static_cast<DWORD>(len);
    DWORD read = 0;
    if (!ReadFile(h_, buf, want, &read, nullptr)) return false;
    got = read;
    return true;
  }
  bool seek(std::uint64_t offset) {
    LARGE_INTEGER li;
    li.QuadPart = static_cast<LONGLONG>(offset);
    return SetFilePointerEx(h_, li, nullptr, FILE_BEGIN) != 0;
  }
  bool size(std::uint64_t &out) {
    LARGE_INTEGER li;
    if (!GetFileSizeEx(h_, &li) || li.QuadPart < 0) return false;
    out = static_cast<std::uint64_t>(li.QuadPart);
    return true;
  }

 private:
  HANDLE h_;
};

}  // namespace

void *detail::open_for_read(const std::string &path) {
  // Names reach the check as UTF-8, except that [/settings/logfile]
  // `file access = allowed` hands back the path it resolved through
  // boost::filesystem, which on Windows spells it in the ANSI code page -
  // the encoding std::ifstream took. Text that is not UTF-8 is read as that.
  const std::wstring wide = utf8::is_valid(path) ? utf8::cvt<std::wstring>(path) : utf8::to_unicode(path);
  // A path can name a named pipe (\\.\pipe\x, \\host\pipe\x), and opening one
  // connects to it before GetFileType below gets to refuse the handle. With no
  // quality-of-service flags that connection lets the pipe's server
  // impersonate the agent's account (SYSTEM by default). SECURITY_IDENTIFICATION
  // lets it learn who connected and nothing more; the flags mean nothing for a
  // file.
  return CreateFileW(wide.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                     FILE_ATTRIBUTE_NORMAL | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr);
}

std::unique_ptr<std::streambuf> open_log_file(const std::string &path, std::string &error) {
  HANDLE h = static_cast<HANDLE>(detail::open_for_read(path));
  if (h == INVALID_HANDLE_VALUE) {
    const std::string reason = error::lookup::last_error();
    error = "Failed to open file: " + utf8::make_valid(path) + ": " + reason;
    return nullptr;
  }
  handle_device device(h);
  // Only a file on a disk (local or a share): a named pipe or a character
  // device (CON, \\.\COM1) would block ReadFile until its other end writes.
  // A directory cannot be opened without FILE_FLAG_BACKUP_SEMANTICS, so it
  // never gets this far.
  if (GetFileType(h) != FILE_TYPE_DISK) {
    error = "Failed to open file: " + utf8::make_valid(path) + ": not a regular file";
    return nullptr;
  }
  return std::unique_ptr<std::streambuf>(new device_streambuf<handle_device>(std::move(device)));
}

}  // namespace check_logfile
