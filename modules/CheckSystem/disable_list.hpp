// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <set>
#include <str/utils_no_boost.hpp>
#include <string>

namespace disable_list {

// The values accepted by the `disable` setting in [/settings/system/windows].
inline const std::set<std::string> &known_tokens() {
  static const std::set<std::string> tokens = {"battery", "cpu", "cpu_frequency", "handles", "metrics", "network", "os_updates", "pdh", "temperature"};
  return tokens;
}

// Parse the comma-separated `disable` setting into whole tokens. Tokens must
// be compared exactly, never by substring: "cpu" is a substring of
// "cpu_frequency", so substring matching made `disable = cpu_frequency`
// silently stop the CPU load sampling as well (#1368).
inline std::set<std::string> parse(const std::string &raw) {
  std::set<std::string> ret;
  for (const std::string &entry : str::utils::split_lst(raw, std::string(","))) {
    const auto first = entry.find_first_not_of(" \t");
    if (first == std::string::npos) continue;
    const auto last = entry.find_last_not_of(" \t");
    ret.insert(entry.substr(first, last - first + 1));
  }
  return ret;
}

}  // namespace disable_list
