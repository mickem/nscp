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

// --- instances and labels ---------------------------------------------------
//
// The property all of these circle: a labelled metric writes the same `key` the
// concatenation it replaces wrote. Everything downstream of the protobuf except
// the OpenMetrics renderer reads `key` and nothing else, so a builder call that
// moved one would move a dashboard on every upgraded host, silently. The tests
// spell out the key each call produces rather than asserting it is reasonable.

TEST(MetricsHelper, AnInstanceIsPrependedExactlyAsConcatenationDidIt) {
  // `metric(cpu, name + ".idle")`, spelled as a builder call. The dotted key is
  // what the web UI's metric parser splits on and what Graphite turns into a
  // carbon path.
  PB::Metrics::MetricsBundle bundle;
  nscapi::metrics::metric(&bundle, "idle").instance("core 0").label("core", "0").gauge(95.0);

  EXPECT_EQ(only(bundle).key(), "core 0.idle");
}

TEST(MetricsHelper, TheAliasIsTheFamilyNameWithTheInstanceTakenBackOut) {
  PB::Metrics::MetricsBundle bundle;
  nscapi::metrics::metric(&bundle, "idle").instance("core 0").label("core", "0").gauge(95.0);

  const PB::Metrics::Metric &m = only(bundle);
  EXPECT_EQ(m.alias(), "idle");
  ASSERT_EQ(m.dims_size(), 1);
  EXPECT_EQ(m.dims(0).key(), "core");
  EXPECT_EQ(m.dims(0).value(), "0");
}

TEST(MetricsHelper, NoLabelsMeansNoAlias) {
  // The two travel together on purpose: an alias names the family that several
  // labelled samples share, and the renderer reading it without labels to tell
  // those samples apart would collapse every instance into one series.
  PB::Metrics::MetricsBundle bundle;
  nscapi::metrics::metric(&bundle, "idle").instance("core 0").gauge(95.0);

  EXPECT_EQ(only(bundle).key(), "core 0.idle");
  EXPECT_EQ(only(bundle).alias(), "");
}

TEST(MetricsHelper, AnEmptyInstanceLeavesTheKeyAlone) {
  // Windows publishes a single unnamed battery under a bare key, having built
  // the prefix as `name.empty() ? "" : name + "."`. An empty instance has to
  // mean that, not a leading dot - and the empty label that would come with it
  // is dropped rather than written.
  PB::Metrics::MetricsBundle bundle;
  nscapi::metrics::metric(&bundle, "charge_percent").instance("").label("battery", "").gauge(80ll);

  EXPECT_EQ(only(bundle).key(), "charge_percent");
  EXPECT_EQ(only(bundle).dims_size(), 0);
  EXPECT_EQ(only(bundle).alias(), "");
}

TEST(MetricsHelper, AProducerCanSpellTheKeyOutWhenTheInstanceIsNotAPrefix) {
  // A PDH counter publishes `pdh.<counter>.<instance>` - the instance last - so
  // the key cannot be composed from the family name and the instance.
  PB::Metrics::MetricsBundle bundle;
  nscapi::metrics::metric(&bundle, "pdh.\\Processor(*)\\% Idle Time").key("pdh.\\Processor(*)\\% Idle Time._Total").label("instance", "_Total").gauge(95ll);

  const PB::Metrics::Metric &m = only(bundle);
  EXPECT_EQ(m.key(), "pdh.\\Processor(*)\\% Idle Time._Total");
  EXPECT_EQ(m.alias(), "pdh.\\Processor(*)\\% Idle Time");
}

TEST(MetricsHelper, TheLastOfInstanceAndKeyWins) {
  PB::Metrics::MetricsBundle bundle;
  nscapi::metrics::metric(&bundle, "idle").instance("core 0").key("spelled.out").label("core", "0").gauge(1.0);

  EXPECT_EQ(only(bundle).key(), "spelled.out");
}

TEST(MetricsHelper, LabelsAreRecordedInTheOrderTheyWereAdded) {
  // The renderer emits them in this order, and a stable order is what keeps a
  // scraper from seeing the same series renamed between two scrapes.
  PB::Metrics::MetricsBundle bundle;
  nscapi::metrics::metric(&bundle, "sent").instance("eth0").label("nic", "eth0").label("kind", "physical").gauge(343ll);

  const PB::Metrics::Metric &m = only(bundle);
  ASSERT_EQ(m.dims_size(), 2);
  EXPECT_EQ(m.dims(0).key(), "nic");
  EXPECT_EQ(m.dims(1).key(), "kind");
}

TEST(MetricsHelper, AnEmptyLabelKeyOrValueIsDropped) {
  // `x=""` and an absent `x` are the same series in OpenMetrics, so a sample
  // carrying an empty label would silently collide with one that has it filled
  // in. Dropping it here lets a producer pass a field that is sometimes missing
  // - a NIC with no MAC, a zone with no label file - without guarding the call.
  PB::Metrics::MetricsBundle bundle;
  nscapi::metrics::metric(&bundle, "temperature").instance("acpitz").label("zone", "acpitz").label("label", "").label("", "x").gauge(42ll);

  const PB::Metrics::Metric &m = only(bundle);
  ASSERT_EQ(m.dims_size(), 1);
  EXPECT_EQ(m.dims(0).key(), "zone");
}

TEST(MetricsHelper, LabelsComposeWithTheMetadataAndEveryTerminator) {
  // The instance and the dimension are orthogonal to help, unit and type: a
  // per-core counter declares all of it in one expression.
  PB::Metrics::MetricsBundle bundle;
  nscapi::metrics::metric(&bundle, "times_seen").instance("nscp").label("exe", "nscp").help("Samples seen in").unit("seconds").counter(7ll);

  const PB::Metrics::Metric &m = only(bundle);
  EXPECT_EQ(m.key(), "nscp.times_seen");
  EXPECT_EQ(m.alias(), "times_seen");
  EXPECT_EQ(m.desc(), "Samples seen in");
  EXPECT_EQ(m.unit(), "seconds");
  EXPECT_TRUE(m.has_counter_value());
  ASSERT_EQ(m.dims_size(), 1);
  EXPECT_EQ(m.dims(0).value(), "nscp");
}

TEST(MetricsHelper, AStringMetricCarriesItsInstanceLabelsToo) {
  // Strings become labels of their bundle's `_info` family, and which series of
  // that family they land on is decided by exactly these dimensions - so one
  // NIC's MAC address and link state share a line and the next NIC's do not.
  PB::Metrics::MetricsBundle bundle;
  nscapi::metrics::metric(&bundle, "status").instance("eth0").label("nic", "eth0").info("up");

  const PB::Metrics::Metric &m = only(bundle);
  EXPECT_EQ(m.key(), "eth0.status");
  EXPECT_EQ(m.alias(), "status");
  EXPECT_TRUE(m.has_string_value());
  ASSERT_EQ(m.dims_size(), 1);
  EXPECT_EQ(m.dims(0).key(), "nic");
}

TEST(MetricsHelper, NothingIsAppendedUntilAnInstancedMetricIsValuedEither) {
  PB::Metrics::MetricsBundle bundle;
  nscapi::metrics::metric(&bundle, "idle").instance("core 0").label("core", "0");

  EXPECT_EQ(bundle.value_size(), 0);
}

// --- the core label ---------------------------------------------------------

TEST(MetricsHelper, CoreLabelReadsTheSameOnBothPlatformsSpellings) {
  // Linux normalises the CPU-load key to `core_0` before publishing it and
  // Windows leaves it as `core 0`. That difference is a fact about keys that
  // dashboards already read, and it must not reach the label, where it would
  // make one core look like two depending on which host reported it.
  EXPECT_EQ(nscapi::metrics::core_label("core 0"), "0");
  EXPECT_EQ(nscapi::metrics::core_label("core_0"), "0");
  EXPECT_EQ(nscapi::metrics::core_label("core 15"), "15");
}

TEST(MetricsHelper, CoreLabelKeepsTheAggregateAsItsOwnValue) {
  // `core="total"` mirrors the JSON key and keeps `sum by (core)` honest; the
  // cost is that `sum without (core)` double-counts, which the reference
  // documentation calls out rather than the code papering over.
  EXPECT_EQ(nscapi::metrics::core_label("total"), "total");
}

TEST(MetricsHelper, CoreLabelLeavesAnythingItDoesNotRecogniseAlone) {
  // Better a label value that reads oddly than one truncated by a prefix rule
  // guessing at a key shape it has never seen.
  EXPECT_EQ(nscapi::metrics::core_label("core"), "core");
  EXPECT_EQ(nscapi::metrics::core_label("cpu0"), "cpu0");
  EXPECT_EQ(nscapi::metrics::core_label("corexyz"), "corexyz");
  EXPECT_EQ(nscapi::metrics::core_label(""), "");
  // A separator with nothing after it stays whole: an empty label value is
  // dropped, and a sample missing the label that tells it apart from its
  // siblings collides with them.
  EXPECT_EQ(nscapi::metrics::core_label("core_"), "core_");
}

// --- the per-instance scope -------------------------------------------------

TEST(MetricsHelper, AnInstanceScopeWritesTheSameThingTheLongFormDoes) {
  // The whole point is that saying the dimension once is not a different
  // metric from saying it on every line - so the two forms are compared, not
  // just spot-checked.
  PB::Metrics::MetricsBundle scoped;
  const nscapi::metrics::instance_scope disk = nscapi::metrics::for_instance(&scoped, "sda", "disk");
  disk.metric("reads_per_sec").help("Read operations per second").gauge(7ll);

  PB::Metrics::MetricsBundle spelled_out;
  nscapi::metrics::metric(&spelled_out, "reads_per_sec").instance("sda").label("disk", "sda").help("Read operations per second").gauge(7ll);

  EXPECT_EQ(only(scoped).SerializeAsString(), only(spelled_out).SerializeAsString());
  EXPECT_EQ(only(scoped).key(), "sda.reads_per_sec");
  EXPECT_EQ(only(scoped).alias(), "reads_per_sec");
}

TEST(MetricsHelper, AnInstanceScopeIsReusableAndEachMetricIsItsOwn) {
  // One scope per loop iteration, many metrics from it: a builder is one
  // metric, but the scope that hands them out is not consumed by doing so.
  PB::Metrics::MetricsBundle bundle;
  const nscapi::metrics::instance_scope nic = nscapi::metrics::for_instance(&bundle, "eth0", "nic");
  nic.metric("received").gauge(1ll);
  nic.metric("sent").unit("bytes").gauge(2ll);

  ASSERT_EQ(bundle.value_size(), 2);
  EXPECT_EQ(bundle.value(0).key(), "eth0.received");
  EXPECT_EQ(bundle.value(1).key(), "eth0.sent");
  // The unit of the second must not have leaked back onto the first.
  EXPECT_EQ(bundle.value(0).unit(), "");
  EXPECT_EQ(bundle.value(1).unit(), "bytes");
  for (const PB::Metrics::Metric &m : bundle.value()) {
    ASSERT_EQ(m.dims_size(), 1);
    EXPECT_EQ(m.dims(0).key(), "nic");
    EXPECT_EQ(m.dims(0).value(), "eth0");
  }
}

TEST(MetricsHelper, AScopesLabelValueNeedNotBeItsKeyPrefix) {
  // Two producers need this. A CPU-load key is `core 0` where the label is the
  // bare `0`; a Windows processor's key is its model name, which is the same
  // string on every socket, where the dimension that tells two sockets apart
  // is the device id. Labelling either with its key prefix would be wrong in
  // opposite ways - one inconsistent across platforms, one duplicated across
  // sockets.
  PB::Metrics::MetricsBundle bundle;
  const nscapi::metrics::instance_scope cpu = nscapi::metrics::for_instance(&bundle, "Intel(R) Xeon(R) Gold 6248R", "cpu", "CPU1");
  cpu.metric("current_mhz").gauge(2400ll);

  EXPECT_EQ(only(bundle).key(), "Intel(R) Xeon(R) Gold 6248R.current_mhz");
  EXPECT_EQ(only(bundle).alias(), "current_mhz");
  ASSERT_EQ(only(bundle).dims_size(), 1);
  EXPECT_EQ(only(bundle).dims(0).value(), "CPU1");
}

TEST(MetricsHelper, TwoSocketsOfOneModelAreTwoSeries) {
  // The multi-socket case in full: `Win32_Processor.Name` is the model string,
  // identical on both sockets, so a scope labelled with it would publish the
  // same series twice and the renderer would drop the second one from every
  // scrape. Keyed on the model and labelled on the device id, they are two.
  PB::Metrics::MetricsBundle bundle;
  const std::string model = "Intel(R) Xeon(R) Gold 6248R";
  nscapi::metrics::for_instance(&bundle, model, "cpu", "CPU0").metric("current_mhz").gauge(2400ll);
  nscapi::metrics::for_instance(&bundle, model, "cpu", "CPU1").metric("current_mhz").gauge(2500ll);

  ASSERT_EQ(bundle.value_size(), 2);
  EXPECT_EQ(bundle.value(0).key(), bundle.value(1).key());
  EXPECT_NE(bundle.value(0).dims(0).value(), bundle.value(1).dims(0).value());
}

TEST(MetricsHelper, AnUnnamedInstanceScopeStillPublishesABareKey) {
  // A single unnamed battery: no key prefix and no label, which is the same
  // thing the long form does with an empty instance.
  PB::Metrics::MetricsBundle bundle;
  nscapi::metrics::for_instance(&bundle, "", "battery").metric("charge_percent").gauge(80ll);

  EXPECT_EQ(only(bundle).key(), "charge_percent");
  EXPECT_EQ(only(bundle).dims_size(), 0);
  EXPECT_EQ(only(bundle).alias(), "");
}
