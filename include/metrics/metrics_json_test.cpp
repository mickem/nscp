// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The flat JSON rendering of a metrics snapshot. Three consumers depend on it
// staying identical (`/api/v2/metrics`, the failed-check context, the fleet
// failure report), so the shape is pinned here rather than in any of them.

#include <gtest/gtest.h>

#include <metrics/metrics_json.hpp>
#include <string>

namespace json = boost::json;

namespace {
PB::Metrics::MetricsMessage snapshot() {
  PB::Metrics::MetricsMessage message;
  PB::Metrics::MetricsBundle *system = message.add_payload()->add_bundles();
  system->set_key("system");
  system->set_desc("System counters");
  PB::Metrics::MetricsBundle *cpu = system->add_children();
  cpu->set_key("cpu");
  PB::Metrics::Metric *idle = cpu->add_value();
  idle->set_key("idle");
  idle->set_desc("Idle time");
  idle->set_unit("percent");
  idle->mutable_gauge_value()->set_value(87.5);
  PB::Common::KeyValue *dim = idle->add_dims();
  dim->set_key("core");
  dim->set_value("0");
  PB::Metrics::Metric *jobs = system->add_value();
  jobs->set_key("jobs");
  jobs->mutable_counter_value()->set_value(12);
  PB::Metrics::Metric *os = system->add_value();
  os->set_key("os");
  os->mutable_string_value()->set_value("Windows");
  PB::Metrics::Metric *empty = system->add_value();
  empty->set_key("nothing");
  return message;
}
}  // namespace

TEST(metrics_json, flat_keys_join_the_bundle_trail) {
  json::object list, meta;
  metrics_json::build(list, &meta, snapshot());
  ASSERT_EQ(list.size(), 3u);
  EXPECT_EQ(list.at("system.cpu.idle").as_double(), 87.5);
  // A whole number comes out as an integer, and a counter is still a number.
  EXPECT_EQ(list.at("system.jobs").as_int64(), 12);
  EXPECT_EQ(list.at("system.os").as_string(), "Windows");
  // A metric without a value is neither listed nor described.
  EXPECT_EQ(list.if_contains("system.nothing"), nullptr);
  EXPECT_EQ(meta.if_contains("system.nothing"), nullptr);
}

TEST(metrics_json, metadata_carries_only_what_was_declared) {
  json::object list, meta;
  metrics_json::build(list, &meta, snapshot());
  const json::object &idle = meta.at("system.cpu.idle").as_object();
  EXPECT_EQ(idle.at("type").as_string(), "gauge");
  EXPECT_EQ(idle.at("help").as_string(), "Idle time");
  EXPECT_EQ(idle.at("unit").as_string(), "percent");
  EXPECT_EQ(idle.at("labels").as_object().at("core").as_string(), "0");
  const json::object &jobs = meta.at("system.jobs").as_object();
  EXPECT_EQ(jobs.at("type").as_string(), "counter");
  // Falls back to the bundle's help, as the exposition does.
  EXPECT_EQ(jobs.at("help").as_string(), "System counters");
  EXPECT_EQ(jobs.if_contains("unit"), nullptr);
  EXPECT_EQ(meta.at("system.os").as_object().at("type").as_string(), "info");
}

TEST(metrics_json, metadata_is_optional) {
  json::object list;
  metrics_json::build(list, nullptr, snapshot());
  EXPECT_EQ(list.size(), 3u);
}

TEST(metrics_json, gauge_to_json_keeps_integers_integral) {
  EXPECT_TRUE(metrics_json::gauge_to_json(12592123904.0).is_int64());
  EXPECT_TRUE(metrics_json::gauge_to_json(0.5).is_double());
  // 2^63 does not fit an int64 and must stay a double.
  EXPECT_TRUE(metrics_json::gauge_to_json(9223372036854775808.0).is_double());
}
