// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/optional.hpp>
#include <string>

namespace str {

// Parse an RFC 3339 / ISO 8601 UTC timestamp as the docker daemon and the
// Kubernetes API server emit them ("2026-08-12T07:44:00Z",
// "2026-08-12T07:44:00.123456789Z"). Fractional seconds are dropped. None when
// the text is empty or not a timestamp.
inline boost::optional<boost::posix_time::ptime> parse_rfc3339(const std::string &rfc3339) {
  if (rfc3339.empty()) return boost::none;
  std::string s = rfc3339;
  const auto dot = s.find('.');
  if (dot != std::string::npos) {
    s = s.substr(0, dot);
  } else if (s.back() == 'Z') {
    s.pop_back();
  }
  try {
    const boost::posix_time::ptime t = boost::posix_time::from_iso_extended_string(s);
    if (t.is_special()) return boost::none;
    return t;
  } catch (const std::exception &) {
    return boost::none;
  }
}

// Seconds elapsed since an RFC 3339 timestamp; -1 when it does not parse.
inline long long seconds_since_rfc3339(const std::string &rfc3339) {
  const boost::optional<boost::posix_time::ptime> t = parse_rfc3339(rfc3339);
  if (!t) return -1;
  const boost::posix_time::ptime now = boost::posix_time::second_clock::universal_time();
  return (now - t.value()).total_seconds();
}

}  // namespace str
