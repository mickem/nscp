// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// safe_file on Windows. See safe_file.hpp for what is refused and why.

#include <windows.h>

#include <error/error.hpp>
#include <files/safe_file.hpp>
#include <str/utf8.hpp>

namespace safe_file {

namespace {
HANDLE as_handle(void *h) { return h == nullptr ? INVALID_HANDLE_VALUE : static_cast<HANDLE>(h); }
}  // namespace

void *detail::create_for_read(const std::string &path) {
  // Names reach a check as UTF-8, except that a path_policy in `allowed` mode
  // hands back the path it resolved through boost::filesystem, which spells
  // it in the ANSI code page. Text that is not UTF-8 is read as that.
  const std::wstring wide = utf8::is_valid(path) ? utf8::cvt<std::wstring>(path) : utf8::to_unicode(path);
  // SECURITY_IDENTIFICATION: should the name still reach a named pipe (a
  // symbolic link, a spelling the name check does not know), its server
  // learns who connected and cannot act as that account. Without the flags,
  // the default is SecurityImpersonation. They mean nothing for a file.
  const HANDLE h = CreateFileW(wide.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr);
  return h == INVALID_HANDLE_VALUE ? nullptr : h;
}

file::file(file &&other) noexcept : handle_(other.handle_) { other.handle_ = nullptr; }

file &file::operator=(file &&other) noexcept {
  if (this != &other) {
    close();
    handle_ = other.handle_;
    other.handle_ = nullptr;
  }
  return *this;
}

file::~file() { close(); }

bool file::is_open() const { return handle_ != nullptr; }

void file::close() {
  if (handle_ != nullptr) CloseHandle(static_cast<HANDLE>(handle_));
  handle_ = nullptr;
}

bool file::open(const std::string &path, std::string &error) {
  close();
  const std::string shown = utf8::make_valid(path);
  if (is_win32_device_path(path)) {
    error = "Refusing to open " + shown + ": it names a device or a pipe, not a file";
    return false;
  }
  const HANDLE h = as_handle(detail::create_for_read(path));
  if (h == INVALID_HANDLE_VALUE) {
    const std::string reason = error::lookup::last_error();
    error = "Failed to open file: " + shown + ": " + reason;
    return false;
  }
  handle_ = h;
  // Only a file on a disk (local or on a share). A pipe or a character
  // device would also block ReadFile until its other end writes. A
  // directory cannot be opened without FILE_FLAG_BACKUP_SEMANTICS, but check
  // the attributes anyway: a raw volume reached through a link would report
  // FILE_TYPE_DISK too, and has no file information to give.
  BY_HANDLE_FILE_INFORMATION info;
  if (GetFileType(h) != FILE_TYPE_DISK || !GetFileInformationByHandle(h, &info) || (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
    error = "Not a regular file: " + shown;
    close();
    return false;
  }
  return true;
}

bool file::read(char *buf, const std::size_t len, std::size_t &got) {
  got = 0;
  const DWORD want = len > 0x40000000 ? 0x40000000 : static_cast<DWORD>(len);
  DWORD n = 0;
  if (!ReadFile(as_handle(handle_), buf, want, &n, nullptr)) return false;
  got = n;
  return true;
}

bool file::seek(const std::uint64_t offset) {
  LARGE_INTEGER li;
  li.QuadPart = static_cast<LONGLONG>(offset);
  return SetFilePointerEx(as_handle(handle_), li, nullptr, FILE_BEGIN) != 0;
}

bool file::size(std::uint64_t &out) {
  LARGE_INTEGER li;
  if (!GetFileSizeEx(as_handle(handle_), &li) || li.QuadPart < 0) return false;
  out = static_cast<std::uint64_t>(li.QuadPart);
  return true;
}

}  // namespace safe_file
