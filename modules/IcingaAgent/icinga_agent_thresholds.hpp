// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// The thresholds of the Icinga 2 Windows plugins (check_disk.exe,
// check_load.exe, ...), as plugins/thresholds.cpp in Icinga 2 reads them, and
// what they are in the terms of an NSClient++ filter check.
//
//   20        alert when the value is above 20
//   !20       inverted: alert when it is not above 20 (20 or less)
//   [10-20]   alert when it is inside 10..20, both ends included
//   ![10-20]  inverted: alert when it is outside
//   20%       the same forms as a percentage (both ends of a range, or neither)
//
// A plugin that watches free space (disk, memory, swap) flips the reading:
// there "20%" alerts at 20% free or less. The plugin says so, not the
// threshold, which is why the conversions below take `inverted`.
//
// Kept free of everything but the standard library, like the short threshold
// parser it feeds (parsers/filter/threshold_shorthand.hpp).

#include <stdexcept>
#include <string>

namespace icinga_agent {

// A threshold the Icinga plugin would have refused as well.
struct threshold_error : std::runtime_error {
  using std::runtime_error::runtime_error;
};

struct plugin_threshold {
  bool negated = false;   // a leading `!`
  bool is_range = false;  // `[lower-upper]`
  bool percent = false;
  std::string lower;  // the number as written; for a single value, the same as upper
  std::string upper;
};

// Throws threshold_error.
plugin_threshold parse_plugin_threshold(const std::string &text);

// The threshold as a range in the monitoring plugins format, which is what the
// -w / -c of a filter check take: "~:20" (above 20), "@~:20" (20 or less),
// "@10:20" (inside), "10:20" (outside). The comparisons come out exactly as
// the plugin made them, boundaries included.
//
// `suffix` is the unit of a value that is not a percentage ("m", "s", ...);
// a percentage keeps its "%" when `keep_percent` is set and loses it
// otherwise, for a keyword that already is one.
std::string to_range(const plugin_threshold &threshold, bool inverted, const std::string &suffix, bool keep_percent);

// The same threshold as a filter expression on `keyword`, for the cases the
// short form cannot say because it bounds another keyword (`used` when the
// check's short form bounds `free`).
std::string to_expression(const plugin_threshold &threshold, bool inverted, const std::string &keyword, const std::string &suffix, bool keep_percent);

}  // namespace icinga_agent
