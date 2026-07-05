// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/thread.hpp>
#include <map>
#include <nscapi/protobuf/metrics.hpp>
#include <string>

namespace metrics {

struct metrics_store {
  typedef std::map<std::string, std::string> values_map;
  void set(const PB::Metrics::MetricsMessage &response);
  values_map get(const std::string &filter) const;

 private:
  values_map values_;
  mutable boost::timed_mutex mutex_;
};

}  // namespace metrics
