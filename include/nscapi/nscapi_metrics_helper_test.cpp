// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The metric builder, pinned on the one property the whole design rests on:
// a labelled metric writes the same `key` the old concatenation wrote.
//
// Everything downstream of the protobuf except the OpenMetrics renderer reads
// `key` and nothing else - the flat and nested JSON endpoints, the web UI's
// dashboard, Graphite's carbon path, collectd, and the dict handed to a Python
// `submit_metrics` callback. If a builder call moves a key, every one of those
// moves with it, silently, on somebody's existing dashboard. So the tests below
// spell out the key each call produces rather than asserting that it is
// "reasonable".

#include <gtest/gtest.h>

#include <nscapi/nscapi_metrics_helper.hpp>

#include <string>

using namespace nscapi::metrics;

namespace {

const PB::Metrics::Metric &only(const PB::Metrics::MetricsBundle &b) {
  EXPECT_EQ(b.value_size(), 1);
  return b.value(0);
}

}  // namespace

// --- keys -------------------------------------------------------------------

TEST(MetricBuilder, AMetricWithNoInstanceKeepsItsNameAsTheKey) {
  PB::Metrics::MetricsBundle b;
  metric(&b, "refresh_interval").gauge(1ll);

  EXPECT_EQ(only(b).key(), "refresh_interval");
  EXPECT_EQ(only(b).gauge_value().value(), 1.0);
}

TEST(MetricBuilder, AnInstanceIsPrependedExactlyAsConcatenationDidIt) {
  // `add_metric(cpu, name + ".idle", ...)`, spelled as a builder call. The
  // dotted key is what the web UI's metric parser splits on, so this is the
  // assertion that says the dashboard did not move.
  PB::Metrics::MetricsBundle b;
  metric(&b, "idle").instance("core 0").label("core", "0").gauge(95.0);

  EXPECT_EQ(only(b).key(), "core 0.idle");
}

TEST(MetricBuilder, AnEmptyInstanceLeavesTheKeyAlone) {
  // Windows publishes a single unnamed battery under a bare key, having built
  // the prefix as `name.empty() ? "" : name + "."`. An empty instance has to
  // mean that, not a leading dot.
  PB::Metrics::MetricsBundle b;
  metric(&b, "charge_percent").instance("").label("battery", "").gauge(80ll);

  EXPECT_EQ(only(b).key(), "charge_percent");
  EXPECT_EQ(only(b).dims_size(), 0);
}

TEST(MetricBuilder, AProducerCanSpellTheKeyOutWhenTheInstanceIsNotAPrefix) {
  // A PDH counter publishes `pdh.<counter>.<instance>` - the instance last -
  // so the key cannot be composed from the family name and the instance.
  PB::Metrics::MetricsBundle b;
  metric(&b, "pdh.\\Processor(*)\\% Idle Time").key("pdh.\\Processor(*)\\% Idle Time._Total").label("instance", "_Total").gauge(95ll);

  EXPECT_EQ(only(b).key(), "pdh.\\Processor(*)\\% Idle Time._Total");
  EXPECT_EQ(only(b).alias(), "pdh.\\Processor(*)\\% Idle Time");
}

TEST(MetricBuilder, TheLastOfInstanceAndKeyWins) {
  PB::Metrics::MetricsBundle b;
  metric(&b, "idle").instance("core 0").key("spelled.out").label("core", "0").gauge(1.0);

  EXPECT_EQ(only(b).key(), "spelled.out");
}

// --- labels -----------------------------------------------------------------

TEST(MetricBuilder, LabelsAreRecordedInTheOrderTheyWereAdded) {
  // The renderer emits them in this order, and a stable order is what stops a
  // scraper seeing the same series renamed between two scrapes.
  PB::Metrics::MetricsBundle b;
  metric(&b, "sent").instance("eth0").label("nic", "eth0").label("mac", "00:11:22").gauge(343ll);

  const PB::Metrics::Metric &m = only(b);
  ASSERT_EQ(m.dims_size(), 2);
  EXPECT_EQ(m.dims(0).key(), "nic");
  EXPECT_EQ(m.dims(0).value(), "eth0");
  EXPECT_EQ(m.dims(1).key(), "mac");
  EXPECT_EQ(m.dims(1).value(), "00:11:22");
}

TEST(MetricBuilder, TheAliasIsTheFamilyNameWithTheInstanceTakenBackOut) {
  PB::Metrics::MetricsBundle b;
  metric(&b, "idle").instance("core 0").label("core", "0").gauge(95.0);

  EXPECT_EQ(only(b).alias(), "idle");
}

TEST(MetricBuilder, NoLabelsMeansNoAlias) {
  // The two travel together on purpose: an alias names the family that several
  // labelled samples share, and without labels to tell those samples apart a
  // renderer reading it would collapse them into one series.
  PB::Metrics::MetricsBundle b;
  metric(&b, "idle").instance("core 0").gauge(95.0);

  EXPECT_EQ(only(b).key(), "core 0.idle");
  EXPECT_EQ(only(b).alias(), "");
}

TEST(MetricBuilder, AnEmptyLabelKeyOrValueIsDropped) {
  // `x=""` and an absent `x` are the same series in OpenMetrics, so a sample
  // carrying an empty label would silently collide with one that has it
  // filled in. Dropping it here lets a producer pass a field that is sometimes
  // missing - a NIC with no MAC, a zone with no label file - unguarded.
  PB::Metrics::MetricsBundle b;
  metric(&b, "temperature").instance("acpitz").label("zone", "acpitz").label("label", "").label("", "x").gauge(42ll);

  const PB::Metrics::Metric &m = only(b);
  ASSERT_EQ(m.dims_size(), 1);
  EXPECT_EQ(m.dims(0).key(), "zone");
}

// --- values -----------------------------------------------------------------

TEST(MetricBuilder, EachTerminatorWritesItsOwnOneofMember) {
  PB::Metrics::MetricsBundle b;
  metric(&b, "a").gauge(1.5);
  metric(&b, "b").gauge(2ll);
  metric(&b, "c").gauge(static_cast<unsigned long long>(3));
  metric(&b, "d").info("text");

  ASSERT_EQ(b.value_size(), 4);
  EXPECT_TRUE(b.value(0).has_gauge_value());
  EXPECT_EQ(b.value(0).gauge_value().value(), 1.5);
  EXPECT_TRUE(b.value(1).has_gauge_value());
  EXPECT_EQ(b.value(1).gauge_value().value(), 2.0);
  EXPECT_TRUE(b.value(2).has_gauge_value());
  EXPECT_EQ(b.value(2).gauge_value().value(), 3.0);
  EXPECT_TRUE(b.value(3).has_string_value());
  EXPECT_EQ(b.value(3).string_value().value(), "text");
}

TEST(MetricBuilder, ABuilderThatIsNeverTerminatedAddsNothing) {
  // The metric is written by the terminator, not by the constructor, so an
  // abandoned expression cannot leave a keyless, valueless metric in the
  // bundle for the renderer to trip over.
  PB::Metrics::MetricsBundle b;
  metric(&b, "idle").instance("core 0").label("core", "0");

  EXPECT_EQ(b.value_size(), 0);
}

// --- the add_metric shorthand -----------------------------------------------

TEST(MetricBuilder, TheAddMetricOverloadsAreUnchanged) {
  // Out-of-tree modules call these, and a producer with nothing to say about
  // dimensions should not have to reach for a builder to say so.
  PB::Metrics::MetricsBundle b;
  add_metric(&b, "a", 1ll);
  add_metric(&b, "b", static_cast<unsigned long long>(2));
  add_metric(&b, "c", std::string("three"));
  add_metric(&b, "d", 4.5);

  ASSERT_EQ(b.value_size(), 4);
  for (const PB::Metrics::Metric &m : b.value()) {
    EXPECT_EQ(m.alias(), "");
    EXPECT_EQ(m.dims_size(), 0);
  }
  EXPECT_EQ(b.value(0).gauge_value().value(), 1.0);
  EXPECT_EQ(b.value(2).string_value().value(), "three");
  EXPECT_EQ(b.value(3).gauge_value().value(), 4.5);
}

// --- the core label ---------------------------------------------------------

TEST(MetricBuilder, CoreLabelReadsTheSameOnBothPlatformsSpellings) {
  // Linux normalises the CPU-load key to `core_0` before publishing it and
  // Windows leaves it as `core 0`. That difference is a fact about keys
  // dashboards already read, and it must not reach the label, where it would
  // make one core look like two depending on which host reported it.
  EXPECT_EQ(core_label("core 0"), "0");
  EXPECT_EQ(core_label("core_0"), "0");
  EXPECT_EQ(core_label("core 15"), "15");
}

TEST(MetricBuilder, CoreLabelKeepsTheAggregateAsItsOwnValue) {
  // `core="total"` mirrors the JSON key and keeps `sum by (core)` honest; the
  // cost is that `sum without (core)` double-counts, which the reference
  // documentation calls out rather than the code papering over.
  EXPECT_EQ(core_label("total"), "total");
}

TEST(MetricBuilder, CoreLabelLeavesAnythingItDoesNotRecogniseAlone) {
  // Better a label value that reads oddly than one that has been truncated by
  // a prefix rule guessing at a key shape it has never seen.
  EXPECT_EQ(core_label("core"), "core");
  EXPECT_EQ(core_label("cpu0"), "cpu0");
  EXPECT_EQ(core_label("corexyz"), "corexyz");
  EXPECT_EQ(core_label(""), "");
  // And a separator with nothing after it stays whole: an empty label value is
  // dropped, and a sample missing the label that tells it apart from its
  // siblings collides with them.
  EXPECT_EQ(core_label("core_"), "core_");
}
