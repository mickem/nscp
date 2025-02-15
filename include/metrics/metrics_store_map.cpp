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

#include <metrics/metrics_store_map.hpp>

#include <str/xtos.hpp>

namespace metrics {

void build_metrics(metrics_store::values_map &metrics, const PB::Metrics::MetricsBundle &b, const std::string &path) {
  std::string p = "";
  if (!path.empty()) p += path + ".";
  p += b.key();

  for (const PB::Metrics::MetricsBundle &b2 : b.children()) {
    build_metrics(metrics, b2, p);
  }

  for (const PB::Metrics::Metric &v : b.value()) {
    if (v.has_gauge_value())
      metrics[p + "." + v.key()] = str::xtos(v.gauge_value().value());
    else if (v.has_string_value())
      metrics[p + "." + v.key()] = v.string_value().value();
  }
}

void metrics_store::set(const PB::Metrics::MetricsMessage &response) {
  metrics_store::values_map tmp;

  for (const PB::Metrics::MetricsMessage::Response &p : response.payload()) {
    for (const PB::Metrics::MetricsBundle &b : p.bundles()) {
      build_metrics(tmp, b, "");
    }
  }
  {
    boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
    if (!lock.owns_lock()) return;
    values_ = tmp;
  }
}

metrics_store::values_map metrics_store::get(const std::string &filter) {
  bool f = !filter.empty();
  metrics_store::values_map ret;
  {
    boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
    if (!lock.owns_lock()) return ret;

    for (const values_map::value_type &v : values_) {
      if (!f || v.first.find(filter) != std::string::npos) ret[v.first] = v.second;
    }
  }
  return ret;
}

}  // namespace metrics