// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/protobuf/metrics.hpp>

namespace nscapi {
namespace metrics {
void add_metric(PB::Metrics::MetricsBundle *b, const std::string &key, long long value);
void add_metric(PB::Metrics::MetricsBundle *b, const std::string &key, unsigned long long value);
void add_metric(PB::Metrics::MetricsBundle *b, const std::string &key, std::string value);
void add_metric(PB::Metrics::MetricsBundle *b, const std::string &key, double value);
}  // namespace metrics
}  // namespace nscapi
