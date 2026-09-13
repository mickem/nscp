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
// The interactive prompt's tokenizer. Like parse_command - split on
// unquoted blanks, "..." with backslash escapes - plus single quotes that take
// their content literally, for the paths and expressions that are miserable
// to backslash-escape: 'C:\temp\x y', path='C:\Program Files\app'.
//
// A single quote only opens a string where a whole argument or a whole value
// starts: at the start of an argument, or right after the first '=' in it.
// Anywhere else it is an ordinary character, so filter=core='total' typed
// bare still reaches the check as written (the filter language uses single
// quotes for its own strings). An unterminated quote runs to the end of the
// line. Empty arguments are dropped, as parse_command drops them.
//
// Not a replacement for parse_command: that one also reads alias and script
// definitions from the configuration, where a single quote has always been
// literal and must stay so.
template <class T>
void parse_prompt_command(const std::string &line, T &args) {
  std::string current;
  bool in_token = false;
  std::size_t equals = 0;  // unquoted '=' seen in the current token
  const auto flush = [&]() {
    if (in_token && !current.empty()) args.push_back(current);
    current.clear();
    in_token = false;
    equals = 0;
  };
  for (std::size_t i = 0; i < line.size(); ++i) {
    const char c = line[i];
    if (c == ' ' || c == '\t') {
      flush();
      continue;
    }
    if (c == '"') {
      in_token = true;
      for (++i; i < line.size() && line[i] != '"'; ++i) {
        if (line[i] == '\\' && i + 1 < line.size()) ++i;
        current.push_back(line[i]);
      }
      continue;
    }
    const bool value_start = !in_token || (equals == 1 && !current.empty() && current.back() == '=');
    if (c == '\'' && value_start) {
      in_token = true;
      for (++i; i < line.size() && line[i] != '\''; ++i) current.push_back(line[i]);
      continue;
    }
    in_token = true;
    if (c == '\\' && i + 1 < line.size()) {
      current.push_back(line[++i]);
      continue;
    }
    if (c == '=') ++equals;
    current.push_back(c);
  }
  flush();
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
