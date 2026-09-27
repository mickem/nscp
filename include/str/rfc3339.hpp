// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/optional.hpp>
#include <string>

namespace str {

// Parse an RFC 3339 timestamp to UTC: "2026-08-12T07:44:00Z" as the docker
// daemon and the Kubernetes API server emit it, with or without fractional
// seconds (dropped), and with a numeric offset ("2026-08-12T09:44:00+02:00")
// applied. A missing zone reads as UTC. None when the text is empty or not a
// timestamp.
inline boost::optional<boost::posix_time::ptime> parse_rfc3339(const std::string &rfc3339) {
  // "YYYY-MM-DDTHH:MM:SS" is 19 characters; a fraction and a zone may follow.
  const std::string::size_type date_time = 19;
  if (rfc3339.size() < date_time) return boost::none;
  const auto digit = [&rfc3339](const std::string::size_type i) { return rfc3339[i] >= '0' && rfc3339[i] <= '9'; };
  std::string::size_type pos = date_time;
  if (pos < rfc3339.size() && rfc3339[pos] == '.') {
    const std::string::size_type first = ++pos;
    while (pos < rfc3339.size() && digit(pos)) ++pos;
    if (pos == first) return boost::none;
  }
  long offset_minutes = 0;
  const std::string::size_type zone = rfc3339.size() - pos;
  if (zone == 1 && (rfc3339[pos] == 'Z' || rfc3339[pos] == 'z')) {
    // UTC
  } else if (zone == 6 && (rfc3339[pos] == '+' || rfc3339[pos] == '-') && digit(pos + 1) && digit(pos + 2) && rfc3339[pos + 3] == ':' && digit(pos + 4) &&
             digit(pos + 5)) {
    const long hours = (rfc3339[pos + 1] - '0') * 10 + (rfc3339[pos + 2] - '0');
    const long minutes = (rfc3339[pos + 4] - '0') * 10 + (rfc3339[pos + 5] - '0');
    if (hours > 23 || minutes > 59) return boost::none;
    offset_minutes = (hours * 60 + minutes) * (rfc3339[pos] == '-' ? -1 : 1);
  } else if (zone != 0) {
    return boost::none;
  }
  try {
    const boost::posix_time::ptime local = boost::posix_time::from_iso_extended_string(rfc3339.substr(0, date_time));
    if (local.is_special()) return boost::none;
    // The text is the UTC time plus the offset.
    return local - boost::posix_time::minutes(offset_minutes);
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
