// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/date_time/posix_time/posix_time.hpp>
#include <nscp_time.hpp>
#include <str/format.hpp>
#include <string>
#include <utility>

namespace check_uptime_filter_common {

/**
 * Filter object for `check_uptime`, shared between the Windows and Unix
 * CheckSystem modules to prevent drift. The owning plugin caches the
 * configured timezone string (from `/settings/default/timezone` in its
 * `loadModuleEx`) and passes it in here; rendering uses the cached value
 * via the stateless `nscp_time` helper.
 */
struct filter_obj {
  long long uptime;
  long long now;
  boost::posix_time::ptime boot;
  std::string tz;
  // Largest unit used when rendering `${uptime}`. The check command surfaces
  // this as the `max-unit` argument (s|m|h|d|w); default `unit_week` matches
  // the historical formatting (issue #590).
  str::format::itos_as_time_unit max_unit;

  filter_obj(long long uptime, long long now, boost::posix_time::ptime boot, std::string tz, str::format::itos_as_time_unit max_unit = str::format::unit_week)
      : uptime(uptime), now(now), boot(boot), tz(std::move(tz)), max_unit(max_unit) {}

  std::string show() const { return get_uptime_s(); }
  long long get_uptime() const { return uptime; }
  long long get_boot() const { return now - uptime; }
  std::string get_boot_s() const { return str::format::format_date(boot); }
  std::string get_uptime_s() const { return str::format::itos_as_time(get_uptime() * 1000, max_unit); }
  // Short label for the configured timezone (e.g. "UTC", "local", "MST").
  std::string get_tz() const { return nscp_time::tz_label(tz); }
};

}  // namespace check_uptime_filter_common
