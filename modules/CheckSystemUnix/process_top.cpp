// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "process_top.h"

#include <cstdlib>
#include <sstream>

namespace process_top {

namespace {
// strtoull, which reads digits whatever the locale says a number looks like.
bool to_ull(const std::string &text, unsigned long long &out) {
  if (text.empty()) return false;
  for (const char c : text) {
    if (c < '0' || c > '9') return false;
  }
  out = std::strtoull(text.c_str(), nullptr, 10);
  return true;
}

std::vector<std::string> split(const std::string &text) {
  std::vector<std::string> out;
  std::string token;
  for (const char c : text) {
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
      if (!token.empty()) out.push_back(token);
      token.clear();
    } else {
      token += c;
    }
  }
  if (!token.empty()) out.push_back(token);
  return out;
}
}  // namespace

bool parse_stat(const std::string &line, process_times &out) {
  const std::string::size_type open = line.find('(');
  const std::string::size_type close = line.rfind(')');
  if (open == std::string::npos || close == std::string::npos || close < open) return false;
  // Field 3 (the state) is the first token after the name.
  const std::vector<std::string> fields = split(line.substr(close + 1));
  // utime is field 14, stime 15 and starttime 22: tokens 11, 12 and 19.
  if (fields.size() < 20) return false;
  unsigned long long utime = 0, stime = 0, start = 0;
  if (!to_ull(fields[11], utime) || !to_ull(fields[12], stime) || !to_ull(fields[19], start)) return false;
  out.name = line.substr(open + 1, close - open - 1);
  out.ticks = utime + stime;
  out.start = start;
  return true;
}

bool parse_statm(const std::string &line, const unsigned long long page_size, process_times &out) {
  const std::vector<std::string> fields = split(line);
  if (fields.size() < 3) return false;
  unsigned long long size = 0, resident = 0, shared = 0;
  if (!to_ull(fields[0], size) || !to_ull(fields[1], resident) || !to_ull(fields[2], shared)) return false;
  out.working_set = resident * page_size;
  out.private_bytes = (resident > shared ? resident - shared : 0) * page_size;
  return true;
}

std::vector<process_tables::process_row> rows(const table &prev, const table &cur, const unsigned long long total_ticks) {
  std::vector<process_tables::process_row> out;
  out.reserve(cur.size());
  for (const table::value_type &p : cur) {
    process_tables::process_row row;
    row.name = p.second.name;
    row.pid = p.first;
    row.working_set = p.second.working_set;
    row.private_bytes = p.second.private_bytes;
    const table::const_iterator before = prev.find(p.first);
    if (total_ticks > 0 && before != prev.end() && before->second.start == p.second.start && p.second.ticks >= before->second.ticks) {
      row.cpu = 100.0 * static_cast<double>(p.second.ticks - before->second.ticks) / static_cast<double>(total_ticks);
    }
    out.push_back(row);
  }
  return out;
}

}  // namespace process_top
