// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// How a module declares a metric.
//
// The builder is the one choke point every producer goes through, so what it
// writes into the protobuf is what the OpenMetrics endpoint, the JSON views,
// Graphite, collectd and the Python bridge all read. A field it forgets to set
// is a metric that scrapes as an anonymous number; a field it sets that the old
// `add_metric` did not is a change to a wire format four consumers parse.

#include <gtest/gtest.h>

#include <nscapi/nscapi_metrics_helper.hpp>
#include <string>

namespace {

const PB::Metrics::Metric &only(const PB::Metrics::MetricsBundle &bundle) {
  EXPECT_EQ(bundle.value_size(), 1);
  return bundle.value(0);
}

}  // namespace

// --- the shorthand ----------------------------------------------------------

TEST(MetricsHelper, AddMetricStillWritesNothingButAKeyAndAGauge) {
  // Out-of-tree modules call this, and a metric they declared before the
  // metadata existed must keep producing exactly the same bytes - in
  // particular it must not gain an invented description.
  PB::Metrics::MetricsBundle bundle;
  nscapi::metrics::add_metric(&bundle, "used", 42ll);

  const PB::Metrics::Metric &m = only(bundle);
  EXPECT_EQ(m.key(), "used");
  EXPECT_TRUE(m.has_gauge_value());
  EXPECT_EQ(m.gauge_value().value(), 42.0);
  EXPECT_EQ(m.desc(), "");
  EXPECT_EQ(m.unit(), "");
  EXPECT_EQ(m.alias(), "");
  EXPECT_EQ(m.dims_size(), 0);
}

TEST(MetricsHelper, AddMetricKeepsItsFourOverloads) {
  PB::Metrics::MetricsBundle bundle;
  nscapi::metrics::add_metric(&bundle, "signed", -7ll);
  nscapi::metrics::add_metric(&bundle, "unsigned", static_cast<unsigned long long>(17175158784ull));
  nscapi::metrics::add_metric(&bundle, "real", 0.5);
  nscapi::metrics::add_metric(&bundle, "text", std::string("1d 12:30"));

  ASSERT_EQ(bundle.value_size(), 4);
  EXPECT_EQ(bundle.value(0).gauge_value().value(), -7.0);
  EXPECT_EQ(bundle.value(1).gauge_value().value(), 17175158784.0);
  EXPECT_EQ(bundle.value(2).gauge_value().value(), 0.5);
  EXPECT_TRUE(bundle.value(3).has_string_value());
  EXPECT_EQ(bundle.value(3).string_value().value(), "1d 12:30");
}

// --- the builder ------------------------------------------------------------

TEST(MetricsHelper, TheBuilderWritesTheKeyHelpUnitAndValue) {
  PB::Metrics::MetricsBundle bundle;
  nscapi::metrics::metric(&bundle, "physical.used").help("Physical memory in use").unit("bytes").gauge(17175158784ll);

  const PB::Metrics::Metric &m = only(bundle);
  EXPECT_EQ(m.key(), "physical.used");
  EXPECT_EQ(m.desc(), "Physical memory in use");
  EXPECT_EQ(m.unit(), "bytes");
  EXPECT_TRUE(m.has_gauge_value());
  EXPECT_EQ(m.gauge_value().value(), 17175158784.0);
}

TEST(MetricsHelper, TheOrderOfTheMetadataCallsDoesNotMatter) {
  PB::Metrics::MetricsBundle a;
  PB::Metrics::MetricsBundle b;
  nscapi::metrics::metric(&a, "k").help("h").unit("bytes").gauge(1);
  nscapi::metrics::metric(&b, "k").unit("bytes").help("h").gauge(1);

  EXPECT_EQ(a.SerializeAsString(), b.SerializeAsString());
}

TEST(MetricsHelper, EachTerminalCallPicksTheType) {
  // Which member of the oneof is set *is* the metric's type, for the
  // OpenMetrics renderer and for collectd's DERIVE mapping alike.
  PB::Metrics::MetricsBundle bundle;
  nscapi::metrics::metric(&bundle, "gauge").gauge(1);
  nscapi::metrics::metric(&bundle, "counter").counter(2);
  nscapi::metrics::metric(&bundle, "untyped").untyped(3);
  nscapi::metrics::metric(&bundle, "info").info("up");

  ASSERT_EQ(bundle.value_size(), 4);
  EXPECT_TRUE(bundle.value(0).has_gauge_value());
  EXPECT_TRUE(bundle.value(1).has_counter_value());
  EXPECT_EQ(bundle.value(1).counter_value().value(), 2.0);
  EXPECT_TRUE(bundle.value(2).has_untyped_value());
  EXPECT_TRUE(bundle.value(3).has_string_value());
  EXPECT_EQ(bundle.value(3).string_value().value(), "up");
}

TEST(MetricsHelper, TheBuilderTakesWhateverArithmeticTypeTheProducerHas) {
  // The producers hand over ints, longs, size_ts, unsigned long longs and
  // doubles straight out of the APIs they read; needing a cast at every call
  // site is how a sweep of two hundred of them goes wrong.
  PB::Metrics::MetricsBundle bundle;
  nscapi::metrics::metric(&bundle, "int").gauge(1);
  nscapi::metrics::metric(&bundle, "size").gauge(static_cast<std::size_t>(2));
  nscapi::metrics::metric(&bundle, "ull").gauge(static_cast<unsigned long long>(3));
  nscapi::metrics::metric(&bundle, "double").gauge(4.5);

  ASSERT_EQ(bundle.value_size(), 4);
  EXPECT_EQ(bundle.value(3).gauge_value().value(), 4.5);
}

TEST(MetricsHelper, AMetricThatDeclaresNoMetadataCarriesNone) {
  // An empty help or unit must stay unset rather than become an empty string:
  // the renderer decides whether to emit a `# HELP` line by asking whether
  // there is one.
  PB::Metrics::MetricsBundle bundle;
  nscapi::metrics::metric(&bundle, "k").gauge(1);

  EXPECT_EQ(only(bundle).desc(), "");
  EXPECT_EQ(only(bundle).unit(), "");
}

TEST(MetricsHelper, NothingIsAppendedUntilTheValueIsGiven) {
  // The builder holds the metadata and appends on the terminal call, so a
  // producer that builds one and drops it cannot leave a keyed metric with no
  // value in the snapshot.
  PB::Metrics::MetricsBundle bundle;
  {
    nscapi::metrics::metric_builder abandoned(&bundle, "never");
  }
  EXPECT_EQ(bundle.value_size(), 0);

  nscapi::metrics::metric(&bundle, "given").gauge(1);
  EXPECT_EQ(bundle.value_size(), 1);
}

TEST(MetricsHelper, DescribeSetsTheBundlesFallbackHelp) {
  PB::Metrics::MetricsBundle bundle;
  nscapi::metrics::describe(&bundle, "Memory as reported by the kernel");

  EXPECT_EQ(bundle.desc(), "Memory as reported by the kernel");
}

// --- reading a metric back --------------------------------------------------

TEST(MetricsHelper, NumericValueReadsEveryNumericType) {
  // The JSON views, Graphite, Elastic and the Python dict forward numbers and
  // do not care about the type. Before this they read `gauge_value` alone, so
  // typing a metric as a counter would have made it vanish from all four.
  PB::Metrics::MetricsBundle bundle;
  nscapi::metrics::metric(&bundle, "gauge").gauge(1.5);
  nscapi::metrics::metric(&bundle, "counter").counter(2.5);
  nscapi::metrics::metric(&bundle, "untyped").untyped(3.5);

  double value = 0;
  ASSERT_TRUE(nscapi::metrics::numeric_value(bundle.value(0), value));
  EXPECT_EQ(value, 1.5);
  ASSERT_TRUE(nscapi::metrics::numeric_value(bundle.value(1), value));
  EXPECT_EQ(value, 2.5);
  ASSERT_TRUE(nscapi::metrics::numeric_value(bundle.value(2), value));
  EXPECT_EQ(value, 3.5);
}

TEST(MetricsHelper, NumericValueRefusesAStringAndLeavesTheOutputAlone) {
  PB::Metrics::MetricsBundle bundle;
  nscapi::metrics::metric(&bundle, "info").info("1d 12:30");

  double value = 99;
  EXPECT_FALSE(nscapi::metrics::numeric_value(only(bundle), value));
  EXPECT_EQ(value, 99);
}

TEST(MetricsHelper, NumericValueRefusesAMetricWithNoValueAtAll) {
  // A metric a producer keyed but never valued, and the aggregate types, which
  // have no single number to hand over.
  PB::Metrics::MetricsBundle bundle;
  bundle.add_value()->set_key("empty");
  bundle.add_value()->mutable_summary_value()->set_sample_count(3);

  double value = 99;
  EXPECT_FALSE(nscapi::metrics::numeric_value(bundle.value(0), value));
  EXPECT_FALSE(nscapi::metrics::numeric_value(bundle.value(1), value));
  EXPECT_EQ(value, 99);
}
