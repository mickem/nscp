// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/tokenizer.hpp>
#include <str/utils_no_boost.hpp>

namespace str {
namespace utils {

//
// Split
//
template <class T>
std::string joinEx(const T &lst, const std::string key) {
  std::string ret;
  for (const std::string &s : lst) {
    if (!ret.empty()) ret += key;
    ret += s;
  }
  return ret;
}

//
// Parsing commands
//
template <class T>
void parse_command(const std::string &cmd_line, T &args) {
  const boost::tokenizer<boost::escaped_list_separator<char>> tok(cmd_line, boost::escaped_list_separator<char>('\\', ' ', '\"'));
  for (std::string s : tok) {
    if (!s.empty()) args.push_back(s);
  }
}
inline std::list<std::string> parse_command(const std::string &cmd_line) {
  std::list<std::string> args;
  const boost::tokenizer<boost::escaped_list_separator<char>> tok(cmd_line, boost::escaped_list_separator<char>('\\', ' ', '\"'));
  for (const std::string &s : tok) {
    if (!s.empty()) args.push_back(s);
  }
  return args;
}
inline void parse_command(const std::string &cmd_line, std::string &cmd, std::list<std::string> &args) {
  const boost::tokenizer<boost::escaped_list_separator<char>> tok(cmd_line, boost::escaped_list_separator<char>('\\', ' ', '\"'));
  bool first = true;
  for (const std::string &s : tok) {
    if (first) {
      cmd = s;
      first = false;
    } else {
      if (!s.empty()) {
        args.push_back(s);
      }
    }
  }
}
}  // namespace utils
}  // namespace str
