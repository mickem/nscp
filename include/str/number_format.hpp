// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <str/saturate.hpp>
#include <string>

namespace str {

// The largest number of decimals that is ever meaningful: a double carries at
// most ~17 significant digits, so anything past this is noise. It also bounds
// the width of a rendered number - std::setprecision(N) makes the stream build
// an N-digit fraction, so an unbounded N (a config typo, or a hostile REST
// argument) would try to allocate a huge string and crash the check. Callers
// that take a decimals value from outside reject anything larger; render_fixed
// clamps to it as a last-resort backstop so no path can trigger that.
constexpr int max_decimals = 15;

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
  // in [0, max_decimals] renders exactly that many ("25.19GB"), because a
  // stable width is the whole point of asking for two decimals.
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
  // Clamp as a backstop: the option and function-argument parsers already
  // reject anything above max_decimals, but a settings key or an internal
  // caller must never be able to hand setprecision an unbounded width.
  const int precision = decimals < 0 ? 3 : (decimals > max_decimals ? max_decimals : decimals);
  std::ostringstream ss;
  ss.imbue(std::locale::classic());
  ss << std::fixed << std::setprecision(precision) << value;
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

// Render `value` as the shortest decimal string that reads back as the very
// same double, C-locale, never in scientific notation for an integral value.
//
// This exists because `str::xtos(double)` is a bare `stringstream <<`, which
// means six significant digits: a 16 GB memory reading leaves as
// "1.6554e+10" and a byte counter is rounded to the nearest 100 KB. That is
// fine for a human-readable message and wrong for a machine-read exposition
// such as OpenMetrics, where the scraped sample must equal the number the
// JSON endpoint reports for the same metric.
//
// Integral values that fit an int64 are rendered as integers ("12592123904"),
// matching what the JSON endpoints already do (see `gauge_to_json`).
// Otherwise the shortest round-tripping representation wins: precisions are
// tried in turn and the first one that parses back bit-identical is kept, so
// 0.1 stays "0.1" rather than becoming "0.10000000000000001".
//
// Non-finite values are the caller's problem - each exposition format spells
// them differently (OpenMetrics wants "NaN", "+Inf", "-Inf") - and are only
// handled here so that no input can produce an empty string.
inline std::string render_shortest(const double value) {
  if (!std::isfinite(value)) {
    std::ostringstream ss;
    ss.imbue(std::locale::classic());
    ss << value;
    return ss.str();
  }
  // Range first, then integrality: casting an out-of-range double to long long
  // is undefined behaviour, so `fits_int64` has to gate the cast rather than
  // sit beside it.
  if (fits_int64(value) && std::trunc(value) == value) {
    std::ostringstream ss;
    ss.imbue(std::locale::classic());
    ss << static_cast<long long>(value);
    return ss.str();
  }
  // A double carries at most 17 significant decimal digits, so the loop always
  // terminates with an exact round trip on the last iteration at the latest.
  for (int precision = 1; precision <= 17; ++precision) {
    std::ostringstream ss;
    ss.imbue(std::locale::classic());
    ss << std::setprecision(precision) << value;
    const std::string candidate = ss.str();
    std::istringstream back(candidate);
    back.imbue(std::locale::classic());
    double parsed = 0;
    back >> parsed;
    if (!back.fail() && parsed == value) return candidate;
  }
  std::ostringstream ss;
  ss.imbue(std::locale::classic());
  ss << std::setprecision(17) << value;
  return ss.str();
}

}  // namespace str
