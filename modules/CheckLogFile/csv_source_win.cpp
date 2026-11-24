// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// check_csv's local reader on Windows: CreateFileW on the UTF-8 path converted
// to UTF-16, so non-ACP names and UNC paths (\\server\share\file.csv) work.
// Every share flag is passed, so a file the exporting program still holds open
// for writing can be read, and the read never stops that program from
// writing, renaming or deleting it.

#include <windows.h>

#include <error/error.hpp>
#include <str/utf8.hpp>

#include "check_csv.hpp"

namespace check_csv_command {

namespace {
struct handle_closer {
  HANDLE h;
  explicit handle_closer(HANDLE handle) : h(handle) {}
  ~handle_closer() {
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
  }
  handle_closer(const handle_closer &) = delete;
  handle_closer &operator=(const handle_closer &) = delete;
};
}  // namespace

bool read_local_file(const std::string &path, const std::uint64_t max_bytes, std::string &bytes, std::string &error) {
  const std::wstring wide = utf8::cvt<std::wstring>(path);
  const HANDLE h = CreateFileW(wide.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
  if (h == INVALID_HANDLE_VALUE) {
    error = "Failed to open " + path + ": " + error::lookup::last_error();
    return false;
  }
  handle_closer closer(h);

  // Only a file on a disk (local or a share): a named pipe (\\.\pipe\x) or a
  // character device (CON, \\.\COM1) would block ReadFile until its other end
  // writes, which is the check hanging.
  BY_HANDLE_FILE_INFORMATION info;
  if (GetFileType(h) != FILE_TYPE_DISK || (GetFileInformationByHandle(h, &info) && (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)) {
    error = "Not a regular file: " + path;
    return false;
  }
  LARGE_INTEGER size;
  if (!GetFileSizeEx(h, &size)) {
    error = "Failed to read the size of " + path + ": " + error::lookup::last_error();
    return false;
  }
  if (size.QuadPart < 0 || (max_bytes > 0 && static_cast<std::uint64_t>(size.QuadPart) > max_bytes)) {
    error = "CSV is larger than max-size (" + std::to_string(max_bytes) + " bytes): " + path;
    return false;
  }

  bytes.clear();
  bytes.reserve(static_cast<std::size_t>(size.QuadPart));
  char buffer[65536];
  for (;;) {
    DWORD read = 0;
    if (!ReadFile(h, buffer, sizeof(buffer), &read, nullptr)) {
      error = "Failed to read " + path + ": " + error::lookup::last_error();
      return false;
    }
    if (read == 0) break;
    bytes.append(buffer, read);
    // The file may be growing while it is read.
    if (max_bytes > 0 && bytes.size() > max_bytes) {
      error = "CSV is larger than max-size (" + std::to_string(max_bytes) + " bytes): " + path;
      return false;
    }
  }
  return true;
}

}  // namespace check_csv_command
