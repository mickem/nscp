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

// Fold a path into the one spelling the checks below compare: `/` becomes `\`
// and a run of separators becomes one (two at the very start, where they mean
// a UNC or device prefix). Win32 accepts all of those spellings for the same
// object, so a check which only knew `\\host\pipe\x` would wave
// `\\host\\pipe\x` through.
inline std::string fold_separators(const std::string &path) {
  std::string out;
  out.reserve(path.size());
  for (const char raw : path) {
    const char c = raw == '/' ? '\\' : raw;
    if (c == '\\' && out.size() >= 2 && out.back() == '\\') continue;
    out += c;
  }
  return out;
}

// The server and share of a folded UNC path (`\\host\share\...` or
// `\\?\UNC\host\share\...`), and the length of the prefix in front of the
// server. False for anything else, the device prefixes `\\.\` and `\\?\`
// included.
inline bool split_unc(const std::string &folded, std::size_t &prefix, std::string &host, std::string &share) {
  if (folded.size() > 8 && folded.compare(0, 4, "\\\\?\\") == 0 && iequals(folded.substr(4, 4), "UNC\\")) {
    prefix = 8;
  } else if (folded.compare(0, 2, "\\\\") == 0 && folded.compare(0, 4, "\\\\?\\") != 0 && folded.compare(0, 4, "\\\\.\\") != 0) {
    prefix = 2;
  } else {
    return false;
  }
  host = element(folded, prefix);
  if (host.empty() || host == "." || host == "?") return false;
  const std::size_t after_host = prefix + host.size();
  share = after_host < folded.size() ? element(folded, after_host + 1) : std::string();
  return true;
}

// A share which is not a file system, whichever host serves it: a named pipe
// is `\\host\pipe\x`, and that is only the redirector's alias for a file on
// the IPC$ share, so `\\host\IPC$\x` connects to the same pipe.
inline bool is_ipc_share_name(const std::string &share) { return iequals(share, "pipe") || iequals(share, "mailslot") || iequals(share, "IPC$"); }

// The names Win32 maps to a device in any directory (`C:\logs\con.txt` is the
// console on most Windows versions): the part before the first period or
// colon (`COM1:` and `NUL:` are the devices too), with trailing spaces
// dropped, as Win32 drops them.
inline bool is_dos_device_name(const std::string &name) {
  std::string base = name.substr(0, name.find_first_of(".:"));
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
inline bool is_win32_device_path(const std::string &raw) {
  const std::string path = detail::fold_separators(raw);
  if (path.compare(0, 4, "\\??\\") == 0) return true;   // the NT object namespace
  if (path.compare(0, 4, "\\\\.\\") == 0) return true;  // the Win32 device namespace
  std::size_t prefix = 0;
  std::string host, share;
  const bool unc = detail::split_unc(path, prefix, host, share);
  if (unc && detail::is_ipc_share_name(share)) return true;
  if (!unc && path.compare(0, 4, "\\\\?\\") == 0) {
    // The long-path prefix: fine in front of a drive path (\\?\C:\x) or a
    // share (\\?\UNC\host\share\x, handled above), and the device namespace
    // otherwise (\\?\C: is the volume, \\?\GLOBALROOT\..., \\?\Volume{...},
    // \\?\pipe\x).
    const std::string rest = path.substr(4);
    if (rest.size() >= 3 && std::isalpha(static_cast<unsigned char>(rest[0])) && rest[1] == ':' && rest[2] == '\\') {
      return detail::is_dos_device_name(rest.substr(rest.find_last_of('\\') + 1));
    }
    return true;
  }
  // A bare drive (`C:`) is the current directory on that drive, not a device;
  // only the last element can be a DOS device name that matters.
  const std::size_t last = path.find_last_of('\\');
  std::string name = last == std::string::npos ? path : path.substr(last + 1);
  if (name.size() >= 2 && name[1] == ':' && last == std::string::npos) name = name.substr(2);
  return detail::is_dos_device_name(name);
}

// The server a UNC path names (`\\fs01\logs\app.log`, `//fs01/logs/app.log`,
// `\\?\UNC\fs01\logs\app.log` all say fs01), or false when the path is not
// one. Opening such a path makes the agent authenticate to that server with
// its own account, so which servers a caller may name is a policy decision
// (check::access::path_policy, `remote hosts`). Lexical and portable like
// is_win32_device_path(), and like it only meaningful on Windows.
inline bool unc_host(const std::string &path, std::string &host) {
  std::size_t prefix = 0;
  std::string share;
  return detail::split_unc(detail::fold_separators(path), prefix, host, share);
}

// The root of the share a UNC path is on, with its trailing separator
// (`\\fs01\logs\`), or false when the path is not a UNC path with a share.
// The root has an empty name on the share, so opening it asks the server what
// the share is without opening anything a caller named on it.
inline bool unc_share_root(const std::string &path, std::string &root) {
  const std::string folded = detail::fold_separators(path);
  std::size_t prefix = 0;
  std::string host, share;
  if (!detail::split_unc(folded, prefix, host, share) || share.empty()) return false;
  root = folded.substr(0, prefix) + host + "\\" + share + "\\";
  return true;
}

// A folder and a file mask as one search string: `C:\logs` and `*.log` give
// `C:\logs\*.log`. A folder which already ends in a separator gets no second
// one - `\` (the root of the current drive) and `evil\share\*` must give
// `\evil\share\*`, a folder on that drive, and not `\\evil\share\*`, which
// is a share on a server called evil.
inline std::string join_mask(const std::string &root, const std::string &mask) {
  if (!root.empty() && (root.back() == '\\' || root.back() == '/')) return root + mask;
  return root + "\\" + mask;
}

// True when join_mask(root, mask) names somewhere `root` alone does not:
// another server or share, or a device.
//
// A directory scan on Windows hands FindFirstFile the folder with the file
// mask appended. A "mask" which carries separators is a path, and the join
// can then land outside everything the root was checked for: the root `\`
// and the mask `\evil\share\*` join to `\\evil\share\*`, a server nobody held
// against `remote hosts`, and `\` with `\.\pipe\x` joins to the pipe
// namespace. The root is what the access policy saw, so the joined string
// has to still be on it.
inline bool mask_leaves_root(const std::string &root, const std::string &mask) {
  const std::string joined = join_mask(root, mask);
  if (is_win32_device_path(joined) && !is_win32_device_path(root)) return true;
  std::string root_share, joined_share, host;
  const bool root_on_share = unc_share_root(root, root_share);
  if (unc_share_root(joined, joined_share)) {
    if (!root_on_share) return true;
    if (root_share.size() != joined_share.size()) return true;
    for (std::size_t i = 0; i < root_share.size(); ++i) {
      if (std::toupper(static_cast<unsigned char>(root_share[i])) != std::toupper(static_cast<unsigned char>(joined_share[i]))) return true;
    }
    return false;
  }
  // No share in the joined string: it may still have gained a server.
  return unc_host(joined, host) && !unc_host(root, host);
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
