// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// Platform-neutral glue between the collector's per-drive trend history and
// the check_drivesize filter keywords (full_in / rate / trend_span /
// trend_samples): key lookup, slope evaluation, total-row aggregation and the
// human renderings. The platform files own the filter_obj and keyword
// registration; everything here is pure and unit-testable.

#include <algorithm>
#include <boost/optional.hpp>
#include <cctype>
#include <list>
#include <map>
#include <parsers/where/node.hpp>
#include <str/format.hpp>
#include <str/xtos.hpp>
#include <string>
#include <trend/trend_buffer.hpp>

namespace drive_trend {

typedef std::map<std::string, trend::trend_buffer> trend_map;

const long long seconds_per_day = 24 * 3600;

// Slope of the drive's used bytes over the window; a default-constructed
// (invalid, 0-sample) result when the drive has no history.
inline trend::slope_result compute(const trend::trend_buffer *buf, const long long now, const long long window) {
  if (!buf) return trend::slope_result();
  return buf->slope_over(now, window);
}

// Unix lookup: an exact match on the mount the row's data comes from. The
// caller resolves `drive=<path>` to its containing mount first (see
// drive_container::trend_key), so this deliberately does NOT fall back to a
// longest-prefix search: the collector skips pseudo filesystems, and a prefix
// search would hand a tmpfs or overlay mount the trend of its nearest tracked
// ancestor - reporting, say, the root filesystem's growth rate for /dev/shm.
// A filesystem the collector does not track simply has no trend.
inline const trend::trend_buffer *lookup_unix(const trend_map &trends, const std::string &mount) {
  const trend_map::const_iterator cit = trends.find(mount);
  return cit == trends.end() ? nullptr : &cit->second;
}

// Windows lookup: the collector keys by "C:" while the filter rows carry
// "C:\" (or lowercase user input); compare case-insensitively without any
// trailing backslash. Un-lettered volumes have no collector row and no trend.
inline const trend::trend_buffer *lookup_win(const trend_map &trends, const std::string &letter) {
  std::string key = letter;
  while (!key.empty() && key.back() == '\\') key.pop_back();
  if (key.empty()) return nullptr;
  std::transform(key.begin(), key.end(), key.begin(), [](const unsigned char c) { return static_cast<char>(std::toupper(c)); });
  for (const trend_map::value_type &v : trends) {
    std::string k = v.first;
    while (!k.empty() && k.back() == '\\') k.pop_back();
    std::transform(k.begin(), k.end(), k.begin(), [](const unsigned char c) { return static_cast<char>(std::toupper(c)); });
    if (k == key) return &v.second;
  }
  return nullptr;
}

// The signed growth rate in bytes/day, or nothing while no trend exists.
inline boost::optional<long long> rate_per_day(const trend::slope_result &r) {
  if (!r.valid) return boost::none;
  return static_cast<long long>(r.slope * static_cast<double>(seconds_per_day));
}

// Seconds until full projected from the *current* free space (not the
// regression endpoint), or nothing when shrinking/unknown. Unreadable rows
// (size 0) have no meaningful projection.
inline boost::optional<long long> full_in(const trend::slope_result &r, const long long free_now, const long long size) {
  if (size <= 0) return boost::none;
  return trend::trend_buffer::project_zero(free_now, r);
}

// Aggregation for the `total` row: the soonest-full drive is the one that
// matters (min), while rates pool additively.
struct total_values {
  boost::optional<long long> full_in;
  boost::optional<long long> rate;
  long long span;
  long long samples;

  total_values() : span(0), samples(0) {}

  void append(const boost::optional<long long> &other_full_in, const boost::optional<long long> &other_rate, const long long other_span,
              const long long other_samples) {
    if (other_full_in && (!full_in || *other_full_in < *full_in)) full_in = other_full_in;
    if (other_rate) rate = rate.get_value_or(0) + *other_rate;
    span = (std::max)(span, other_span);
    samples += other_samples;
  }
};

inline std::string format_full_in(const boost::optional<long long> &v) {
  if (!v) return "never";
  return str::format::itos_as_time(static_cast<unsigned long long>(*v < 0 ? 0 : *v) * 1000);
}

inline std::string format_rate(const boost::optional<long long> &v, const str::number_format &fmt = str::number_format()) {
  if (!v) return "unknown";
  if (*v < 0) return "-" + str::format::format_byte_units(-*v, fmt) + "/day";
  return str::format::format_byte_units(*v, fmt) + "/day";
}

// Duration literal ("12h", or the tokenized [12, h] list form) to seconds, as
// a node for the full_in threshold converter. Same reassembly dance as
// check_uptime's parse_time (issues #452/#589).
inline parsers::where::node_type parse_time_node(parsers::where::evaluation_context context, parsers::where::node_type subject) {
  std::list<parsers::where::node_type> tokens = subject->get_list_value(context);
  std::string expr;
  if (tokens.size() == 2) {
    std::list<parsers::where::node_type>::const_iterator cit = tokens.begin();
    const long long n = (*cit)->get_int_value(context);
    ++cit;
    const std::string unit = (*cit)->get_value(context, parsers::where::type_string).get_string("");
    expr = str::xtos(n) + unit;
  } else {
    expr = subject->get_string_value(context);
  }
  return parsers::where::factory::create_int(str::format::stox_as_time_sec<long long>(expr, "s"));
}

}  // namespace drive_trend
