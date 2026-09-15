// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <nscapi/nscapi_metrics_helper.hpp>
#include <nscapi/protobuf/metrics.hpp>

#include <utility>

namespace nscapi {
namespace metrics {

metric_builder::metric_builder(PB::Metrics::MetricsBundle *bundle, std::string name) : bundle_(bundle), name_(std::move(name)) {}

metric_builder &metric_builder::instance(const std::string &instance) {
  if (!instance.empty()) key_ = instance + "." + name_;
  return *this;
}

metric_builder &metric_builder::key(const std::string &key) {
  key_ = key;
  return *this;
}

metric_builder &metric_builder::label(const std::string &key, const std::string &value) {
  // An empty value is not a label an exporter may emit: OpenMetrics treats
  // `x=""` and an absent `x` as the same series, so a sample carrying one
  // would silently collide with a sample that has the label filled in. Dropping
  // it here means a producer can pass a field that is sometimes missing (a NIC
  // with no MAC, a zone with no label file) without guarding every call.
  if (key.empty() || value.empty()) return *this;
  labels_.push_back(std::make_pair(key, value));
  return *this;
}

PB::Metrics::Metric *metric_builder::emit() const {
  PB::Metrics::Metric *m = bundle_->add_value();
  // The key is what it has always been, instance and all: every consumer other
  // than the OpenMetrics renderer reads it and nothing else.
  m->set_key(key_.empty() ? name_ : key_);
  if (!labels_.empty()) {
    // `alias` is only meaningful next to `dims`: it names the family the
    // labelled samples share, and without labels to tell those samples apart
    // it would just be an invitation to render several of them as one series.
    // So the two are written together or not at all, and a renderer reading
    // `alias` never has to ask whether the dimensions came with it.
    m->set_alias(name_);
    for (const std::pair<std::string, std::string> &l : labels_) {
      PB::Common::KeyValue *dim = m->add_dims();
      dim->set_key(l.first);
      dim->set_value(l.second);
    }
  }
  return m;
}

void metric_builder::gauge(const double value) { emit()->mutable_gauge_value()->set_value(value); }
void metric_builder::gauge(const long long value) { emit()->mutable_gauge_value()->set_value(static_cast<double>(value)); }
void metric_builder::gauge(const unsigned long long value) { emit()->mutable_gauge_value()->set_value(static_cast<double>(value)); }
void metric_builder::info(const std::string &value) { emit()->mutable_string_value()->set_value(value); }

metric_builder metric(PB::Metrics::MetricsBundle *bundle, const std::string &name) { return metric_builder(bundle, name); }

std::string core_label(const std::string &cpu_key) {
  static const std::string prefix = "core";
  if (cpu_key.size() <= prefix.size() || cpu_key.compare(0, prefix.size(), prefix) != 0) return cpu_key;
  const char separator = cpu_key[prefix.size()];
  if (separator != ' ' && separator != '_') return cpu_key;
  const std::string core = cpu_key.substr(prefix.size() + 1);
  // `core_` with nothing after it is not a key anything produces, but reducing
  // it to an empty label would drop the label entirely - and a sample missing
  // the label that tells it apart from its siblings collides with them.
  return core.empty() ? cpu_key : core;
}

void add_metric(PB::Metrics::MetricsBundle *b, const std::string &key, long long value) {
  PB::Metrics::Metric *m = b->add_value();
  m->set_key(key);
  m->mutable_gauge_value()->set_value(static_cast<double>(value));
}
void add_metric(PB::Metrics::MetricsBundle *b, const std::string &key, unsigned long long value) {
  PB::Metrics::Metric *m = b->add_value();
  m->set_key(key);
  m->mutable_gauge_value()->set_value(static_cast<double>(value));
}
void add_metric(PB::Metrics::MetricsBundle *b, const std::string &key, std::string value) {
  PB::Metrics::Metric *m = b->add_value();
  m->set_key(key);
  m->mutable_string_value()->set_value(value);
}
void add_metric(PB::Metrics::MetricsBundle *b, const std::string &key, double value) {
  PB::Metrics::Metric *m = b->add_value();
  m->set_key(key);
  m->mutable_gauge_value()->set_value(value);
}
}  // namespace metrics
}  // namespace nscapi
