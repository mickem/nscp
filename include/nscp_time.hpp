/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <algorithm>
#include <boost/date_time/local_time/local_time.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <cctype>
#include <string>

namespace nscp_time {

/**
 * Stateless time helpers for plugins that cache a timezone string.
 *
 * The cached value is owned by each plugin (typically loaded in its
 * `loadModuleEx` from `/settings/default/timezone`). This module is
 * intentionally free of global state so it can be unit tested without
 * any settings core, and so plugins can choose to cache (or not).
 *
 * Accepted timezone strings:
 *   - "local" (default)         ... use the host's local clock
 *   - "utc"                     ... use UTC
 *   - any POSIX TZ string parseable by boost::local_time::posix_time_zone
 *     (for example "MST-07" or "EST-05EDT,M3.2.0,M11.1.0")
 *
 * IANA names (e.g. "Europe/Stockholm") are NOT supported by Boost out of
 * the box; users wanting a named zone must supply the POSIX form. An
 * unparseable string falls back to UTC and the label "UTC?" so the user
 * can spot the misconfiguration.
 */

inline std::string normalize(const std::string &tz) {
  std::string out;
  out.reserve(tz.size());
  for (char c : tz) out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
  // strip trailing/leading whitespace (cheap)
  while (!out.empty() && std::isspace(static_cast<unsigned char>(out.back()))) out.pop_back();
  std::size_t i = 0;
  while (i < out.size() && std::isspace(static_cast<unsigned char>(out[i]))) ++i;
  if (i > 0) out.erase(0, i);
  return out;
}

inline bool is_local(const std::string &tz) {
  const std::string n = normalize(tz);
  return n.empty() || n == "local";
}

inline bool is_utc(const std::string &tz) {
  const std::string n = normalize(tz);
  return n == "utc" || n == "gmt";
}

/**
 * Returns the current wall-clock time in the requested zone as a
 * boost::posix_time::ptime. The returned ptime has no zone information
 * attached; it is just the local-clock equivalent for the configured
 * zone. Callers may subtract durations from it to obtain other points
 * in the same zone (e.g. boot time = now - uptime).
 */
inline boost::posix_time::ptime now(const std::string &tz) {
  if (is_utc(tz)) return boost::posix_time::second_clock::universal_time();
  if (is_local(tz)) return boost::posix_time::second_clock::local_time();
  try {
    boost::local_time::time_zone_ptr zone(new boost::local_time::posix_time_zone(tz));
    boost::local_time::local_date_time ldt(boost::posix_time::second_clock::universal_time(), zone);
    return ldt.local_time();
  } catch (const std::exception &) {
    // Fall back to UTC for unparseable strings; tz_label will surface
    // the error to the user via the syntax label.
    return boost::posix_time::second_clock::universal_time();
  }
}

/**
 * Returns a short human-readable label for the configured zone, suitable
 * for embedding in syntax strings such as "boot: ${boot} (${tz})".
 *   - "local" / ""     -> "local"
 *   - "utc" / "gmt"    -> "UTC"
 *   - parseable POSIX  -> std abbreviation (e.g. "MST")
 *   - anything else    -> "UTC?" (fallback indicator)
 */
inline std::string tz_label(const std::string &tz) {
  if (is_utc(tz)) return "UTC";
  if (is_local(tz)) return "local";
  try {
    boost::local_time::posix_time_zone zone(tz);
    const std::string abbrev = zone.std_zone_abbrev();
    if (!abbrev.empty()) return abbrev;
    return tz;
  } catch (const std::exception &) {
    return "UTC?";
  }
}

}  // namespace nscp_time
