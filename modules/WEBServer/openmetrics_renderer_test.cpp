// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The OpenMetrics exposition served on /api/v2/openmetrics.
//
// What the endpoint emitted before this renderer existed was three lines in
// WEBServer.cpp appending `<bundle>_<key> <value>`, which is not a document
// any strict parser accepts: names carried `.`, `%`, spaces and colons, there
// was no `# TYPE` and no `# EOF`, and `str::xtos` truncated every value to six
// significant digits, so a 16 GB memory reading scraped as `1.6554e+10`.
//
// These tests pin the grammar rather than a golden body: the name rules, the
// precision, the family grouping and the terminator are what a scraper depends
// on, and each of them is a regression that would otherwise only show up as a
// silently dropped series on somebody's Prometheus.

#include "openmetrics_renderer.hpp"

#include <gtest/gtest.h>

#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace {

// Builds a snapshot the way the core does: one response, one or more bundles.
class snapshot {
 public:
  snapshot() : payload_(message_.add_payload()) {}

  PB::Metrics::MetricsBundle *bundle(const std::string &key) {
    PB::Metrics::MetricsBundle *b = payload_->add_bundles();
    b->set_key(key);
    return b;
  }

  static PB::Metrics::MetricsBundle *child(PB::Metrics::MetricsBundle *parent, const std::string &key) {
    PB::Metrics::MetricsBundle *b = parent->add_children();
    b->set_key(key);
    return b;
  }

  static void gauge(PB::Metrics::MetricsBundle *b, const std::string &key, const double value) {
    PB::Metrics::Metric *m = b->add_value();
    m->set_key(key);
    m->mutable_gauge_value()->set_value(value);
  }

  // A metric as `nscapi::metrics::metric(b, name).instance(i).label(...)`
  // builds it: the key still carries the instance, and `alias` + `dims` say
  // which family it belongs to and what tells it apart from its siblings.
  typedef std::vector<std::pair<std::string, std::string> > labels_type;
  static void labelled_gauge(PB::Metrics::MetricsBundle *b, const std::string &name, const std::string &instance, const labels_type &labels,
                             const double value) {
    PB::Metrics::Metric *m = b->add_value();
    m->set_key(instance.empty() ? name : instance + "." + name);
    m->set_alias(name);
    for (const labels_type::value_type &l : labels) {
      PB::Common::KeyValue *dim = m->add_dims();
      dim->set_key(l.first);
      dim->set_value(l.second);
    }
    m->mutable_gauge_value()->set_value(value);
  }

  static void string_metric(PB::Metrics::MetricsBundle *b, const std::string &key, const std::string &value) {
    PB::Metrics::Metric *m = b->add_value();
    m->set_key(key);
    m->mutable_string_value()->set_value(value);
  }

  const PB::Metrics::MetricsMessage &message() const { return message_; }

 private:
  PB::Metrics::MetricsMessage message_;
  PB::Metrics::MetricsMessage::Response *payload_;
};

bool contains(const std::string &haystack, const std::string &needle) { return haystack.find(needle) != std::string::npos; }

// One label pair, so the call sites below read as a label set rather than as
// nested brace initialisers.
std::pair<std::string, std::string> l(const std::string &key, const std::string &value) { return std::make_pair(key, value); }

}  // namespace

// --- names ------------------------------------------------------------------

TEST(OpenmetricsRenderer, SanitizeRewritesEveryCharacterOutsideTheGrammar) {
  // A metric name is `[a-zA-Z_][a-zA-Z0-9_]*`. Everything the producers
  // actually emit that falls outside it, in one table.
  EXPECT_EQ(openmetrics::sanitize_name("system_cpu_total.idle"), "system_cpu_total_idle");
  EXPECT_EQ(openmetrics::sanitize_name("system_cpu_core 0.idle"), "system_cpu_core_0_idle");
  EXPECT_EQ(openmetrics::sanitize_name("disk_free_C:.total"), "disk_free_C_total");
  EXPECT_EQ(openmetrics::sanitize_name("system_process_history_notepad.exe.times_seen"), "system_process_history_notepad_exe_times_seen");
  EXPECT_EQ(openmetrics::sanitize_name("system_network_Intel(R) Ethernet.sent"), "system_network_Intel_R_Ethernet_sent");
}

TEST(OpenmetricsRenderer, PercentBecomesTheWordAndNotAnUnderscore) {
  // The documentation has shown `system_mem_physical_percent` since forever;
  // the code emitted `system_mem_physical.%`. Collapsing `%` to `_` instead
  // would give `system_mem_physical_` - a different name from the documented
  // one, and an ugly one.
  EXPECT_EQ(openmetrics::sanitize_name("system_mem_physical.%"), "system_mem_physical_percent");
  EXPECT_EQ(openmetrics::sanitize_name("%"), "percent");
  EXPECT_EQ(openmetrics::sanitize_name("a.%.b"), "a_percent_b");
}

TEST(OpenmetricsRenderer, RunsOfIllegalCharactersCollapseToOneUnderscore) {
  // Otherwise `disk.io.` + `C:` would render `disk_io__C_`, and two keys that
  // differ only in how many separators they use would become two families.
  EXPECT_EQ(openmetrics::sanitize_name("a...b"), "a_b");
  EXPECT_EQ(openmetrics::sanitize_name("a - b"), "a_b");
  EXPECT_EQ(openmetrics::sanitize_name("a_-_b"), "a_b");
  // An underscore is a separator too, not a character to keep alongside a
  // substituted one: `C:` joined to `_total` must not give `C__total`.
  EXPECT_EQ(openmetrics::sanitize_name("a__b"), "a_b");
}

TEST(OpenmetricsRenderer, ColonsAreRewrittenBecauseTheyAreReservedForRecordingRules) {
  // `:` is legal in the grammar but reserved for user-defined recording rules,
  // so an exporter must not emit it. `C:` drives are the live case.
  EXPECT_EQ(openmetrics::sanitize_name("disk_free_C:_total"), "disk_free_C_total");
  EXPECT_FALSE(contains(openmetrics::sanitize_name("a:b:c"), ":"));
}

TEST(OpenmetricsRenderer, ANameThatWouldNotStartWithALetterBorrowsOne) {
  // A PDH counter or a Python script can name a metric anything at all. The
  // obvious `_` prefix is not the fix: a leading underscore is legal in the
  // name grammar but OpenMetrics reserves every name that begins with one, so
  // it would only trade one non-conformance for another.
  EXPECT_EQ(openmetrics::sanitize_name("5m_load"), "metric_5m_load");
  EXPECT_EQ(openmetrics::sanitize_name("0"), "metric_0");
  EXPECT_EQ(openmetrics::sanitize_name(".leading"), "metric_leading");
}

TEST(OpenmetricsRenderer, ANameThatSanitisesAwayEntirelyStaysANameAtAll) {
  // An empty name is not a valid sample line, so it must never be emitted -
  // even for a key that is nothing but punctuation.
  EXPECT_EQ(openmetrics::sanitize_name(""), "metric");
  EXPECT_EQ(openmetrics::sanitize_name("..."), "metric");
}

TEST(OpenmetricsRenderer, NoNameEverBeginsWithAnUnderscore) {
  // The reserved-prefix rule, swept rather than spot-checked: whatever a
  // producer hands us, the name it lands on is usable.
  const char *keys[] = {"", ".", "___", "5", "%", "_x", ":x", " x", "..5m", "0.5"};
  for (const char *key : keys) {
    const std::string name = openmetrics::sanitize_name(key);
    ASSERT_FALSE(name.empty()) << key;
    EXPECT_NE(name[0], '_') << "reserved leading underscore for key: " << key;
    EXPECT_TRUE((name[0] >= 'a' && name[0] <= 'z') || (name[0] >= 'A' && name[0] <= 'Z')) << key;
  }
}

TEST(OpenmetricsRenderer, SanitisingIsDeterministic) {
  // The same input always maps to the same name: a scraper that sees a family
  // rename between two scrapes loses the series' history.
  EXPECT_EQ(openmetrics::sanitize_name("system.mem.commited.%"), openmetrics::sanitize_name("system.mem.commited.%"));
}

// --- values -----------------------------------------------------------------

TEST(OpenmetricsRenderer, IntegralValuesKeepEveryDigit) {
  // The whole reason the renderer exists: `str::xtos` is six significant
  // digits, so 16 GB of memory scraped as `1.6554e+10` and a byte counter was
  // rounded to the nearest 100 KB.
  EXPECT_EQ(openmetrics::render_value(16554000000.0), "16554000000");
  EXPECT_EQ(openmetrics::render_value(12592123904.0), "12592123904");
  EXPECT_EQ(openmetrics::render_value(255000000000.0), "255000000000");
  EXPECT_EQ(openmetrics::render_value(0.0), "0");
  EXPECT_EQ(openmetrics::render_value(-1.0), "-1");
}

TEST(OpenmetricsRenderer, FractionalValuesRoundTripWithoutGrowingDigits) {
  // Shortest representation that still reads back as the same double, so a
  // percentage stays readable instead of becoming 0.10000000000000001.
  EXPECT_EQ(openmetrics::render_value(0.1), "0.1");
  EXPECT_EQ(openmetrics::render_value(97.8293), "97.8293");
  EXPECT_EQ(openmetrics::render_value(0.975472), "0.975472");
}

TEST(OpenmetricsRenderer, NonFiniteValuesUseTheSpecSpelling) {
  // A collector that has not sampled yet can hand us a NaN; `nan` and `inf`
  // (what a stringstream writes) are not what the grammar accepts.
  EXPECT_EQ(openmetrics::render_value(std::numeric_limits<double>::quiet_NaN()), "NaN");
  EXPECT_EQ(openmetrics::render_value(std::numeric_limits<double>::infinity()), "+Inf");
  EXPECT_EQ(openmetrics::render_value(-std::numeric_limits<double>::infinity()), "-Inf");
}

// --- documents --------------------------------------------------------------

TEST(OpenmetricsRenderer, AnEmptySnapshotIsStillAValidDocument) {
  // A scrape before the first metrics tick must not return a body a parser
  // rejects - `# EOF` is what makes an empty document well-formed.
  const PB::Metrics::MetricsMessage empty;
  EXPECT_EQ(openmetrics::render(empty), "# EOF\n");
}

TEST(OpenmetricsRenderer, EveryFamilyCarriesATypeAndTheBodyEndsWithEof) {
  snapshot s;
  PB::Metrics::MetricsBundle *system = s.bundle("system");
  PB::Metrics::MetricsBundle *mem = snapshot::child(system, "mem");
  snapshot::gauge(mem, "physical.total", 17175158784.0);
  snapshot::gauge(mem, "physical.%", 73.0);

  const std::string body = openmetrics::render(s.message());

  EXPECT_EQ(body,
            "# TYPE system_mem_physical_total gauge\n"
            "system_mem_physical_total 17175158784\n"
            "# TYPE system_mem_physical_percent gauge\n"
            "system_mem_physical_percent 73\n"
            "# EOF\n");
}

TEST(OpenmetricsRenderer, NestedBundlesBecomeTheNamePrefix) {
  snapshot s;
  PB::Metrics::MetricsBundle *system = s.bundle("system");
  PB::Metrics::MetricsBundle *cpu = snapshot::child(system, "cpu");
  snapshot::gauge(cpu, "core_0.idle", 95.0);
  PB::Metrics::MetricsBundle *disk = s.bundle("disk");
  snapshot::gauge(disk, "free.C:.total", 255000000000.0);

  const std::string body = openmetrics::render(s.message());

  EXPECT_TRUE(contains(body, "system_cpu_core_0_idle 95\n"));
  EXPECT_TRUE(contains(body, "disk_free_C_total 255000000000\n"));
}

TEST(OpenmetricsRenderer, StringMetricsAreSkipped) {
  // Uptime, boot time and MAC addresses have no numeric sample. They need an
  // `info` family and the metadata that goes with it; until then they must not
  // leak out as a bare name with no value, which would break the whole body.
  snapshot s;
  PB::Metrics::MetricsBundle *up = s.bundle("uptime");
  snapshot::string_metric(up, "uptime", "1d 12:30");
  snapshot::gauge(up, "ticks.raw", 84135.0);

  const std::string body = openmetrics::render(s.message());

  EXPECT_FALSE(contains(body, "1d 12:30"));
  EXPECT_TRUE(contains(body, "uptime_ticks_raw 84135\n"));
}

TEST(OpenmetricsRenderer, EachFamilyIsEmittedOnceUnderItsOwnType) {
  // Every family is one `# TYPE` followed by its sample. Repeating a `# TYPE`
  // for a family, or letting a sample appear after another family started,
  // makes the document invalid - which is what the first-seen ordered family
  // list exists to prevent.
  snapshot s;
  PB::Metrics::MetricsBundle *a = s.bundle("dup");
  snapshot::gauge(a, "value", 1.0);
  PB::Metrics::MetricsBundle *b = s.bundle("other");
  snapshot::gauge(b, "value", 2.0);

  const std::string body = openmetrics::render(s.message());

  EXPECT_EQ(body,
            "# TYPE dup_value gauge\n"
            "dup_value 1\n"
            "# TYPE other_value gauge\n"
            "other_value 2\n"
            "# EOF\n");
}

TEST(OpenmetricsRenderer, TwoKeysCollidingOnOneNameKeepTheFirstAndAreReported) {
  // Sanitising is lossy: `mem.%` and `mem.percent` are different keys in the
  // JSON view and the same family here. Emitting both would be the same series
  // twice, which a strict parser rejects for the whole scrape - so the second
  // is dropped and the operator is told which metric went missing and why.
  snapshot s;
  PB::Metrics::MetricsBundle *mem = s.bundle("mem");
  snapshot::gauge(mem, "used.%", 73.0);
  snapshot::gauge(mem, "used percent", 99.0);

  std::vector<std::string> problems;
  const std::string body = openmetrics::render(s.message(), &problems);

  EXPECT_EQ(body,
            "# TYPE mem_used_percent gauge\n"
            "mem_used_percent 73\n"
            "# EOF\n");
  ASSERT_EQ(problems.size(), 1u);
  EXPECT_TRUE(contains(problems[0], "mem.used percent"));
  EXPECT_TRUE(contains(problems[0], "mem_used_percent"));
}

TEST(OpenmetricsRenderer, ChildBundlesAreRenderedBeforeTheParentsOwnValues) {
  // Matches the order the JSON walk uses, so the two views list the same
  // metrics in the same order and a diff of the endpoints stays readable.
  snapshot s;
  PB::Metrics::MetricsBundle *system = s.bundle("system");
  snapshot::gauge(system, "refresh_interval", 1.0);
  PB::Metrics::MetricsBundle *cpu = snapshot::child(system, "cpu");
  snapshot::gauge(cpu, "total.idle", 95.0);

  const std::string body = openmetrics::render(s.message());

  EXPECT_LT(body.find("system_cpu_total_idle"), body.find("system_refresh_interval"));
}

TEST(OpenmetricsRenderer, EveryEmittedLineMatchesTheExpositionGrammar) {
  // A walking check over a snapshot shaped like a real host, so a future change
  // that introduces some other malformed line fails here rather than on
  // somebody's scrape.
  snapshot s;
  PB::Metrics::MetricsBundle *system = s.bundle("system");
  PB::Metrics::MetricsBundle *cpu = snapshot::child(system, "cpu");
  snapshot::gauge(cpu, "core 0.idle", 95.0);
  snapshot::gauge(cpu, "total.idle", 91.5);
  PB::Metrics::MetricsBundle *net = snapshot::child(system, "network");
  snapshot::gauge(net, "Ethernet 1.BytesReceivedPersec", 343.0);
  PB::Metrics::MetricsBundle *disk = s.bundle("disk");
  snapshot::gauge(disk, "free.C:.total", 255000000000.0);
  snapshot::string_metric(disk, "free.C:.label", "System");

  const std::string body = openmetrics::render(s.message());

  ASSERT_GE(body.size(), 6u);
  EXPECT_EQ(body.substr(body.size() - 6), "# EOF\n");

  size_t line_start = 0;
  int samples = 0;
  while (line_start < body.size()) {
    const size_t line_end = body.find('\n', line_start);
    ASSERT_NE(line_end, std::string::npos) << "the body must not end mid-line";
    const std::string line = body.substr(line_start, line_end - line_start);
    line_start = line_end + 1;
    if (!line.empty() && line[0] == '#') continue;

    const size_t space = line.find(' ');
    ASSERT_NE(space, std::string::npos) << "sample line without a value: " << line;
    const std::string name = line.substr(0, space);
    const std::string value = line.substr(space + 1);
    ASSERT_FALSE(name.empty()) << line;
    EXPECT_TRUE((name[0] >= 'a' && name[0] <= 'z') || (name[0] >= 'A' && name[0] <= 'Z') || name[0] == '_') << "name starts illegally: " << line;
    for (const char c : name) {
      EXPECT_TRUE((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') << "illegal character in name: " << line;
    }
    EXPECT_EQ(value.find(' '), std::string::npos) << "value carries a space: " << line;
    EXPECT_FALSE(value.empty()) << line;
    ++samples;
  }
  EXPECT_EQ(samples, 4);
}


// --- labels -----------------------------------------------------------------

TEST(OpenmetricsRenderer, InstancesOfOneMetricBecomeSamplesOfOneFamily) {
  // The point of the whole exercise. Before labels, four cores meant four
  // families named after the core, so `sum by (core)` had nothing to sum, a
  // Grafana variable had no label to bind to, and the family names differed
  // between two hosts with different core counts.
  snapshot s;
  PB::Metrics::MetricsBundle *cpu = snapshot::child(s.bundle("system"), "cpu");
  snapshot::labelled_gauge(cpu, "idle", "core 0", {l("core", "0")}, 95.0);
  snapshot::labelled_gauge(cpu, "idle", "core 1", {l("core", "1")}, 91.0);
  snapshot::labelled_gauge(cpu, "idle", "total", {l("core", "total")}, 93.0);

  EXPECT_EQ(openmetrics::render(s.message()),
            "# TYPE system_cpu_idle gauge\n"
            "system_cpu_idle{core=\"0\"} 95\n"
            "system_cpu_idle{core=\"1\"} 91\n"
            "system_cpu_idle{core=\"total\"} 93\n"
            "# EOF\n");
}

TEST(OpenmetricsRenderer, TheSamplesOfAFamilyStayContiguousWhenProducersInterleave) {
  // Two metrics per core, emitted core by core, is what the producer loops
  // actually do. A sample that lands after another family has started belongs
  // to no `# TYPE` at all, so the families have to be collected before
  // anything is written - which is why the renderer does not stream.
  snapshot s;
  PB::Metrics::MetricsBundle *cpu = snapshot::child(s.bundle("system"), "cpu");
  snapshot::labelled_gauge(cpu, "idle", "core 0", {l("core", "0")}, 95.0);
  snapshot::labelled_gauge(cpu, "user", "core 0", {l("core", "0")}, 3.0);
  snapshot::labelled_gauge(cpu, "idle", "core 1", {l("core", "1")}, 91.0);
  snapshot::labelled_gauge(cpu, "user", "core 1", {l("core", "1")}, 7.0);

  EXPECT_EQ(openmetrics::render(s.message()),
            "# TYPE system_cpu_idle gauge\n"
            "system_cpu_idle{core=\"0\"} 95\n"
            "system_cpu_idle{core=\"1\"} 91\n"
            "# TYPE system_cpu_user gauge\n"
            "system_cpu_user{core=\"0\"} 3\n"
            "system_cpu_user{core=\"1\"} 7\n"
            "# EOF\n");
}

TEST(OpenmetricsRenderer, LabelsAreEmittedInTheOrderTheProducerAddedThem) {
  // Stable, not sorted: a series whose label order changes between two scrapes
  // reads as a different series to some tooling, and the builder already
  // preserves insertion order, so the renderer must not reshuffle it.
  snapshot s;
  PB::Metrics::MetricsBundle *net = snapshot::child(s.bundle("system"), "network");
  snapshot::labelled_gauge(net, "sent", "eth0", {l("nic", "eth0"), l("mac", "00:11:22"), l("kind", "physical")}, 1.0);

  EXPECT_TRUE(contains(openmetrics::render(s.message()), "system_network_sent{nic=\"eth0\",mac=\"00:11:22\",kind=\"physical\"} 1\n"));
}

TEST(OpenmetricsRenderer, LabelValuesAreEscapedRatherThanSanitised) {
  // A label value is free text - unlike a name, nothing about it has to be
  // rewritten, only escaped. All three escapes turn up in real values: a
  // Windows volume reads `\Device\HarddiskVolume1`, and a WMI adapter
  // description can carry a quote.
  snapshot s;
  PB::Metrics::MetricsBundle *disk = s.bundle("disk");
  snapshot::labelled_gauge(disk, "free", "vol", {l("drive", "\\Device\\HarddiskVolume1")}, 1.0);
  snapshot::labelled_gauge(disk, "free", "quoted", {l("drive", "say \"hi\"")}, 2.0);
  snapshot::labelled_gauge(disk, "free", "multi", {l("drive", "two\nlines")}, 3.0);

  const std::string body = openmetrics::render(s.message());

  EXPECT_TRUE(contains(body, "disk_free{drive=\"\\\\Device\\\\HarddiskVolume1\"} 1\n"));
  EXPECT_TRUE(contains(body, "disk_free{drive=\"say \\\"hi\\\"\"} 2\n"));
  EXPECT_TRUE(contains(body, "disk_free{drive=\"two\\nlines\"} 3\n"));
  // The escaped newline must not have become an actual line break, which would
  // split one sample into two the parser cannot read.
  EXPECT_FALSE(contains(body, "two\nlines"));
}

TEST(OpenmetricsRenderer, LabelNamesAreMappedOntoTheGrammarTheSameWayNamesAre) {
  // Producers use fixed, already-legal names. What needs cleaning is the
  // operator-defined end: a PDH counter's dimension, or the `labels` dict a
  // Python script returns.
  EXPECT_EQ(openmetrics::sanitize_label_name("core"), "core");
  EXPECT_EQ(openmetrics::sanitize_label_name("disk io"), "disk_io");
  EXPECT_EQ(openmetrics::sanitize_label_name("5m"), "label_5m");
  EXPECT_EQ(openmetrics::sanitize_label_name(""), "label");
  // A leading underscore is legal in a label name but `__` is reserved, and
  // borrowing a letter is one rule rather than two.
  EXPECT_EQ(openmetrics::sanitize_label_name("__reserved"), "label_reserved");
}

TEST(OpenmetricsRenderer, ALabelNameThatNeedsCleaningIsCleanedInTheBody) {
  snapshot s;
  PB::Metrics::MetricsBundle *b = s.bundle("system");
  snapshot::labelled_gauge(b, "value", "x", {l("disk io", "sda")}, 1.0);

  EXPECT_TRUE(contains(openmetrics::render(s.message()), "system_value{disk_io=\"sda\"} 1\n"));
}

TEST(OpenmetricsRenderer, TwoLabelsCollapsingToOneNameKeepTheFirst) {
  // Nobody writes the same label twice, but two raw names can sanitise to one
  // - and a label name repeated within a sample makes the line invalid, so the
  // duplicate goes rather than the sample.
  snapshot s;
  PB::Metrics::MetricsBundle *b = s.bundle("system");
  snapshot::labelled_gauge(b, "value", "x", {l("disk io", "sda"), l("disk.io", "sdb")}, 1.0);

  EXPECT_EQ(openmetrics::render(s.message()),
            "# TYPE system_value gauge\n"
            "system_value{disk_io=\"sda\"} 1\n"
            "# EOF\n");
}

TEST(OpenmetricsRenderer, AnEmptyLabelValueIsDroppedRatherThanEmitted) {
  // `x=""` and an absent `x` are the same series to a scraper, so emitting one
  // would make two samples the producer meant to keep apart collide - and the
  // collision would drop a metric rather than just render it oddly.
  snapshot s;
  PB::Metrics::MetricsBundle *b = s.bundle("system");
  snapshot::labelled_gauge(b, "value", "x", {l("zone", "acpitz"), l("label", "")}, 1.0);

  EXPECT_TRUE(contains(openmetrics::render(s.message()), "system_value{zone=\"acpitz\"} 1\n"));
}

TEST(OpenmetricsRenderer, OneFamilyMayHoldSamplesWithDifferentLabelSets) {
  // Not something the built-in producers do, but valid OpenMetrics and
  // reachable from a Python script, so it must render rather than drop: the
  // two samples are different series, which is the only thing that matters.
  snapshot s;
  PB::Metrics::MetricsBundle *b = s.bundle("system");
  snapshot::labelled_gauge(b, "value", "a", {l("kind", "a")}, 1.0);
  snapshot::labelled_gauge(b, "value", "b", {l("kind", "b"), l("extra", "yes")}, 2.0);

  EXPECT_EQ(openmetrics::render(s.message()),
            "# TYPE system_value gauge\n"
            "system_value{kind=\"a\"} 1\n"
            "system_value{kind=\"b\",extra=\"yes\"} 2\n"
            "# EOF\n");
}

TEST(OpenmetricsRenderer, LabelOrderDoesNotMakeASecondSeries) {
  // `{a="1",b="2"}` and `{b="2",a="1"}` are one series to a scraper even though
  // the two strings differ, so the duplicate check has to ignore order - or the
  // body ships the same series twice and a strict parser rejects all of it.
  snapshot s;
  PB::Metrics::MetricsBundle *b = s.bundle("system");
  snapshot::labelled_gauge(b, "value", "a", {l("x", "1"), l("y", "2")}, 1.0);
  snapshot::labelled_gauge(b, "value", "b", {l("y", "2"), l("x", "1")}, 2.0);

  std::vector<std::string> problems;
  EXPECT_EQ(openmetrics::render(s.message(), &problems),
            "# TYPE system_value gauge\n"
            "system_value{x=\"1\",y=\"2\"} 1\n"
            "# EOF\n");
  EXPECT_EQ(problems.size(), 1u);
}

TEST(OpenmetricsRenderer, TheSameSeriesTwiceIsDroppedAndReported) {
  // Two instances whose names sanitise to nothing distinguishable would do
  // this; so would a producer looping over a list with a duplicate in it.
  snapshot s;
  PB::Metrics::MetricsBundle *cpu = snapshot::child(s.bundle("system"), "cpu");
  snapshot::labelled_gauge(cpu, "idle", "core 0", {l("core", "0")}, 95.0);
  snapshot::labelled_gauge(cpu, "idle", "core 0", {l("core", "0")}, 42.0);

  std::vector<std::string> problems;
  const std::string body = openmetrics::render(s.message(), &problems);

  EXPECT_EQ(body,
            "# TYPE system_cpu_idle gauge\n"
            "system_cpu_idle{core=\"0\"} 95\n"
            "# EOF\n");
  ASSERT_EQ(problems.size(), 1u);
  EXPECT_TRUE(contains(problems[0], "system_cpu_idle{core=\"0\"}"));
  EXPECT_TRUE(contains(problems[0], "with the same labels"));
}

TEST(OpenmetricsRenderer, AMetricWithoutDimsStillRendersFromItsKey) {
  // An out-of-tree module, or a producer this sweep did not touch, calls
  // `add_metric` and sets neither `alias` nor `dims`. Its family name is the
  // key, exactly as before labels existed.
  snapshot s;
  PB::Metrics::MetricsBundle *cpu = snapshot::child(s.bundle("system"), "cpu");
  snapshot::gauge(cpu, "core 0.idle", 95.0);

  EXPECT_EQ(openmetrics::render(s.message()),
            "# TYPE system_cpu_core_0_idle gauge\n"
            "system_cpu_core_0_idle 95\n"
            "# EOF\n");
}

TEST(OpenmetricsRenderer, AnAliasWithoutDimsIsIgnored) {
  // The builder writes the two together, so this shape only reaches the
  // renderer from a hand-rolled producer or an older plugin. Honouring the
  // alias on its own would merge every instance into one series, each
  // overwriting the last; falling back to the key keeps them apart.
  snapshot s;
  PB::Metrics::MetricsBundle *cpu = snapshot::child(s.bundle("system"), "cpu");
  PB::Metrics::Metric *m = cpu->add_value();
  m->set_key("core 0.idle");
  m->set_alias("idle");
  m->mutable_gauge_value()->set_value(95.0);

  EXPECT_TRUE(contains(openmetrics::render(s.message()), "system_cpu_core_0_idle 95\n"));
}

TEST(OpenmetricsRenderer, LabelledAndUnlabelledMetricsCoexistInOneBody) {
  // Which is the state of the world for at least one release: the swept
  // producers label their per-instance metrics, everything else does not.
  snapshot s;
  PB::Metrics::MetricsBundle *system = s.bundle("system");
  snapshot::gauge(system, "refresh_interval", 1.0);
  PB::Metrics::MetricsBundle *cpu = snapshot::child(system, "cpu");
  snapshot::labelled_gauge(cpu, "idle", "core 0", {l("core", "0")}, 95.0);

  EXPECT_EQ(openmetrics::render(s.message()),
            "# TYPE system_cpu_idle gauge\n"
            "system_cpu_idle{core=\"0\"} 95\n"
            "# TYPE system_refresh_interval gauge\n"
            "system_refresh_interval 1\n"
            "# EOF\n");
}

TEST(OpenmetricsRenderer, LegacyModeIsUnmovedByLabels) {
  // The switch exists to reproduce the pre-renderer bytes, and the pre-renderer
  // body was built from keys alone. Since a labelled metric keeps its key, the
  // legacy body is identical whether or not the producer was swept - which is
  // the property that makes the sweep safe to ship.
  snapshot s;
  PB::Metrics::MetricsBundle *cpu = snapshot::child(s.bundle("system"), "cpu");
  snapshot::labelled_gauge(cpu, "idle", "core 0", {l("core", "0")}, 95.0);

  snapshot before;
  PB::Metrics::MetricsBundle *old_cpu = snapshot::child(before.bundle("system"), "cpu");
  snapshot::gauge(old_cpu, "core 0.idle", 95.0);

  EXPECT_EQ(openmetrics::render_legacy(s.message()), openmetrics::render_legacy(before.message()));
  EXPECT_EQ(openmetrics::render_legacy(s.message()), "system_cpu_core 0.idle 95\n");
}

TEST(OpenmetricsRenderer, EveryLabelledLineMatchesTheExpositionGrammar) {
  // The grammar walk from the unlabelled case, extended over a snapshot shaped
  // like a real host after the sweep - so a future change that produces some
  // other malformed label fails here rather than on somebody's scrape.
  snapshot s;
  PB::Metrics::MetricsBundle *system = s.bundle("system");
  PB::Metrics::MetricsBundle *cpu = snapshot::child(system, "cpu");
  snapshot::labelled_gauge(cpu, "idle", "core 0", {l("core", "0")}, 95.0);
  snapshot::labelled_gauge(cpu, "idle", "total", {l("core", "total")}, 91.5);
  PB::Metrics::MetricsBundle *net = snapshot::child(system, "network");
  snapshot::labelled_gauge(net, "BytesReceivedPersec", "Intel(R) Ethernet #2", {l("nic", "Intel(R) Ethernet #2")}, 343.0);
  PB::Metrics::MetricsBundle *disk = s.bundle("disk");
  snapshot::labelled_gauge(disk, "total", "C:", {l("drive", "C:")}, 255000000000.0);

  const std::string body = openmetrics::render(s.message());

  size_t line_start = 0;
  int samples = 0;
  while (line_start < body.size()) {
    const size_t line_end = body.find('\n', line_start);
    ASSERT_NE(line_end, std::string::npos);
    const std::string line = body.substr(line_start, line_end - line_start);
    line_start = line_end + 1;
    if (!line.empty() && line[0] == '#') continue;

    // `name{labels} value`, with the labels optional. The name is checked the
    // same way as in the unlabelled walk; what is new is that everything
    // between the braces has to be well-formed too.
    const size_t brace = line.find('{');
    const std::string name = line.substr(0, brace == std::string::npos ? line.find(' ') : brace);
    ASSERT_FALSE(name.empty()) << line;
    for (const char c : name) {
      EXPECT_TRUE((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') << "illegal character in name: " << line;
    }
    if (brace != std::string::npos) {
      const size_t close = line.rfind("} ");
      ASSERT_NE(close, std::string::npos) << "label set never closed: " << line;
      const std::string labels = line.substr(brace + 1, close - brace - 1);
      ASSERT_FALSE(labels.empty()) << "an empty label set must not be emitted: " << line;
      // Every label name is legal, and every value is quoted with no
      // unescaped quote inside it.
      size_t at = 0;
      while (at < labels.size()) {
        const size_t eq = labels.find('=', at);
        ASSERT_NE(eq, std::string::npos) << line;
        const std::string label = labels.substr(at, eq - at);
        ASSERT_FALSE(label.empty()) << line;
        EXPECT_TRUE((label[0] >= 'a' && label[0] <= 'z') || (label[0] >= 'A' && label[0] <= 'Z') || label[0] == '_') << "label starts illegally: " << line;
        for (const char c : label) {
          EXPECT_TRUE((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') << "illegal character in label: " << line;
        }
        ASSERT_EQ(labels[eq + 1], '"') << line;
        // Walk to the closing quote, stepping over every escaped character.
        size_t at_value = eq + 2;
        while (at_value < labels.size() && labels[at_value] != '"') at_value += labels[at_value] == '\\' ? 2 : 1;
        ASSERT_LT(at_value, labels.size()) << "unterminated label value: " << line;
        at = at_value + 1;
        if (at < labels.size()) {
          ASSERT_EQ(labels[at], ',') << line;
          ++at;
        }
      }
    }
    const std::string value = line.substr(line.rfind(' ') + 1);
    EXPECT_FALSE(value.empty()) << line;
    ++samples;
  }
  EXPECT_EQ(samples, 4);
}

// --- the legacy escape hatch ------------------------------------------------

TEST(OpenmetricsRenderer, LegacyModeReproducesThePreviousBodyByteForByte) {
  // The captured shape of what `build_metrics` appended: names pasted in
  // verbatim, `str::xtos` precision, strings skipped, no metadata at all. This
  // is what a dashboard built before the change is still reading.
  snapshot s;
  PB::Metrics::MetricsBundle *system = s.bundle("system");
  PB::Metrics::MetricsBundle *mem = snapshot::child(system, "mem");
  snapshot::gauge(mem, "commited.avail", 12592123904.0);
  snapshot::gauge(mem, "commited.%", 73.0);
  snapshot::string_metric(mem, "commited.note", "skipped");
  PB::Metrics::MetricsBundle *cpu = snapshot::child(system, "cpu");
  snapshot::gauge(cpu, "core 0.idle", 95.0);

  EXPECT_EQ(openmetrics::render_legacy(s.message()),
            "system_mem_commited.avail 1.25921e+10\n"
            "system_mem_commited.% 73\n"
            "system_cpu_core 0.idle 95\n");
}

TEST(OpenmetricsRenderer, LegacyModeEmitsNoTerminator) {
  // Deliberately: the old body had no `# EOF`, and adding one would change the
  // bytes the switch exists to preserve.
  const PB::Metrics::MetricsMessage empty;
  EXPECT_EQ(openmetrics::render_legacy(empty), "");
}

// --- content negotiation ----------------------------------------------------

TEST(OpenmetricsRenderer, AScraperAskingForOpenmetricsIsToldItGotOpenmetrics) {
  EXPECT_EQ(openmetrics::content_type_for("application/openmetrics-text;version=1.0.0;q=0.75,text/plain;version=0.0.4;q=0.5,*/*;q=0.1"),
            "application/openmetrics-text; version=1.0.0; charset=utf-8");
}

TEST(OpenmetricsRenderer, EveryoneElseGetsTheVersionedPrometheusTextType) {
  // Including the no-Accept case: a bare `text/plain` with no version is what
  // the endpoint used to send, and it tells a scraper nothing.
  EXPECT_EQ(openmetrics::content_type_for(""), "text/plain; version=0.0.4; charset=utf-8");
  EXPECT_EQ(openmetrics::content_type_for("*/*"), "text/plain; version=0.0.4; charset=utf-8");
  EXPECT_EQ(openmetrics::content_type_for("text/html"), "text/plain; version=0.0.4; charset=utf-8");
}
