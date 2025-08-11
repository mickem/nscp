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

#include <boost/thread/mutex.hpp>
#include <map>
#include <nscapi/nscapi_protobuf_metrics.hpp>
#include <string>

namespace metrics {

struct metrics_store {
  typedef std::map<std::string, std::string> values_map;
  void set(const PB::Metrics::MetricsMessage &response);
  values_map get(const std::string &filter);

 private:
  values_map values_;
  boost::timed_mutex mutex_;
};

}  // namespace metrics
