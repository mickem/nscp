// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <iomanip>
#include <locale>
#include <sstream>
#include <string>

namespace str {

// How the human readable numbers of a check message are rendered (issue #1428).
//
// This describes the *message* and nothing else. Performance data is rendered
// separately (nscapi/protobuf/functions_perfdata.cpp) and has to stay locale
// neutral, and so does every number the filter grammar parses back out of a
// threshold - a decimal comma in either would break the consumers rather than
// please them.
struct number_format {
  // Decimals to render. -1 keeps the historical rendering: up to three
  // decimals with the trailing zeros stripped ("1KB", "70.874GB"). Anything
  // >= 0 renders exactly that many ("25.19GB"), because a stable width is the
  // whole point of asking for two decimals.
  int decimals;
  // Unit to pin byte values to ("GB"); empty lets every value scale on its
  // own, which is why one drive reads "140.293GB/0.983TB" by default.
  std::string byte_unit;
  // Radix character; "," gives the European rendering.
  std::string decimal_separator;
  // Digit grouping for the integer part; empty means no grouping.
  std::string thousands_separator;

  number_format() : decimals(-1), decimal_separator(".") {}

  // True while nothing has been overridden. Callers use it to take the exact
  // legacy code path rather than a reimplementation of it, so an unconfigured
  // check renders byte for byte what it always did.
  bool is_default() const { return decimals == -1 && byte_unit.empty() && decimal_separator == "." && thousands_separator.empty(); }
};

// Render `value` with `decimals` decimals. A negative `decimals` means "up to
// three, trailing zeros stripped" - the rendering every byte value has used
// since forever.
inline std::string render_fixed(const double value, const int decimals) {
  std::ostringstream ss;
  ss.imbue(std::locale::classic());
  ss << std::fixed << std::setprecision(decimals < 0 ? 3 : decimals) << value;
  std::string ret = ss.str();
  if (decimals >= 0) return ret;
  const std::string::size_type pos = ret.find_last_not_of('0');
  if (pos == std::string::npos) return ret;
  if (ret[pos] != '.') return ret.substr(0, pos + 1);
  // Everything after the radix point was a zero: drop the point as well, and
  // keep a leading "0" for a string that is nothing but the fraction.
  return pos == 0 ? std::string("0") : ret.substr(0, pos);
}

// Swap in the configured separators. `plain` is a C-locale number, so the
// radix character is a '.' and there is no grouping to undo.
inline std::string apply_separators(const std::string &plain, const number_format &fmt) {
  if (fmt.decimal_separator == "." && fmt.thousands_separator.empty()) return plain;
  const std::string::size_type dot = plain.find('.');
  std::string int_part = dot == std::string::npos ? plain : plain.substr(0, dot);
  const std::string fraction = dot == std::string::npos ? std::string() : plain.substr(dot + 1);
  if (!fmt.thousands_separator.empty()) {
    const std::string::size_type first_digit = (!int_part.empty() && (int_part[0] == '-' || int_part[0] == '+')) ? 1 : 0;
    for (std::string::size_type i = int_part.size(); i > first_digit + 3;) {
      i -= 3;
      int_part.insert(i, fmt.thousands_separator);
    }
  }
  if (fraction.empty()) return int_part;
  return int_part + (fmt.decimal_separator.empty() ? "." : fmt.decimal_separator) + fraction;
}

inline std::string render_number(const double value, const number_format &fmt) { return apply_separators(render_fixed(value, fmt.decimals), fmt); }

}  // namespace str
