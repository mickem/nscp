// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <list>
#include <string>

namespace str {
namespace utils {
//
// Replace
//
inline void replace(std::string& string, const std::string& replace, const std::string& with) {
  auto pos = string.find(replace);
  const auto len = replace.length();
  while (pos != std::string::npos) {
    string = string.substr(0, pos) + with + string.substr(pos + len);
    if (with.find(replace) != std::string::npos)  // If the replace containes the key look after the replace!
      pos = string.find(replace, pos + with.length());
    else
      pos = string.find(replace, pos + 1);
  }
}

//
// Unescape
//
/**
 * Decode the C-style escapes an operator can actually type into a
 * configuration file or a check argument: `\n`, `\r`, `\t` and `\\`.
 *
 * A value in nsclient.ini is a single line and check arguments travel over
 * REST/NRPE as plain tokens, so there is no way to write a real newline or tab
 * in either. Options whose value IS a piece of literal output (list-separator)
 * run through this so `\n` means what the operator meant. Message templates
 * (top-syntax, detail-syntax, ...) are deliberately NOT decoded: existing
 * configurations contain literal backslashes (C:\temp, regexes) that decoding
 * would silently corrupt - they reference the separator as %(sep) instead.
 *
 * Scanning left to right and consuming both characters of an escape means a
 * literal backslash written as `\\` is never re-examined, so `\\n` decodes to
 * the two characters `\` and `n`, not to a newline. Anything else keeps its
 * backslash: an unknown escape is far more likely a Windows path than a typo.
 */
inline std::string unescape(const std::string& str) {
  std::string ret;
  ret.reserve(str.size());
  for (std::string::size_type i = 0; i < str.size(); ++i) {
    if (str[i] != '\\' || i + 1 >= str.size()) {
      ret += str[i];
      continue;
    }
    switch (str[i + 1]) {
      case 'n':
        ret += '\n';
        break;
      case 'r':
        ret += '\r';
        break;
      case 't':
        ret += '\t';
        break;
      case '\\':
        ret += '\\';
        break;
      default:
        ret += str[i];
        ret += str[i + 1];
        break;
    }
    ++i;
  }
  return ret;
}

//
// Split
//
template <class T>
void split(T& ret, const std::string& str, const std::string& key) {
  std::string::size_type pos = 0, lpos = 0;
  while ((pos = str.find(key, pos)) != std::string::npos) {
    ret.push_back(str.substr(lpos, pos - lpos));
    lpos = ++pos;
  }
  if (lpos < str.size()) ret.push_back(str.substr(lpos));
}
typedef std::pair<std::string, std::string> token;
inline token split2(const std::string& str, const std::string& key) {
  std::string::size_type pos = str.find(key);
  if (pos == std::string::npos) return {str, std::string()};
  return {str.substr(0, pos), str.substr(pos + key.length())};
}
inline std::list<std::string> split_lst(const std::string& str, const std::string& key) {
  std::list<std::string> ret;
  std::string::size_type pos = 0, lpos = 0;
  while ((pos = str.find(key, pos)) != std::string::npos) {
    ret.push_back(str.substr(lpos, pos - lpos));
    lpos = ++pos;
  }
  if (lpos < str.size()) ret.push_back(str.substr(lpos));
  return ret;
}
template <class T>
T split(const std::string& str, const std::string& key) {
  T ret;
  std::string::size_type pos = 0, lpos = 0;
  while ((pos = str.find(key, pos)) != std::string::npos) {
    ret.push_back(str.substr(lpos, pos - lpos));
    lpos = ++pos;
  }
  if (lpos < str.size()) ret.push_back(str.substr(lpos));
  return ret;
}

//
// Tokenizer
//
inline token getToken(const std::string& buffer, const char split) {
  const auto pos = buffer.find(split);
  if (pos == std::string::npos) return {buffer, ""};
  if (pos == buffer.length() - 1) return {buffer.substr(0, pos), ""};
  return {buffer.substr(0, pos), buffer.substr(pos + 1)};
}

}  // namespace utils
}  // namespace str
