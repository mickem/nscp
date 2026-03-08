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

#include <gtest/gtest.h>

#include <metrics/metrics_store_map.hpp>
#include <nscapi/protobuf/metrics.hpp>
#include <string>
#include <thread>
#include <vector>

// ============================================================================
// Helper: build a MetricsMessage with a single bundle containing gauge values
// ============================================================================
static PB::Metrics::MetricsMessage make_gauge_message(const std::string &bundle_key,
                                                      const std::vector<std::pair<std::string, double>> &values) {
  PB::Metrics::MetricsMessage msg;
  PB::Metrics::MetricsMessage::Response *resp = msg.add_payload();
  PB::Metrics::MetricsBundle *bundle = resp->add_bundles();
  bundle->set_key(bundle_key);
  for (const auto &kv : values) {
    PB::Metrics::Metric *m = bundle->add_value();
    m->set_key(kv.first);
    m->mutable_gauge_value()->set_value(kv.second);
  }
  return msg;
}

// ============================================================================
// Helper: build a MetricsMessage with a single bundle containing string values
// ============================================================================
static PB::Metrics::MetricsMessage make_string_message(const std::string &bundle_key,
                                                       const std::vector<std::pair<std::string, std::string>> &values) {
  PB::Metrics::MetricsMessage msg;
  PB::Metrics::MetricsMessage::Response *resp = msg.add_payload();
  PB::Metrics::MetricsBundle *bundle = resp->add_bundles();
  bundle->set_key(bundle_key);
  for (const auto &kv : values) {
    PB::Metrics::Metric *m = bundle->add_value();
    m->set_key(kv.first);
    m->mutable_string_value()->set_value(kv.second);
  }
  return msg;
}

// ============================================================================
// MetricsStoreTest fixture
// ============================================================================
class MetricsStoreTest : public ::testing::Test {
 protected:
  metrics::metrics_store store_;
};

// ============================================================================
// get() on empty store
// ============================================================================
TEST_F(MetricsStoreTest, GetEmptyStoreReturnsEmptyMap) {
  const auto result = store_.get("");
  EXPECT_TRUE(result.empty());
}

TEST_F(MetricsStoreTest, GetEmptyStoreWithFilterReturnsEmptyMap) {
  const auto result = store_.get("anything");
  EXPECT_TRUE(result.empty());
}

// ============================================================================
// set() + get() basic round-trip with gauge values
// ============================================================================
TEST_F(MetricsStoreTest, SetAndGetGaugeValues) {
  const auto msg = make_gauge_message("cpu", {{"load", 42.0}, {"idle", 58.0}});
  store_.set(msg);

  const auto result = store_.get("");
  EXPECT_EQ(2u, result.size());
  EXPECT_NE(result.end(), result.find("cpu.load"));
  EXPECT_NE(result.end(), result.find("cpu.idle"));
}

TEST_F(MetricsStoreTest, GaugeValueIsFormattedAsString) {
  const auto msg = make_gauge_message("sys", {{"uptime", 3600.0}});
  store_.set(msg);

  const auto result = store_.get("");
  ASSERT_EQ(1u, result.size());
  // str::xtos(3600.0) → "3600"
  EXPECT_EQ("3600", result.at("sys.uptime"));
}

// ============================================================================
// set() + get() with string values
// ============================================================================
TEST_F(MetricsStoreTest, SetAndGetStringValues) {
  const auto msg = make_string_message("info", {{"version", "1.2.3"}, {"hostname", "myhost"}});
  store_.set(msg);

  const auto result = store_.get("");
  EXPECT_EQ(2u, result.size());
  EXPECT_EQ("1.2.3", result.at("info.version"));
  EXPECT_EQ("myhost", result.at("info.hostname"));
}

// ============================================================================
// set() replaces previous values entirely
// ============================================================================
TEST_F(MetricsStoreTest, SetReplacesAllPreviousValues) {
  store_.set(make_gauge_message("cpu", {{"load", 10.0}}));
  EXPECT_EQ(1u, store_.get("").size());

  // Replace with a completely different set
  store_.set(make_gauge_message("mem", {{"free", 2048.0}, {"used", 4096.0}}));
  const auto result = store_.get("");
  EXPECT_EQ(2u, result.size());
  // Old key must be gone
  EXPECT_EQ(result.end(), result.find("cpu.load"));
  EXPECT_NE(result.end(), result.find("mem.free"));
  EXPECT_NE(result.end(), result.find("mem.used"));
}

// ============================================================================
// get() with filter (substring match)
// ============================================================================
TEST_F(MetricsStoreTest, GetWithFilterReturnsMatchingKeysOnly) {
  const auto msg = make_gauge_message("cpu", {{"load", 50.0}, {"idle", 50.0}});
  store_.set(msg);

  const auto result = store_.get("load");
  EXPECT_EQ(1u, result.size());
  EXPECT_NE(result.end(), result.find("cpu.load"));
}

TEST_F(MetricsStoreTest, GetWithFilterNoMatch) {
  store_.set(make_gauge_message("cpu", {{"load", 50.0}}));

  const auto result = store_.get("nonexistent");
  EXPECT_TRUE(result.empty());
}

TEST_F(MetricsStoreTest, GetWithFilterMatchesSubstring) {
  store_.set(make_gauge_message("system", {{"cpu_load", 1.0}, {"cpu_idle", 2.0}, {"mem_free", 3.0}}));

  const auto result = store_.get("cpu");
  EXPECT_EQ(2u, result.size());
  EXPECT_NE(result.end(), result.find("system.cpu_load"));
  EXPECT_NE(result.end(), result.find("system.cpu_idle"));
  EXPECT_EQ(result.end(), result.find("system.mem_free"));
}

TEST_F(MetricsStoreTest, GetWithEmptyFilterReturnsAll) {
  store_.set(make_gauge_message("a", {{"x", 1.0}, {"y", 2.0}, {"z", 3.0}}));

  const auto result = store_.get("");
  EXPECT_EQ(3u, result.size());
}

// ============================================================================
// Nested bundles (children)
// ============================================================================
TEST_F(MetricsStoreTest, NestedBundlesProduceDottedKeys) {
  PB::Metrics::MetricsMessage msg;
  PB::Metrics::MetricsMessage::Response *resp = msg.add_payload();
  PB::Metrics::MetricsBundle *parent = resp->add_bundles();
  parent->set_key("system");

  PB::Metrics::MetricsBundle *child = parent->add_children();
  child->set_key("cpu");
  PB::Metrics::Metric *m = child->add_value();
  m->set_key("load");
  m->mutable_gauge_value()->set_value(75.0);

  store_.set(msg);

  const auto result = store_.get("");
  EXPECT_EQ(1u, result.size());
  EXPECT_NE(result.end(), result.find("system.cpu.load"));
}

TEST_F(MetricsStoreTest, DeeplyNestedBundles) {
  PB::Metrics::MetricsMessage msg;
  PB::Metrics::MetricsMessage::Response *resp = msg.add_payload();

  PB::Metrics::MetricsBundle *l1 = resp->add_bundles();
  l1->set_key("a");
  PB::Metrics::MetricsBundle *l2 = l1->add_children();
  l2->set_key("b");
  PB::Metrics::MetricsBundle *l3 = l2->add_children();
  l3->set_key("c");
  PB::Metrics::Metric *m = l3->add_value();
  m->set_key("val");
  m->mutable_gauge_value()->set_value(99.0);

  store_.set(msg);

  const auto result = store_.get("");
  ASSERT_EQ(1u, result.size());
  EXPECT_NE(result.end(), result.find("a.b.c.val"));
}

// ============================================================================
// Multiple payloads / multiple bundles
// ============================================================================
TEST_F(MetricsStoreTest, MultipleBundlesInSinglePayload) {
  PB::Metrics::MetricsMessage msg;
  PB::Metrics::MetricsMessage::Response *resp = msg.add_payload();

  PB::Metrics::MetricsBundle *b1 = resp->add_bundles();
  b1->set_key("cpu");
  PB::Metrics::Metric *m1 = b1->add_value();
  m1->set_key("load");
  m1->mutable_gauge_value()->set_value(10.0);

  PB::Metrics::MetricsBundle *b2 = resp->add_bundles();
  b2->set_key("mem");
  PB::Metrics::Metric *m2 = b2->add_value();
  m2->set_key("free");
  m2->mutable_gauge_value()->set_value(2048.0);

  store_.set(msg);

  const auto result = store_.get("");
  EXPECT_EQ(2u, result.size());
  EXPECT_NE(result.end(), result.find("cpu.load"));
  EXPECT_NE(result.end(), result.find("mem.free"));
}

TEST_F(MetricsStoreTest, MultiplePayloads) {
  PB::Metrics::MetricsMessage msg;

  PB::Metrics::MetricsMessage::Response *resp1 = msg.add_payload();
  PB::Metrics::MetricsBundle *b1 = resp1->add_bundles();
  b1->set_key("cpu");
  PB::Metrics::Metric *m1 = b1->add_value();
  m1->set_key("load");
  m1->mutable_gauge_value()->set_value(5.0);

  PB::Metrics::MetricsMessage::Response *resp2 = msg.add_payload();
  PB::Metrics::MetricsBundle *b2 = resp2->add_bundles();
  b2->set_key("disk");
  PB::Metrics::Metric *m2 = b2->add_value();
  m2->set_key("usage");
  m2->mutable_gauge_value()->set_value(80.0);

  store_.set(msg);

  const auto result = store_.get("");
  EXPECT_EQ(2u, result.size());
  EXPECT_NE(result.end(), result.find("cpu.load"));
  EXPECT_NE(result.end(), result.find("disk.usage"));
}

// ============================================================================
// Empty message edge cases
// ============================================================================
TEST_F(MetricsStoreTest, SetEmptyMessageClearsStore) {
  store_.set(make_gauge_message("cpu", {{"load", 50.0}}));
  EXPECT_EQ(1u, store_.get("").size());

  // Set with empty message → store should be empty
  PB::Metrics::MetricsMessage empty_msg;
  store_.set(empty_msg);
  EXPECT_TRUE(store_.get("").empty());
}

TEST_F(MetricsStoreTest, SetMessageWithEmptyPayloadClearsStore) {
  store_.set(make_gauge_message("cpu", {{"load", 50.0}}));

  PB::Metrics::MetricsMessage msg;
  msg.add_payload();  // payload with no bundles
  store_.set(msg);
  EXPECT_TRUE(store_.get("").empty());
}

TEST_F(MetricsStoreTest, BundleWithNoValueProducesNoEntries) {
  PB::Metrics::MetricsMessage msg;
  PB::Metrics::MetricsMessage::Response *resp = msg.add_payload();
  PB::Metrics::MetricsBundle *b = resp->add_bundles();
  b->set_key("empty_bundle");
  // no values added

  store_.set(msg);
  EXPECT_TRUE(store_.get("").empty());
}

// ============================================================================
// Mixed gauge and string values in the same bundle
// ============================================================================
TEST_F(MetricsStoreTest, MixedGaugeAndStringValues) {
  PB::Metrics::MetricsMessage msg;
  PB::Metrics::MetricsMessage::Response *resp = msg.add_payload();
  PB::Metrics::MetricsBundle *b = resp->add_bundles();
  b->set_key("app");

  PB::Metrics::Metric *m1 = b->add_value();
  m1->set_key("requests");
  m1->mutable_gauge_value()->set_value(100.0);

  PB::Metrics::Metric *m2 = b->add_value();
  m2->set_key("status");
  m2->mutable_string_value()->set_value("ok");

  store_.set(msg);

  const auto result = store_.get("");
  EXPECT_EQ(2u, result.size());
  EXPECT_EQ("100", result.at("app.requests"));
  EXPECT_EQ("ok", result.at("app.status"));
}

// ============================================================================
// Metric with neither gauge nor string value is ignored
// ============================================================================
TEST_F(MetricsStoreTest, MetricWithNoValueIsIgnored) {
  PB::Metrics::MetricsMessage msg;
  PB::Metrics::MetricsMessage::Response *resp = msg.add_payload();
  PB::Metrics::MetricsBundle *b = resp->add_bundles();
  b->set_key("test");

  PB::Metrics::Metric *m = b->add_value();
  m->set_key("empty_metric");
  // No gauge_value or string_value set

  store_.set(msg);
  EXPECT_TRUE(store_.get("").empty());
}

// ============================================================================
// Filter matches key of the bundle path, not just metric name
// ============================================================================
TEST_F(MetricsStoreTest, FilterMatchesBundleKeyInPath) {
  store_.set(make_gauge_message("system", {{"load", 1.0}}));

  // "system" is part of the key path "system.load"
  const auto result = store_.get("system");
  EXPECT_EQ(1u, result.size());
  EXPECT_NE(result.end(), result.find("system.load"));
}

// ============================================================================
// Consecutive set() calls are independent
// ============================================================================
TEST_F(MetricsStoreTest, ConsecutiveSetCallsAreIndependent) {
  store_.set(make_gauge_message("a", {{"x", 1.0}}));
  EXPECT_EQ(1u, store_.get("").size());
  EXPECT_NE(store_.get("").end(), store_.get("").find("a.x"));

  store_.set(make_gauge_message("b", {{"y", 2.0}}));
  const auto result = store_.get("");
  EXPECT_EQ(1u, result.size());
  EXPECT_EQ(result.end(), result.find("a.x"));
  EXPECT_NE(result.end(), result.find("b.y"));
}

// ============================================================================
// Thread safety: concurrent set() and get() should not crash
// ============================================================================
TEST_F(MetricsStoreTest, ConcurrentSetAndGetDoNotCrash) {
  int iterations = 100;

  std::thread writer([this, iterations]() {
    for (int i = 0; i < iterations; ++i) {
      store_.set(make_gauge_message("thread", {{"counter", static_cast<double>(i)}}));
    }
  });

  std::thread reader([this, iterations]() {
    for (int i = 0; i < iterations; ++i) {
      const auto result = store_.get("");
      // Just exercise the code path; the result may vary
      (void)result;
    }
  });

  writer.join();
  reader.join();

  // If we get here without crashing or deadlocking, the test passes
  const auto result = store_.get("");
  EXPECT_FALSE(result.empty());
}

TEST_F(MetricsStoreTest, ConcurrentSetCallsDoNotCrash) {
  int iterations = 50;

  std::thread t1([this, iterations]() {
    for (int i = 0; i < iterations; ++i) {
      store_.set(make_gauge_message("t1", {{"val", static_cast<double>(i)}}));
    }
  });

  std::thread t2([this, iterations]() {
    for (int i = 0; i < iterations; ++i) {
      store_.set(make_gauge_message("t2", {{"val", static_cast<double>(i)}}));
    }
  });

  t1.join();
  t2.join();

  // Store should contain exactly one set of values (the last set() wins)
  const auto result = store_.get("");
  EXPECT_EQ(1u, result.size());
}

// ============================================================================
// Gauge value formatting edge cases
// ============================================================================
TEST_F(MetricsStoreTest, GaugeValueZero) {
  store_.set(make_gauge_message("test", {{"zero", 0.0}}));
  const auto result = store_.get("");
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("0", result.at("test.zero"));
}

TEST_F(MetricsStoreTest, GaugeValueNegative) {
  store_.set(make_gauge_message("test", {{"negative", -42.5}}));
  const auto result = store_.get("");
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("-42.5", result.at("test.negative"));
}

TEST_F(MetricsStoreTest, GaugeValueFractional) {
  store_.set(make_gauge_message("test", {{"frac", 3.14}}));
  const auto result = store_.get("");
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("3.14", result.at("test.frac"));
}

// ============================================================================
// String value edge cases
// ============================================================================
TEST_F(MetricsStoreTest, StringValueEmpty) {
  store_.set(make_string_message("test", {{"empty", ""}}));
  const auto result = store_.get("");
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("", result.at("test.empty"));
}

TEST_F(MetricsStoreTest, StringValueWithSpecialCharacters) {
  store_.set(make_string_message("test", {{"special", "hello world!@#$%"}}));
  const auto result = store_.get("");
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("hello world!@#$%", result.at("test.special"));
}

// ============================================================================
// Children and values at the same level
// ============================================================================
TEST_F(MetricsStoreTest, BundleWithBothChildrenAndValues) {
  PB::Metrics::MetricsMessage msg;
  PB::Metrics::MetricsMessage::Response *resp = msg.add_payload();

  PB::Metrics::MetricsBundle *parent = resp->add_bundles();
  parent->set_key("system");

  // Value directly on parent
  PB::Metrics::Metric *m1 = parent->add_value();
  m1->set_key("uptime");
  m1->mutable_gauge_value()->set_value(86400.0);

  // Child bundle with its own value
  PB::Metrics::MetricsBundle *child = parent->add_children();
  child->set_key("cpu");
  PB::Metrics::Metric *m2 = child->add_value();
  m2->set_key("load");
  m2->mutable_gauge_value()->set_value(25.0);

  store_.set(msg);

  const auto result = store_.get("");
  EXPECT_EQ(2u, result.size());
  EXPECT_NE(result.end(), result.find("system.uptime"));
  EXPECT_NE(result.end(), result.find("system.cpu.load"));
}

