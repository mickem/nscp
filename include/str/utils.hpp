// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/tokenizer.hpp>
#include <stdexcept>
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

// parse_command for alias definitions (`my_alias = check_x "arg" ...`), which
// an operator types into a configuration file. parse_command accepts three
// escapes: `\\`, `\"` and `\n`, the last of which it turns into a real newline.
// That is never what an alias means: `"path=C:\nscp"` became `C:` + newline +
// `scp` and the alias registered without a word, while the same path with any
// other letter after the backslash was refused. Here only `\\` and `\"` are
// escapes and every other backslash is refused with a message naming the fix,
// so a typo or an undoubled Windows path fails loudly instead of being
// guessed at. A value that needs a newline (list-separator) takes `\\n`, which
// reaches the check as `\n` for it to decode.
inline void validate_alias_escapes(const std::string &cmd_line) {
  for (std::string::size_type i = 0; i < cmd_line.size(); ++i) {
    if (cmd_line[i] != '\\') continue;
    if (i + 1 >= cmd_line.size()) throw std::invalid_argument("the command line ends with a lone backslash; write a literal backslash as \\\\");
    const char next = cmd_line[i + 1];
    if (next == '\\' || next == '"') {
      ++i;
      continue;
    }
    const std::string escape = std::string("\\") + next;
    if (next == 'n' || next == 'r' || next == 't') {
      throw std::invalid_argument("'" + escape + "' at position " + std::to_string(i) +
                                  " is not allowed in an alias: an alias cannot contain control characters. In a Windows path write the backslash as \\\\ or "
                                  "use /; to pass the two characters on (e.g. list-separator=\\n) write \\\\" +
                                  std::string(1, next));
    }
    throw std::invalid_argument("unknown escape '" + escape + "' at position " + std::to_string(i) +
                                ": only \\\\ and \\\" are escapes in an alias. In a Windows path write the backslash as \\\\ or use /");
  }
}

inline void parse_alias_command(const std::string &cmd_line, std::string &cmd, std::list<std::string> &args) {
  validate_alias_escapes(cmd_line);
  parse_command(cmd_line, cmd, args);
}
}  // namespace utils
}  // namespace str
