// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <cctype>
#include <cstddef>
#include <string>

// Helpers for rejecting attacker-controlled names that would otherwise reach
// the filesystem or the registry. Used by the scripts and modules controllers
// over REST, and by the plugin loader for the name in a [/modules] entry.
//
// The threat model: a request URL like
//   PUT /api/v2/scripts/ext/../../../Windows/System32/evil
//   POST /api/v2/modules/..%2F..%2Fwindows
// must not write outside the intended directory or register a command name
// that can drive a downstream module into traversing out. The same shape
// reaches the loader as `/tmp/evil.so = enabled` in [/modules], where an
// absolute value replaced the module path outright and `..` walked out of it.
//
// `is_safe_module_name` is strict (single segment, alphanum + `._-`). Module
// names map 1-to-1 to a registry id and a single file in module-path. It lives
// here, header-only, rather than beside either caller: it is a security
// predicate, and two hand-rolled copies of one drift - the loader's own copy
// accepted characters this rejects until they were merged.
//
// `is_safe_script_name` permits forward slashes between segments because some
// installers organise scripts in subfolders, but each segment must itself be
// safe and `..` / `.` / empty / drive-letter / leading-separator are rejected.
namespace name_safety {

namespace detail {
constexpr std::size_t kMaxSegmentLen = 128;
constexpr std::size_t kMaxNameLen = 256;

inline bool is_safe_segment_char(char c) {
  const auto u = static_cast<unsigned char>(c);
  if (std::isalnum(u)) return true;
  return c == '.' || c == '_' || c == '-';
}

inline bool is_safe_segment(const std::string& seg) {
  if (seg.empty() || seg.size() > kMaxSegmentLen) return false;
  // "." and ".." are path-traversal primitives even when each character
  // would otherwise be allowed.
  if (seg == "." || seg == "..") return false;
  // A leading '-' makes the name look like an option flag when it is later
  // passed as an argument value (e.g. `--script <name>`); reject it so a name
  // like "-rf" or "-o" cannot be mistaken for a switch by a downstream parser.
  // Interior '-' (e.g. "check-disk") stays allowed.
  if (seg.front() == '-') return false;
  for (char c : seg) {
    if (!is_safe_segment_char(c)) return false;
  }
  return true;
}
}  // namespace detail

inline bool is_safe_module_name(const std::string& name) {
  // Single segment: no separators of any kind.
  if (name.empty() || name.size() > detail::kMaxSegmentLen) return false;
  for (char c : name) {
    if (c == '/' || c == '\\') return false;
  }
  return detail::is_safe_segment(name);
}

inline bool is_safe_script_name(const std::string& name) {
  if (name.empty() || name.size() > detail::kMaxNameLen) return false;
  // No leading separator (would resolve to filesystem root).
  if (name.front() == '/' || name.front() == '\\') return false;
  // No trailing separator (means a missing filename - e.g. "a/" - which the
  // segment loop below would otherwise miss because it exits before
  // validating the empty segment after the separator).
  if (name.back() == '/' || name.back() == '\\') return false;
  // No drive letter on Windows (e.g. "C:foo").
  if (name.size() >= 2 && name[1] == ':') return false;
  // No NULs anywhere.
  for (char c : name) {
    if (c == '\0') return false;
  }
  // Walk segments split on either separator. Each segment must be a safe
  // segment and not `.` / `..` (handled inside is_safe_segment). Empty
  // interior segments (e.g. "a//b") fail is_safe_segment.
  std::size_t i = 0;
  while (i < name.size()) {
    std::size_t j = i;
    while (j < name.size() && name[j] != '/' && name[j] != '\\') {
      ++j;
    }
    const std::string seg = name.substr(i, j - i);
    if (!detail::is_safe_segment(seg)) return false;
    if (j == name.size()) break;
    i = j + 1;
  }
  return true;
}

}  // namespace name_safety
