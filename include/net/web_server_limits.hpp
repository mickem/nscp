// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <cctype>
#include <cstddef>
#include <limits>
#include <string>

namespace net {

// The stock `max request size` of the WEB server: a byte count with an
// optional k/m/g unit, the way `max size` is written elsewhere in the
// settings. 3 MiB is what the mongoose backend has always buffered per
// request, so an agent that never set the key accepts what it used to on
// either backend.
constexpr const char *kDefaultWebMaxRequestSize = "3MiB";

// Parse a `max request size` value into bytes. Accepts a plain byte count or
// one followed by a unit - `b`, `k`/`kb`/`kib`, `m`/`mb`/`mib`, `g`/`gb`/`gib`,
// case-insensitive, blanks around either allowed. Zero, a negative, a unit
// this does not know and anything that does not fit the platform's size_t are
// refused with `error` filled in, so the caller can say what was wrong and
// keep the previous value rather than guess.
inline bool parse_request_size(const std::string &value, std::size_t &bytes, std::string &error) {
  const auto is_blank = [](const char c) { return c == ' ' || c == '\t'; };
  std::size_t begin = 0;
  std::size_t end = value.size();
  while (begin < end && is_blank(value[begin])) ++begin;
  while (end > begin && is_blank(value[end - 1])) --end;
  if (begin == end) {
    error = "no value given";
    return false;
  }
  std::size_t pos = begin;
  unsigned long long number = 0;
  bool any_digit = false;
  for (; pos < end && std::isdigit(static_cast<unsigned char>(value[pos])); ++pos) {
    const unsigned long long digit = static_cast<unsigned long long>(value[pos] - '0');
    if (number > ((std::numeric_limits<unsigned long long>::max)() - digit) / 10) {
      error = "the number is too large";
      return false;
    }
    number = number * 10 + digit;
    any_digit = true;
  }
  if (!any_digit) {
    error = "expected a number of bytes, optionally followed by k, m or g";
    return false;
  }
  while (pos < end && is_blank(value[pos])) ++pos;
  std::string unit;
  for (; pos < end; ++pos) unit.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(value[pos]))));
  unsigned long long multiplier = 1;
  if (unit.empty() || unit == "b") {
    multiplier = 1;
  } else if (unit == "k" || unit == "kb" || unit == "kib") {
    multiplier = 1024ULL;
  } else if (unit == "m" || unit == "mb" || unit == "mib") {
    multiplier = 1024ULL * 1024ULL;
  } else if (unit == "g" || unit == "gb" || unit == "gib") {
    multiplier = 1024ULL * 1024ULL * 1024ULL;
  } else {
    error = "unknown unit '" + unit + "' (expected k, m or g)";
    return false;
  }
  if (number == 0) {
    error = "must be more than zero bytes";
    return false;
  }
  const unsigned long long limit = static_cast<unsigned long long>((std::numeric_limits<std::size_t>::max)());
  if (number > limit / multiplier) {
    error = "larger than this platform can address";
    return false;
  }
  bytes = static_cast<std::size_t>(number * multiplier);
  return true;
}

}  // namespace net
