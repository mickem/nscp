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

#include <boost/algorithm/string.hpp>
#include <nsclient/logger/log_level.hpp>

bool nsclient::logging::log_level::set(const std::string& level) {
  const std::string lc = boost::to_lower_copy(level);
  if (lc == "critical" || lc == "crit" || lc == "c") {
    current_level_ = critical;
    return true;
  }
  if (lc == "error" || lc == "err" || lc == "e") {
    current_level_ = error;
    return true;
  }
  if (lc == "warning" || lc == "warn" || lc == "w") {
    current_level_ = warning;
    return true;
  }
  if (lc == "info" || lc == "log" || lc == "i") {
    current_level_ = info;
    return true;
  }
  if (lc == "debug" || lc == "d") {
    current_level_ = debug;
    return true;
  }
  if (lc == "trace" || lc == "t") {
    current_level_ = trace;
    return true;
  }
  return false;
}
std::string nsclient::logging::log_level::get() const {
  if (current_level_ == critical) return "critical";
  if (current_level_ == error) return "error";
  if (current_level_ == warning) return "warning";
  if (current_level_ == info) return "message";
  if (current_level_ == debug) return "debug";
  if (current_level_ == trace) return "trace";
  return "unknown";
}
