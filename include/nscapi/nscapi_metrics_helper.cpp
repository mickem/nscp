// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <nscapi/protobuf/metrics.hpp>

namespace nscapi {
namespace metrics {
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
