// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// The line and field handling shared by the readers of the colon-separated
// account databases (/etc/passwd, /etc/shadow, /etc/group). It follows what
// glibc's files backend does with a line, because a line the system honours and
// this code drops is an account the checks cannot see:
//  - leading blanks are skipped, then a blank line or a '#' comment is ignored;
//  - a name starting with '+' or '-' is a NIS compat entry that refers to
//    another source and is never returned for a lookup, so it is ignored too;
//  - trailing fields may be missing (how many is up to each reader).
// Anything a reader cannot make sense of is reported by line number, never
// dropped silently and never quoted: a shadow line holds a password hash.

#include <limits>
#include <sstream>
#include <str/xtos.hpp>
#include <string>
#include <vector>

namespace account_files {

inline std::vector<std::string> split_fields(const std::string &line, const char separator = ':') {
  std::vector<std::string> out;
  std::string::size_type start = 0;
  for (;;) {
    const std::string::size_type end = line.find(separator, start);
    if (end == std::string::npos) {
      out.push_back(line.substr(start));
      return out;
    }
    out.push_back(line.substr(start, end - start));
    start = end + 1;
  }
}

// Call f(line, line_number) for every entry line, numbered from 1 as an editor
// shows them.
template <typename F>
void for_each_entry(const std::string &content, F f) {
  std::istringstream in(content);
  std::string line;
  std::size_t number = 0;
  while (std::getline(in, line)) {
    ++number;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    const std::string::size_type first = line.find_first_not_of(" \t\v\f");
    if (first == std::string::npos) continue;
    line.erase(0, first);
    if (line[0] == '#' || line[0] == '+' || line[0] == '-') continue;
    f(line, number);
  }
}

// A non-negative decimal number, or -1 for an empty field. False for anything
// else, so that a corrupt line is reported rather than half-read. Stricter than
// glibc, whose strtoul() also takes blanks and a sign: such a line is reported
// as unparseable, which is the safe side of the difference.
inline bool parse_number(const std::string &field, long long &value) {
  if (field.empty()) {
    value = -1;
    return true;
  }
  unsigned long long parsed = 0;
  if (!str::parse_unsigned(field, parsed)) return false;
  if (parsed > static_cast<unsigned long long>((std::numeric_limits<long long>::max)())) return false;
  value = static_cast<long long>(parsed);
  return true;
}

// "2 unparseable line(s) in /etc/passwd (line 3, 9)". Empty when there are none.
inline std::string unparsed_lines_error(const std::string &file, const std::vector<std::size_t> &lines) {
  if (lines.empty()) return "";
  std::string where;
  const std::size_t shown = lines.size() < 5 ? lines.size() : 5;
  for (std::size_t i = 0; i < shown; ++i) where += (i == 0 ? "" : ", ") + std::to_string(lines[i]);
  if (shown < lines.size()) where += ", ...";
  return std::to_string(lines.size()) + " unparseable line(s) in " + file + " (line " + where + ")";
}

}  // namespace account_files
