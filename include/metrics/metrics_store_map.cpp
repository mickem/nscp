// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <metrics/metrics_store_map.hpp>
#include <nscapi/nscapi_metrics_helper.hpp>

#include <cmath>
#include <locale>
#include <sstream>

namespace metrics {
namespace {

// The store hands values out as strings (the Lua Metrics() API, which
// default_check_mk.lua copies straight into the agent dump), so the
// conversion has to keep every digit. A plain stream keeps six significant
// digits: 32 GiB of RAM came out as 3.37775e+10 bytes and an uptime past
// 999999 s in scientific notation. Integral values - byte counts, seconds,
// counters - print as exact integers; anything else keeps 15 significant
// digits, which round-trips decimal input without max_digits10's noise
// (0.1 stays "0.1").
std::string format_value(const double value) {
  if (std::isfinite(value) && value == std::floor(value) && std::fabs(value) < 9.0e18) {
    return std::to_string(static_cast<long long>(value));
  }
  std::ostringstream ss;
  ss.imbue(std::locale::classic());
  ss.precision(15);
  ss << value;
  return ss.str();
}

void build_metrics(metrics_store::values_map &metrics, const PB::Metrics::MetricsBundle &b, const std::string &path) {
  const std::string p = path.empty() ? b.key() : path + "." + b.key();

  for (const PB::Metrics::MetricsBundle &b2 : b.children()) {
    build_metrics(metrics, b2, p);
  }

  for (const PB::Metrics::Metric &v : b.value()) {
    double value = 0;
    if (nscapi::metrics::numeric_value(v, value))
      metrics[p + "." + v.key()] = format_value(value);
    else if (v.has_string_value())
      metrics[p + "." + v.key()] = v.string_value().value();
  }
}

}  // namespace

void metrics_store::set(const PB::Metrics::MetricsMessage &response) {
  values_map tmp;

  for (const PB::Metrics::MetricsMessage::Response &p : response.payload()) {
    for (const PB::Metrics::MetricsBundle &b : p.bundles()) {
      build_metrics(tmp, b, "");
    }
  }
  {
    const boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
    if (!lock.owns_lock()) return;
    values_ = std::move(tmp);
  }
}

metrics_store::values_map metrics_store::get(const std::string &filter) const {
  const bool f = !filter.empty();
  values_map ret;
  {
    const boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
    if (!lock.owns_lock()) return ret;

    for (const values_map::value_type &v : values_) {
      if (!f || v.first.find(filter) != std::string::npos) ret[v.first] = v.second;
    }
  }
  return ret;
}

}  // namespace metrics