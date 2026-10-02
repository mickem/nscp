// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <str/format.hpp>
#include <string>
#include <utility>
#include <vector>

/**
 * The time= windows of a check that averages a collector's buffer (check_cpu
 * on every platform), decoded once so each platform rejects the same values.
 * Validated before the collector is consulted: a bad window is a
 * configuration error and must not hide behind a warm-up answer.
 */
namespace time_windows {

// Each entry is the window as written and its length in seconds.
typedef std::vector<std::pair<std::string, long>> list;

// Fills `out` and returns an empty string, or returns why the first bad
// window is rejected.
inline std::string decode(const std::vector<std::string> &times, list &out) {
  out.clear();
  for (const std::string &time : times) {
    long seconds = 0;
    try {
      seconds = str::format::decode_time<long>(time, 1);
    } catch (const std::exception &e) {
      return "Invalid time '" + time + "': " + e.what();
    }
    if (seconds <= 0) return "Invalid time '" + time + "': the window must be at least one second";
    out.emplace_back(time, seconds);
  }
  return "";
}

}  // namespace time_windows
