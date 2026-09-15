// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/protobuf/metrics.hpp>
#include <string>
#include <utility>

// How a module declares a metric.
//
// `add_metric` is the original shorthand: a key and a number, typed as a gauge,
// with no description and no unit. It still works and out-of-tree modules can
// keep calling it, but everything it produces is anonymous on the OpenMetrics
// endpoint - a name and a value, with nothing telling a scraper what the value
// means or what it is measured in.
//
// `metric()` is the builder that fills that in:
//
//   metric(mem, "physical.used").help("Physical memory in use").unit("bytes").gauge(used);
//   metric(bundle, "jobs").help("Scheduled jobs executed since start").counter(tasks);
//   metric(section, "uptime").help("Uptime, human readable").info(uptime_str);
//   describe(mem, "Memory as reported by the kernel");   // fallback help for the bundle
//
// A new metric should declare at least `help`, and `unit` whenever the value is
// measured in something (bytes, seconds, percent). The choice of terminal call
// is the metric's type, and it is a real choice: `counter` is for a value that
// only ever grows for the lifetime of the process (jobs run, errors seen), so a
// scraper may `rate()` it across a restart; `gauge` is for anything that can go
// down again. Getting it wrong is not cosmetic - a gauge read as a counter
// produces nonsense at every dip.
namespace nscapi {
namespace metrics {

// The no-metadata shorthand. Kept for out-of-tree modules and for the places
// where a key is all that is known (a PDH counter, a Python script's dict).
void add_metric(PB::Metrics::MetricsBundle *b, const std::string &key, long long value);
void add_metric(PB::Metrics::MetricsBundle *b, const std::string &key, unsigned long long value);
void add_metric(PB::Metrics::MetricsBundle *b, const std::string &key, std::string value);
void add_metric(PB::Metrics::MetricsBundle *b, const std::string &key, double value);

// Accumulates the metadata and appends the metric on the terminal call, so a
// half-built metric can never reach the bundle. Deliberately not reusable: one
// builder is one metric.
class metric_builder {
 public:
  metric_builder(PB::Metrics::MetricsBundle *bundle, std::string key) : bundle_(bundle), key_(std::move(key)) {}

  // One line, no trailing period, describing what the value is. Becomes
  // `# HELP`; a metric without one inherits its bundle's `describe()` text.
  metric_builder &help(std::string text) {
    help_ = std::move(text);
    return *this;
  }
  // What the value is measured in: `bytes`, `seconds`, `percent`, ... Becomes
  // `# UNIT`, and the family name is made to end with it, which is what
  // OpenMetrics requires of a family that declares a unit. Leave it off for a
  // plain count and for a rate (`bytes per second` is not a unit, and a name
  // ending in `_bytes` would lie about what the sample is).
  metric_builder &unit(std::string unit) {
    unit_ = std::move(unit);
    return *this;
  }

  // A value that can go up and down.
  template <typename T>
  void gauge(const T value) {
    add()->mutable_gauge_value()->set_value(static_cast<double>(value));
  }
  // A value that only grows while the process lives. Rendered with the `_total`
  // suffix the spec reserves for it.
  template <typename T>
  void counter(const T value) {
    add()->mutable_counter_value()->set_value(static_cast<double>(value));
  }
  // A number whose direction is genuinely unknown. Rare - prefer gauge.
  template <typename T>
  void untyped(const T value) {
    add()->mutable_untyped_value()->set_value(static_cast<double>(value));
  }
  // A string: an uptime, a MAC address, a power source. It has no numeric
  // sample, so the OpenMetrics renderer folds it into its bundle's `_info`
  // family as a label instead.
  void info(const std::string &value) { add()->mutable_string_value()->set_value(value); }

 private:
  PB::Metrics::Metric *add() {
    PB::Metrics::Metric *m = bundle_->add_value();
    m->set_key(key_);
    if (!help_.empty()) m->set_desc(help_);
    if (!unit_.empty()) m->set_unit(unit_);
    return m;
  }

  PB::Metrics::MetricsBundle *bundle_;
  std::string key_;
  std::string help_;
  std::string unit_;
};

inline metric_builder metric(PB::Metrics::MetricsBundle *bundle, std::string key) { return metric_builder(bundle, std::move(key)); }

// Help text for a whole bundle, used for every metric in it that declares none
// of its own. The cheap way to give a section of near-identical metrics (one
// per CPU core, one per NIC) a description without repeating it per metric.
inline void describe(PB::Metrics::MetricsBundle *bundle, const std::string &desc) { bundle->set_desc(desc); }

// The numeric sample of a metric, whatever type it was declared as, for the
// consumers that forward numbers and do not care about the distinction (the
// JSON views, Graphite, Elastic, the Python dict). Without it every one of them
// silently drops whatever a producer declares as a counter, which is a metric
// disappearing from a dashboard for no reason its owner could see.
// Returns false for a string metric and for the aggregate types (summary,
// histogram), which have no single value to hand over.
inline bool numeric_value(const PB::Metrics::Metric &metric, double &value) {
  if (metric.has_gauge_value()) {
    value = metric.gauge_value().value();
    return true;
  }
  if (metric.has_counter_value()) {
    value = metric.counter_value().value();
    return true;
  }
  if (metric.has_untyped_value()) {
    value = metric.untyped_value().value();
    return true;
  }
  return false;
}

}  // namespace metrics
}  // namespace nscapi
