// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <cctype>
#include <string>

namespace safe_file {

namespace detail {
inline bool iequals(const std::string &a, const char *b) {
  std::size_t i = 0;
  for (; i < a.size() && b[i] != '\0'; ++i) {
    if (std::toupper(static_cast<unsigned char>(a[i])) != std::toupper(static_cast<unsigned char>(b[i]))) return false;
  }
  return i == a.size() && b[i] == '\0';
}

inline std::string element(const std::string &path, std::size_t from) {
  const std::size_t end = path.find('\\', from);
  return path.substr(from, end == std::string::npos ? std::string::npos : end - from);
}

// `\\host\share\...` (the leading `\\` already stripped): a share named pipe
// or mailslot is not a file system, whichever host serves it.
inline bool is_ipc_share(const std::string &rest) {
  const std::size_t sep = rest.find('\\');
  if (sep == std::string::npos) return false;
  const std::string share = element(rest, sep + 1);
  return iequals(share, "pipe") || iequals(share, "mailslot");
}

// The names Win32 maps to a device in any directory (`C:\logs\con.txt` is the
// console on most Windows versions): the part before the first period, with
// trailing spaces dropped, as Win32 drops them.
inline bool is_dos_device_name(const std::string &name) {
  std::string base = name.substr(0, name.find('.'));
  while (!base.empty() && (base.back() == ' ')) base.pop_back();
  static const char *const fixed[] = {"CON", "PRN", "AUX", "NUL", "CONIN$", "CONOUT$", "CLOCK$"};
  for (const char *n : fixed) {
    if (iequals(base, n)) return true;
  }
  if (base.size() == 4 && (base[3] >= '0' && base[3] <= '9') && (iequals(base.substr(0, 3), "COM") || iequals(base.substr(0, 3), "LPT"))) return true;
  return false;
}
}  // namespace detail

// True when a Windows path names something other than a file in a file
// system: a named pipe, a mailslot, a volume or disk opened raw, the console,
// or any other object in the device namespace.
//
// A check that opens what a caller named must refuse these *before* touching
// the path - not after - because the open is itself the damage:
//
//   * `\\.\pipe\x` (or `\\host\pipe\x`) connects to the pipe, and a client
//     that did not ask for less hands the pipe's server an impersonation
//     token for the agent's account (SYSTEM by default).
//   * `\\.\C:` and `\\.\PhysicalDrive0` open the raw volume or disk, which
//     GetFileType reports as FILE_TYPE_DISK - a check running as SYSTEM
//     would read every file on it with no ACL in the way.
//   * `CON`, `COM1`, `\\.\Tape0` and the like are devices, which may block.
//
// Purely lexical and portable, so it is tested on every platform, but only
// meaningful on Windows: on POSIX `\\.\pipe\x` is an odd file name and
// refused_by_name() says no. `/` and `\` are both separators here.
inline bool is_win32_device_path(std::string path) {
  for (char &c : path) {
    if (c == '/') c = '\\';
  }
  if (path.compare(0, 4, "\\??\\") == 0) return true;   // the NT object namespace
  if (path.compare(0, 4, "\\\\.\\") == 0) return true;  // the Win32 device namespace
  if (path.compare(0, 4, "\\\\?\\") == 0) {
    // The long-path prefix: fine in front of a drive path (\\?\C:\x) or a
    // share (\\?\UNC\host\share\x), and the device namespace otherwise
    // (\\?\C: is the volume, \\?\GLOBALROOT\..., \\?\Volume{...}, \\?\pipe\x).
    const std::string rest = path.substr(4);
    if (rest.size() >= 3 && std::isalpha(static_cast<unsigned char>(rest[0])) && rest[1] == ':' && rest[2] == '\\') {
      return detail::is_dos_device_name(rest.substr(rest.find_last_of('\\') + 1));
    }
    if (rest.size() > 4 && detail::iequals(rest.substr(0, 4), "UNC\\")) return detail::is_ipc_share(rest.substr(4));
    return true;
  }
  if (path.compare(0, 2, "\\\\") == 0 && detail::is_ipc_share(path.substr(2))) return true;
  // A bare drive (`C:`) is the current directory on that drive, not a device;
  // only the last element can be a DOS device name that matters.
  const std::size_t last = path.find_last_of('\\');
  std::string name = last == std::string::npos ? path : path.substr(last + 1);
  if (name.size() >= 2 && name[1] == ':' && last == std::string::npos) name = name.substr(2);
  return detail::is_dos_device_name(name);
}

// is_win32_device_path() on Windows, false elsewhere.
inline bool refused_by_name(const std::string &path) {
#ifdef _WIN32
  return is_win32_device_path(path);
#else
  (void)path;
  return false;
#endif
}

}  // namespace safe_file
