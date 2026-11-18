// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// Line formats of the Checkmk agent sections that native checks emit in their
// `fetch-only` mode. The Lua script (scripts/lua/default_check_mk.lua) only
// adds the `<<<section>>>` header, so the body has to be exactly what
// Checkmk's parser expects - a line it cannot parse is dropped silently, which
// is why the formatting lives here, next to a unit test, rather than in each
// check.

#include <cctype>
#include <string>

namespace check_mk_format {

// Replaces every character in `bad` and every whitespace character with '_'.
inline std::string replace_chars(const std::string &value, const char *bad) {
  std::string out = value;
  for (char &c : out) {
    if (std::isspace(static_cast<unsigned char>(c)) || std::string(bad).find(c) != std::string::npos) c = '_';
  }
  return out;
}

// The user field of a `<<<ps>>>` line. Checkmk reads the process info as one
// whitespace-separated token, `(` ... `)`, and splits it on ',', so a space
// (`NT AUTHORITY\SYSTEM`) or a comma in the name would shift every field
// after it.
inline std::string ps_user(const std::string &user) { return replace_chars(user, ",()"); }

// One `<<<ps>>>` line: `(user,vsz_kb,rss_kb,cputime,pid) command line`.
inline std::string ps_line(const std::string &user, long long vsz_kb, long long rss_kb, long long cputime, long long pid, const std::string &command) {
  return "(" + ps_user(user) + "," + std::to_string(vsz_kb) + "," + std::to_string(rss_kb) + "," + std::to_string(cputime) + "," + std::to_string(pid) + ") " +
         command;
}

// The device and filesystem columns of a `<<<df>>>` line. Checkmk splits the
// line on whitespace and takes the mountpoint as everything after the sixth
// column, so only the mountpoint may contain spaces.
inline std::string df_token(const std::string &value, const std::string &fallback) {
  if (value.empty()) return fallback;
  return replace_chars(value, "");
}

// The capacity column the way `df -P` prints it: used / (used + available),
// rounded up. Checkmk recomputes usage from the size columns and ignores this
// one, but a person reading the dump does not.
inline std::string df_percent(long long used, long long avail) {
  const long long denom = used + avail;
  if (denom <= 0) return "-";
  return std::to_string((used * 100 + denom - 1) / denom) + "%";
}

// One line of the `<<<df>>>` block, in `df -PTk` layout:
// `device fstype total_kb used_kb avail_kb pct% mountpoint`.
inline std::string df_line(const std::string &device, const std::string &fs, long long total_kb, long long used_kb, long long avail_kb,
                           const std::string &mountpoint) {
  return df_token(device, "none") + " " + df_token(fs, "unknown") + " " + std::to_string(total_kb) + " " + std::to_string(used_kb) + " " +
         std::to_string(avail_kb) + " " + df_percent(used_kb, avail_kb) + " " + mountpoint;
}

// One line of the `[df_inodes_start]` ... `[df_inodes_end]` block, in
// `df -PTi` layout: `device fstype inodes iused ifree ipct% mountpoint`.
// Filesystems without a fixed inode table (btrfs, vfat) report 0 inodes; `df`
// prints `-` as the percentage for those and so do we.
inline std::string df_inodes_line(const std::string &device, const std::string &fs, long long total, long long free, const std::string &mountpoint) {
  const long long used = total > free ? total - free : 0;
  return df_token(device, "none") + " " + df_token(fs, "unknown") + " " + std::to_string(total) + " " + std::to_string(used) + " " + std::to_string(free) +
         " " + (total > 0 ? df_percent(used, free) : std::string("-")) + " " + mountpoint;
}

}  // namespace check_mk_format
