/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

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
