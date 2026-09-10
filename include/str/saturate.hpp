// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <cmath>
#include <limits>

namespace str {

// Convert a double to long long without undefined behaviour: converting a
// NaN or a value outside the integer range is UB, so clamp instead of cast.
// Written as (max)() because windows.h defines min/max macros.
// The upper bound is 2^63 exactly, because numeric_limits<long long>::max()
// itself rounds up to 2^63 as a double and would admit that value.
inline bool fits_int64(double v) { return !std::isnan(v) && v < 9223372036854775808.0 && v >= -9223372036854775808.0; }

// Truncates toward zero, exactly like the static_cast<long long> it replaces:
// the filter engine documents `9.7` as `9`, so this must not round.
inline long long to_int64_saturating(double v) {
  if (std::isnan(v)) return 0;
  if (v >= 9223372036854775808.0) return (std::numeric_limits<long long>::max)();
  if (v <= -9223372036854775808.0) return (std::numeric_limits<long long>::min)();
  return static_cast<long long>(v);
}

}  // namespace str
