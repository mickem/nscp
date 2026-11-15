// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/json.hpp>
#include <cmath>
#include <cstdint>
#include <nscapi/nscapi_metrics_helper.hpp>
#include <str/saturate.hpp>
#include <string>

// The flat JSON rendering of a metrics snapshot: the `{key: value}` list of
// `/api/v2/metrics`, and the per-key metadata that same endpoint serves for
// `?meta=1`. Header-only, because more than one place renders a snapshot and
// they must agree byte for byte: the WEBServer latches it for the metrics
// endpoint, the failed-check log serves the snapshot a failure was recorded
// with, and the fleet sync reports that same snapshot to the server. The key
// is the wire format (`<bundle>.<child>.<metric>`), so every one of them
// reads it from here.
//
// The OpenMetrics exposition is a different shape - samples grouped into
// families - and lives in modules/WEBServer/openmetrics_renderer.cpp.
namespace metrics_json {

// A whole number is emitted as an integer, so `12592123904` does not come out
// as `1.2592123904e10`. fits_int64 bounds at 2^63 exactly:
// numeric_limits::max() rounds up to 2^63 as a double, so a plain comparison
// admitted that one value.
inline boost::json::value gauge_to_json(const double v) {
  if (std::trunc(v) == v && str::fits_int64(v)) {
    return boost::json::value(static_cast<std::int64_t>(v));
  }
  return boost::json::value(v);
}

// The OpenMetrics type of a metric, spelled as the exposition spells it, for
// the described JSON view. A string has no numeric sample and becomes a label
// of its bundle's `_info` family, so it reports as `info` here rather than as
// a type the `/api/v2/metrics` reader could mistake for a number.
inline const char *metric_type_name(const PB::Metrics::Metric &v) {
  if (v.has_counter_value()) return "counter";
  if (v.has_untyped_value()) return "unknown";
  if (v.has_string_value()) return "info";
  if (v.has_summary_value()) return "summary";
  if (v.has_histogram_value()) return "histogram";
  return "gauge";
}

// What a producer said one metric means: the `# HELP` text (falling back to
// the bundle's, exactly as the exposition falls back), the unit, the type and
// the labels. Only the parts that were declared are emitted, so a metric a
// module publishes through the bare `add_metric()` shorthand carries its type
// and nothing it never said.
inline boost::json::value describe_metric(const PB::Metrics::Metric &v, const PB::Metrics::MetricsBundle &b) {
  boost::json::object meta;
  meta.insert(boost::json::object::value_type("type", std::string(metric_type_name(v))));
  const std::string &help = v.desc().empty() ? b.desc() : v.desc();
  if (!help.empty()) meta.insert(boost::json::object::value_type("help", help));
  if (!v.unit().empty()) meta.insert(boost::json::object::value_type("unit", v.unit()));
  if (v.dims_size() > 0) {
    boost::json::object labels;
    for (const PB::Common::KeyValue &dim : v.dims()) {
      labels.insert(boost::json::object::value_type(dim.key(), dim.value()));
    }
    meta.insert(boost::json::object::value_type("labels", labels));
  }
  return boost::json::value(meta);
}

// One bundle and its children, keyed under `trail`. `metrics_meta` may be
// null when only the values are wanted.
inline void build_bundle(boost::json::object &metrics_list, boost::json::object *metrics_meta, const std::string &trail, const PB::Metrics::MetricsBundle &b) {
  for (const PB::Metrics::MetricsBundle &b2 : b.children()) {
    build_bundle(metrics_list, metrics_meta, trail + "." + b2.key(), b2);
  }
  for (const PB::Metrics::Metric &v : b.value()) {
    // Any numeric type, not just a gauge: a producer that types a monotonic
    // count as a counter must not disappear from the JSON views and the web UI
    // dashboard on its way to being described properly on the OpenMetrics one.
    double value = 0;
    if (nscapi::metrics::numeric_value(v, value)) {
      metrics_list.insert(boost::json::object::value_type(trail + "." + v.key(), gauge_to_json(value)));
    } else if (v.has_string_value()) {
      metrics_list.insert(boost::json::object::value_type(trail + "." + v.key(), v.string_value().value()));
    } else {
      // Nothing the flat views can show, so nothing to describe either: the
      // metadata document is keyed by the same keys as the value document.
      continue;
    }
    if (metrics_meta != nullptr) metrics_meta->insert(boost::json::object::value_type(trail + "." + v.key(), describe_metric(v, b)));
  }
}

// A whole snapshot: every payload's bundles, each keyed by its own key.
inline void build(boost::json::object &metrics_list, boost::json::object *metrics_meta, const PB::Metrics::MetricsMessage &message) {
  for (const PB::Metrics::MetricsMessage::Response &p : message.payload()) {
    for (const PB::Metrics::MetricsBundle &b : p.bundles()) {
      build_bundle(metrics_list, metrics_meta, b.key(), b);
    }
  }
}

}  // namespace metrics_json
