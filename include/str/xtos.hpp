// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/lexical_cast.hpp>
#include <limits>
#include <sstream>
#include <string>

namespace str {

template <class T>
T stox(const std::string s) {
  return boost::lexical_cast<T>(s.c_str());
}
template <class T>
T stox(const std::string s, T def) {
  try {
    return boost::lexical_cast<T>(s.c_str());
  } catch (...) {
    return def;
  }
}

// Parse a non-negative decimal integer made of digits only: no sign, no
// space, no separators. False - leaving `out` as it was - for an empty
// string, any other character, or a value that does not fit. Reads the
// digits itself, so it neither throws nor follows the locale: for text that
// comes from outside, such as a query parameter or a /proc field.
inline bool parse_unsigned(const std::string &text, unsigned long long &out) {
  if (text.empty()) return false;
  const unsigned long long limit = (std::numeric_limits<unsigned long long>::max)();
  unsigned long long value = 0;
  for (const char c : text) {
    if (c < '0' || c > '9') return false;
    const unsigned long long digit = static_cast<unsigned long long>(c - '0');
    if (value > (limit - digit) / 10) return false;
    value = value * 10 + digit;
  }
  out = value;
  return true;
}

template <typename T>
std::string xtos(T i) {
  std::stringstream ss;
  ss << i;
  return ss.str();
}

inline std::string ihextos(unsigned int i) {
  std::stringstream ss;
  ss << std::hex << i;
  return ss.str();
}

template <typename T>
std::string xtos_non_sci(T i) {
  std::stringstream ss;
  if (i < 10) ss.precision(20);
  ss << std::noshowpoint << std::fixed << i;
  std::string s = ss.str();
  const auto pos = s.find('.');
  // 1234456 => 1234456
  if (pos == std::string::npos) return s;
  // 12340.0000001234 => 12340.000000
  if (s.length() - pos > 6) s = s.substr(0, pos + 6);

  const auto dot_pos = s.find_last_of('.');
  // 12345600 -> 12345600
  if (dot_pos == std::string::npos) return s;
  const auto zero_pos = s.find_last_not_of('0');
  // 1234.5600 -> 1234.56
  if (zero_pos > dot_pos) return s.substr(0, zero_pos + 1);
  // 123.0000 -> 123
  return s.substr(0, zero_pos);
}
}  // namespace str