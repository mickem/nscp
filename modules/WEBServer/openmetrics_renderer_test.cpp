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
//
// Since the metadata sweep they also pin what a producer's `help`, `unit`,
// type and labels turn into, and the one place the two expositions genuinely
// differ: OpenMetrics names a counter family `foo` and its sample `foo_total`,
// the older Prometheus text format names both `foo_total`. Rendering one body
// and serving it to both readers is what that costs - every counter loses its
// type for one of them.

#include "openmetrics_renderer.hpp"

#include <gtest/gtest.h>

#include <limits>
#include <string>
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

  static PB::Metrics::Metric *gauge(PB::Metrics::MetricsBundle *b, const std::string &key, const double value) {
    PB::Metrics::Metric *m = b->add_value();
    m->set_key(key);
    m->mutable_gauge_value()->set_value(value);
    return m;
  }

  static PB::Metrics::Metric *counter(PB::Metrics::MetricsBundle *b, const std::string &key, const double value) {
    PB::Metrics::Metric *m = b->add_value();
    m->set_key(key);
    m->mutable_counter_value()->set_value(value);
    return m;
  }

  static PB::Metrics::Metric *untyped(PB::Metrics::MetricsBundle *b, const std::string &key, const double value) {
    PB::Metrics::Metric *m = b->add_value();
    m->set_key(key);
    m->mutable_untyped_value()->set_value(value);
    return m;
  }

  static PB::Metrics::Metric *string_metric(PB::Metrics::MetricsBundle *b, const std::string &key, const std::string &value) {
    PB::Metrics::Metric *m = b->add_value();
    m->set_key(key);
    m->mutable_string_value()->set_value(value);
    return m;
  }

  static PB::Metrics::Metric *described(PB::Metrics::Metric *m, const std::string &help, const std::string &unit = "") {
    m->set_desc(help);
    if (!unit.empty()) m->set_unit(unit);
    return m;
  }

  static PB::Metrics::Metric *labelled(PB::Metrics::Metric *m, const std::string &name, const std::string &value) {
    PB::Common::KeyValue *dim = m->add_dims();
    dim->set_key(name);
    dim->set_value(value);
    return m;
  }

  const PB::Metrics::MetricsMessage &message() const { return message_; }

 private:
  PB::Metrics::MetricsMessage message_;
  PB::Metrics::MetricsMessage::Response *payload_;
};

bool contains(const std::string &haystack, const std::string &needle) { return haystack.find(needle) != std::string::npos; }

// The exposition a scraper that negotiated OpenMetrics 1.0 gets.
std::string render(const snapshot &shot, std::vector<std::string> *problems = nullptr) {
  return openmetrics::render(shot.message(), openmetrics::dialect::openmetrics_1_0, problems);
}

// The exposition everything that did not negotiate gets.
std::string render_text(const snapshot &shot) { return openmetrics::render(shot.message(), openmetrics::dialect::prometheus_text_0_0_4); }

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
  EXPECT_EQ(openmetrics::render(empty, openmetrics::dialect::openmetrics_1_0), "# EOF\n");
  EXPECT_EQ(openmetrics::render(empty, openmetrics::dialect::prometheus_text_0_0_4), "# EOF\n");
}

TEST(OpenmetricsRenderer, EveryFamilyCarriesATypeAndTheBodyEndsWithEof) {
  snapshot s;
  PB::Metrics::MetricsBundle *system = s.bundle("system");
  PB::Metrics::MetricsBundle *mem = snapshot::child(system, "mem");
  snapshot::gauge(mem, "physical.total", 17175158784.0);
  snapshot::gauge(mem, "physical.%", 73.0);

  const std::string body = render(s);

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

  const std::string body = render(s);

  EXPECT_TRUE(contains(body, "system_cpu_core_0_idle 95\n"));
  EXPECT_TRUE(contains(body, "disk_free_C_total 255000000000\n"));
}

TEST(OpenmetricsRenderer, StringMetricsBecomeTheBundlesInfoFamily) {
  // Uptime, boot time and MAC addresses have no numeric sample, so they are
  // labels of one always-1 series - the `node_uname_info` shape. Before the
  // metadata work they were dropped, and a Prometheus user could not see the
  // host's uptime string at all.
  snapshot s;
  PB::Metrics::MetricsBundle *up = s.bundle("uptime");
  snapshot::gauge(up, "ticks.raw", 84135.0);
  snapshot::string_metric(up, "uptime", "1d 12:30");
  snapshot::string_metric(up, "boot", "2026-09-13 01:15");

  const std::string body = render(s);

  EXPECT_EQ(body,
            "# TYPE uptime_ticks_raw gauge\n"
            "uptime_ticks_raw 84135\n"
            "# TYPE uptime info\n"
            "uptime_info{uptime=\"1d 12:30\",boot=\"2026-09-13 01:15\"} 1\n"
            "# EOF\n");
}

TEST(OpenmetricsRenderer, AnInfoFamilyIsAGaugeForTheOlderTextFormat) {
  // `info` arrived with OpenMetrics 1.0. The older parser rejects a `# TYPE`
  // it does not know, and rejecting it costs the scrape every metric in the
  // body, not just this one - so that reader gets the gauge-valued-1 spelling
  // exporters used before the type existed, naming the sample rather than the
  // family.
  snapshot s;
  PB::Metrics::MetricsBundle *up = s.bundle("uptime");
  snapshot::string_metric(up, "uptime", "1d 12:30");

  EXPECT_EQ(render_text(s),
            "# TYPE uptime_info gauge\n"
            "uptime_info{uptime=\"1d 12:30\"} 1\n"
            "# EOF\n");
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

  const std::string body = render(s);

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
  const std::string body = render(s, &problems);

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

  const std::string body = render(s);

  EXPECT_LT(body.find("system_cpu_total_idle"), body.find("system_refresh_interval"));
}

TEST(OpenmetricsRenderer, EveryEmittedLineMatchesTheExpositionGrammar) {
  // A walking check over a snapshot shaped like a real host, so a future change
  // that introduces some other malformed line fails here rather than on
  // somebody's scrape.
  snapshot s;
  PB::Metrics::MetricsBundle *system = s.bundle("system");
  PB::Metrics::MetricsBundle *cpu = snapshot::child(system, "cpu");
  snapshot::described(snapshot::gauge(cpu, "core 0.idle", 95.0), "Share of CPU time spent idle", "percent");
  snapshot::gauge(cpu, "total.idle", 91.5);
  PB::Metrics::MetricsBundle *net = snapshot::child(system, "network");
  snapshot::labelled(snapshot::gauge(net, "Ethernet 1.BytesReceivedPersec", 343.0), "nic", "Ethernet 1");
  PB::Metrics::MetricsBundle *disk = s.bundle("disk");
  snapshot::described(snapshot::gauge(disk, "free.C:.total", 255000000000.0), "Size of the volume", "bytes");
  snapshot::counter(disk, "free.C:.errors", 2.0);
  snapshot::string_metric(disk, "free.C:.label", "System");

  for (const openmetrics::dialect dialect : {openmetrics::dialect::openmetrics_1_0, openmetrics::dialect::prometheus_text_0_0_4}) {
    const std::string body = openmetrics::render(s.message(), dialect);

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

      // A sample is `name[{labels}] value`; a label value is quoted and may
      // hold anything, spaces included, so the name ends at the brace when
      // there is one.
      const size_t brace = line.find('{');
      const size_t close = brace == std::string::npos ? std::string::npos : line.find('}', brace);
      if (brace != std::string::npos) ASSERT_NE(close, std::string::npos) << "unterminated label set: " << line;
      const size_t space = line.find(' ', close == std::string::npos ? 0 : close);
      ASSERT_NE(space, std::string::npos) << "sample line without a value: " << line;
      const std::string name = line.substr(0, brace == std::string::npos ? space : brace);
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
    // Four gauges, one counter and the info series the string folds into.
    EXPECT_EQ(samples, 6);
  }
}

// --- metadata ---------------------------------------------------------------

TEST(OpenmetricsRenderer, HelpTextBecomesTheHelpLine) {
  snapshot s;
  PB::Metrics::MetricsBundle *mem = s.bundle("mem");
  snapshot::described(snapshot::gauge(mem, "used", 42.0), "Physical memory in use");

  EXPECT_EQ(render(s),
            "# HELP mem_used Physical memory in use\n"
            "# TYPE mem_used gauge\n"
            "mem_used 42\n"
            "# EOF\n");
}

TEST(OpenmetricsRenderer, AMetricWithNoHelpOfItsOwnInheritsTheBundles) {
  // How a section of near-identical metrics - one per core, one per NIC - gets
  // help text without every producer repeating it per metric.
  snapshot s;
  PB::Metrics::MetricsBundle *cpu = s.bundle("cpu");
  cpu->set_desc("CPU time over the last 5 minutes");
  snapshot::gauge(cpu, "core_0.idle", 95.0);
  snapshot::described(snapshot::gauge(cpu, "core_1.idle", 91.0), "Something more specific");

  const std::string body = render(s);

  EXPECT_TRUE(contains(body, "# HELP cpu_core_0_idle CPU time over the last 5 minutes\n"));
  EXPECT_TRUE(contains(body, "# HELP cpu_core_1_idle Something more specific\n"));
}

TEST(OpenmetricsRenderer, HelpTextIsEscapedSoOneLineStaysOneLine) {
  // A backslash and a line feed are the two things that have to be escaped; a
  // carriage return has no spelling at all and would end the line early.
  EXPECT_EQ(openmetrics::escape_help("a\\b"), "a\\\\b");
  EXPECT_EQ(openmetrics::escape_help("two\nlines"), "two\\nlines");
  EXPECT_EQ(openmetrics::escape_help("crlf\r\nhere"), "crlf\\nhere");
  // A quote needs no escape in HELP, and escaping it would leave the backslash
  // visible in the text a reader sees.
  EXPECT_EQ(openmetrics::escape_help("a \"quoted\" word"), "a \"quoted\" word");
}

TEST(OpenmetricsRenderer, ADeclaredUnitIsEmittedAndEndsTheName) {
  // OpenMetrics requires the name of a family that declares a unit to end with
  // it, so the producer says `bytes` once and the suffix follows.
  snapshot s;
  PB::Metrics::MetricsBundle *mem = s.bundle("mem");
  snapshot::described(snapshot::gauge(mem, "physical.used", 17175158784.0), "Physical memory in use", "bytes");

  EXPECT_EQ(render(s),
            "# HELP mem_physical_used_bytes Physical memory in use\n"
            "# TYPE mem_physical_used_bytes gauge\n"
            "# UNIT mem_physical_used_bytes bytes\n"
            "mem_physical_used_bytes 17175158784\n"
            "# EOF\n");
}

TEST(OpenmetricsRenderer, ANameThatAlreadyEndsInItsUnitIsNotSuffixedTwice) {
  // `system.mem.physical.%` sanitises to `..._percent` and a frequency key is
  // already `..._mhz`, so declaring the unit there adds the `# UNIT` line and
  // renames nothing.
  snapshot s;
  PB::Metrics::MetricsBundle *mem = s.bundle("mem");
  snapshot::described(snapshot::gauge(mem, "physical.%", 73.0), "Share of physical memory in use", "percent");
  PB::Metrics::MetricsBundle *cpu = s.bundle("cpu");
  snapshot::described(snapshot::gauge(cpu, "core_0.current_mhz", 2400.0), "Frequency the core is running at", "mhz");

  const std::string body = render(s);

  EXPECT_TRUE(contains(body, "mem_physical_percent 73\n"));
  EXPECT_FALSE(contains(body, "mem_physical_percent_percent"));
  EXPECT_TRUE(contains(body, "cpu_core_0_current_mhz 2400\n"));
  EXPECT_FALSE(contains(body, "current_mhz_mhz"));
}

// --- types ------------------------------------------------------------------

TEST(OpenmetricsRenderer, ACounterCarriesTheTotalSuffixOnItsSample) {
  snapshot s;
  PB::Metrics::MetricsBundle *scheduler = s.bundle("scheduler");
  snapshot::described(snapshot::counter(scheduler, "jobs", 1847.0), "Scheduled checks started since the agent was started");

  EXPECT_EQ(render(s),
            "# HELP scheduler_jobs Scheduled checks started since the agent was started\n"
            "# TYPE scheduler_jobs counter\n"
            "scheduler_jobs_total 1847\n"
            "# EOF\n");
}

TEST(OpenmetricsRenderer, TheOlderTextFormatNamesTheSampleInsteadOfTheFamily) {
  // The whole reason two bodies exist. OpenMetrics says `# TYPE foo counter`
  // describes the family whose sample is `foo_total`; the Prometheus text
  // format has no families, so its `# TYPE` has to name `foo_total` or the
  // counter is read as untyped.
  snapshot s;
  PB::Metrics::MetricsBundle *scheduler = s.bundle("scheduler");
  snapshot::described(snapshot::counter(scheduler, "jobs", 1847.0), "Scheduled checks started since the agent was started");

  EXPECT_EQ(render_text(s),
            "# HELP scheduler_jobs_total Scheduled checks started since the agent was started\n"
            "# TYPE scheduler_jobs_total counter\n"
            "scheduler_jobs_total 1847\n"
            "# EOF\n");
}

TEST(OpenmetricsRenderer, ACounterWithAUnitTakesBothSuffixesInOrder) {
  snapshot s;
  PB::Metrics::MetricsBundle *cpu = s.bundle("process");
  snapshot::described(snapshot::counter(cpu, "cpu", 12.5), "CPU time this process has used", "seconds");

  EXPECT_TRUE(contains(render(s), "process_cpu_seconds_total 12.5\n"));
  EXPECT_TRUE(contains(render(s), "# UNIT process_cpu_seconds seconds\n"));
}

TEST(OpenmetricsRenderer, AnUntypedValueIsSpelledDifferentlyByTheTwoFormats) {
  snapshot s;
  PB::Metrics::MetricsBundle *b = s.bundle("odd");
  snapshot::untyped(b, "value", 7.0);

  EXPECT_TRUE(contains(render(s), "# TYPE odd_value unknown\n"));
  EXPECT_TRUE(contains(render_text(s), "# TYPE odd_value untyped\n"));
}

TEST(OpenmetricsRenderer, AGaugeNamedLikeACountersSampleCannotTakeThatName) {
  // A counter family `foo` owns `foo_total` as well as `foo`, so a gauge whose
  // key sanitises to `foo_total` has to be dropped rather than emitted as a
  // second series of the counter's name.
  snapshot s;
  PB::Metrics::MetricsBundle *b = s.bundle("jobs");
  snapshot::counter(b, "run", 3.0);
  snapshot::gauge(b, "run.total", 9.0);

  std::vector<std::string> problems;
  const std::string body = render(s, &problems);

  EXPECT_EQ(body,
            "# TYPE jobs_run counter\n"
            "jobs_run_total 3\n"
            "# EOF\n");
  ASSERT_EQ(problems.size(), 1u);
  EXPECT_TRUE(contains(problems[0], "jobs_run_total"));
}

TEST(OpenmetricsRenderer, OneNameCannotCarryTwoTypes) {
  // One `# TYPE` line per family, so the second metric has nowhere to go.
  snapshot s;
  PB::Metrics::MetricsBundle *b = s.bundle("mixed");
  snapshot::gauge(b, "value", 1.0);
  snapshot::counter(b, "value", 2.0);

  std::vector<std::string> problems;
  const std::string body = render(s, &problems);

  EXPECT_EQ(body,
            "# TYPE mixed_value gauge\n"
            "mixed_value 1\n"
            "# EOF\n");
  EXPECT_EQ(problems.size(), 1u);
}

// --- labels -----------------------------------------------------------------

TEST(OpenmetricsRenderer, LabelsPutEveryInstanceOfAFamilyUnderOneType) {
  snapshot s;
  PB::Metrics::MetricsBundle *cpu = s.bundle("cpu");
  PB::Metrics::Metric *first = snapshot::gauge(cpu, "core 0.idle", 95.0);
  first->set_alias("idle");
  snapshot::described(snapshot::labelled(first, "core", "0"), "Share of CPU time spent idle", "percent");
  PB::Metrics::Metric *second = snapshot::gauge(cpu, "total.idle", 91.0);
  second->set_alias("idle");
  snapshot::described(snapshot::labelled(second, "core", "total"), "Share of CPU time spent idle", "percent");

  EXPECT_EQ(render(s),
            "# HELP cpu_idle_percent Share of CPU time spent idle\n"
            "# TYPE cpu_idle_percent gauge\n"
            "# UNIT cpu_idle_percent percent\n"
            "cpu_idle_percent{core=\"0\"} 95\n"
            "cpu_idle_percent{core=\"total\"} 91\n"
            "# EOF\n");
}

TEST(OpenmetricsRenderer, LabelValuesAreEscaped) {
  // A Windows device path is all backslashes, and an adapter description can
  // carry a quote; either one unescaped ends the label set early and costs the
  // scraper the whole body.
  EXPECT_EQ(openmetrics::escape_label_value("\\Device\\HarddiskVolume1"), "\\\\Device\\\\HarddiskVolume1");
  EXPECT_EQ(openmetrics::escape_label_value("Intel(R) \"Pro\" NIC"), "Intel(R) \\\"Pro\\\" NIC");
  EXPECT_EQ(openmetrics::escape_label_value("two\nlines"), "two\\nlines");
}

TEST(OpenmetricsRenderer, ALabelNameIsHeldToTheSameGrammarAsAMetricName) {
  snapshot s;
  PB::Metrics::MetricsBundle *net = s.bundle("net");
  snapshot::labelled(snapshot::gauge(net, "sent", 1.0), "network card", "Ethernet 1");

  EXPECT_TRUE(contains(render(s), "net_sent{network_card=\"Ethernet 1\"} 1\n"));
}

TEST(OpenmetricsRenderer, TwoMetricsWithTheSameNameAndTheSameLabelsStillCollide) {
  // Labels make a family hold several series; they do not make it hold the
  // same series twice, which is what a strict parser rejects the body over.
  snapshot s;
  PB::Metrics::MetricsBundle *cpu = s.bundle("cpu");
  PB::Metrics::Metric *first = snapshot::gauge(cpu, "a.idle", 95.0);
  first->set_alias("idle");
  snapshot::labelled(first, "core", "0");
  PB::Metrics::Metric *second = snapshot::gauge(cpu, "b.idle", 91.0);
  second->set_alias("idle");
  snapshot::labelled(second, "core", "0");

  std::vector<std::string> problems;
  const std::string body = render(s, &problems);

  EXPECT_EQ(body,
            "# TYPE cpu_idle gauge\n"
            "cpu_idle{core=\"0\"} 95\n"
            "# EOF\n");
  ASSERT_EQ(problems.size(), 1u);
  EXPECT_TRUE(contains(problems[0], "cpu.b.idle"));
}

TEST(OpenmetricsRenderer, StringsOfDifferentInstancesBecomeDifferentInfoSeries) {
  // One NIC's MAC address and link state belong on one line; the next NIC's
  // belong on the next, not folded into the first.
  snapshot s;
  PB::Metrics::MetricsBundle *net = s.bundle("net");
  PB::Metrics::Metric *mac = snapshot::string_metric(net, "eth0.mac", "00:11:22");
  mac->set_alias("mac");
  snapshot::labelled(mac, "nic", "eth0");
  PB::Metrics::Metric *state = snapshot::string_metric(net, "eth0.state", "up");
  state->set_alias("state");
  snapshot::labelled(state, "nic", "eth0");
  PB::Metrics::Metric *other = snapshot::string_metric(net, "eth1.mac", "00:33:44");
  other->set_alias("mac");
  snapshot::labelled(other, "nic", "eth1");

  EXPECT_EQ(render(s),
            "# TYPE net info\n"
            "net_info{nic=\"eth0\",mac=\"00:11:22\",state=\"up\"} 1\n"
            "net_info{nic=\"eth1\",mac=\"00:33:44\"} 1\n"
            "# EOF\n");
}

TEST(OpenmetricsRenderer, TheRootBundleWithNoKeyStillProducesUsableNames) {
  // A Python script's metrics arrive under a bundle whose key is the empty
  // string. The joined path then starts with the separator, and a name may not
  // begin with an underscore, so it borrows the `metric_` prefix - which is
  // what a script's metrics have scraped as since the renderer landed. The
  // info family has nothing but the bundle path to be named after, so it falls
  // back to the same placeholder a name that sanitises away entirely gets.
  snapshot s;
  PB::Metrics::MetricsBundle *root = s.bundle("");
  snapshot::gauge(root, "myscript.requests", 42.0);
  snapshot::string_metric(root, "myscript.status", "ok");

  EXPECT_EQ(render(s),
            "# TYPE metric_myscript_requests gauge\n"
            "metric_myscript_requests 42\n"
            "# TYPE metric info\n"
            "metric_info{myscript_status=\"ok\"} 1\n"
            "# EOF\n");
}

// --- what a producer can hand over that would break the body ----------------

TEST(OpenmetricsRenderer, AUnitIsHeldToTheSameGrammarAsAName) {
  // A unit is producer-supplied - an operator writes it next to a PDH counter,
  // a Python script returns it - and it reaches both the family name and the
  // `# UNIT` line. Pasted in verbatim, `bytes/sec` makes a document no parser
  // accepts, which costs the scraper every other metric in the body too.
  snapshot s;
  PB::Metrics::MetricsBundle *b = s.bundle("disk");
  snapshot::described(snapshot::gauge(b, "read", 1024.0), "Bytes read per second", "bytes/sec");

  std::vector<std::string> problems;
  const std::string body = render(s, &problems);

  EXPECT_EQ(body,
            "# HELP disk_read_bytes_sec Bytes read per second\n"
            "# TYPE disk_read_bytes_sec gauge\n"
            "# UNIT disk_read_bytes_sec bytes_sec\n"
            "disk_read_bytes_sec 1024\n"
            "# EOF\n");
  // And the operator is told, because the unit they configured is not the one
  // the scraper sees.
  ASSERT_EQ(problems.size(), 1u);
  EXPECT_TRUE(contains(problems[0], "bytes/sec"));
  EXPECT_TRUE(contains(problems[0], "bytes_sec"));
}

TEST(OpenmetricsRenderer, AUnitThatSanitisesAwayEntirelyIsDroppedNotEmitted) {
  // `_` or `///` leaves nothing that can be part of a name. Emitting
  // `# UNIT foo ` with an empty unit is not a line the grammar has.
  snapshot s;
  PB::Metrics::MetricsBundle *b = s.bundle("odd");
  snapshot::described(snapshot::gauge(b, "value", 1.0), "Something", "///");

  std::vector<std::string> problems;
  const std::string body = render(s, &problems);

  EXPECT_EQ(body,
            "# HELP odd_value Something\n"
            "# TYPE odd_value gauge\n"
            "odd_value 1\n"
            "# EOF\n");
  ASSERT_EQ(problems.size(), 1u);
  EXPECT_TRUE(contains(problems[0], "Ignoring the unit"));
}

TEST(OpenmetricsRenderer, AUnitNeverBorrowsTheNamePrefix) {
  // `sanitize_name` gives a name that would not start with a letter a
  // `metric_` prefix. A unit is a fragment of a name, not a name, so
  // `metric_` in the middle of one would be nonsense.
  snapshot s;
  PB::Metrics::MetricsBundle *b = s.bundle("odd");
  snapshot::described(snapshot::gauge(b, "value", 1.0), "Something", "2x");

  const std::string body = render(s);

  EXPECT_TRUE(contains(body, "odd_value_2x 1\n"));
  EXPECT_FALSE(contains(body, "metric_"));
}

TEST(OpenmetricsRenderer, AMetricWithNoValueAtAllIsSkippedRatherThanZeroed) {
  // A producer can key a metric and never value it. Reading the gauge out of
  // it hands back the message default, so the endpoint would publish a 0
  // nobody measured - while /api/v2/metrics, which asks whether there is a
  // value, omits the metric entirely. The two views must agree.
  snapshot s;
  PB::Metrics::MetricsBundle *b = s.bundle("plugin");
  b->add_value()->set_key("keyed_but_never_valued");
  snapshot::gauge(b, "real", 5.0);

  EXPECT_EQ(render(s),
            "# TYPE plugin_real gauge\n"
            "plugin_real 5\n"
            "# EOF\n");
}

TEST(OpenmetricsRenderer, ACounterNamedTotalDoesNotTakeTheSuffixTwice) {
  // A Python script declaring `requests_total` as a counter has no way to
  // avoid this: the sample would be `requests_total_total`, and OpenMetrics
  // separately forbids a counter family name ending in `_total`.
  snapshot s;
  PB::Metrics::MetricsBundle *b = s.bundle("app");
  snapshot::counter(b, "requests_total", 42.0);

  EXPECT_EQ(render(s),
            "# TYPE app_requests counter\n"
            "app_requests_total 42\n"
            "# EOF\n");
  // And the older format, which names the sample, is unchanged by the strip.
  EXPECT_EQ(render_text(s),
            "# TYPE app_requests_total counter\n"
            "app_requests_total 42\n"
            "# EOF\n");
}

TEST(OpenmetricsRenderer, ACounterKeepsItsUnitBeforeTheTotalSuffix) {
  // Both rewrites apply, in the order the spec wants: unit on the family name,
  // `_total` on the sample.
  snapshot s;
  PB::Metrics::MetricsBundle *b = s.bundle("app");
  snapshot::described(snapshot::counter(b, "cpu_total", 12.5), "CPU time used", "seconds");

  EXPECT_TRUE(contains(render(s), "# TYPE app_cpu_total_seconds counter\n"));
  EXPECT_TRUE(contains(render(s), "app_cpu_total_seconds_total 12.5\n"));
}

TEST(OpenmetricsRenderer, AStringCannotWriteALabelTheDimensionsAlreadyCarry) {
  // The first string metric of a series used to be added without checking it
  // against the dimensions the series was created with, so a metric keyed
  // `core` in a bundle whose metrics carry a `core` label wrote `core` twice
  // into one label set - a duplicate a strict parser rejects the body over.
  snapshot s;
  PB::Metrics::MetricsBundle *b = s.bundle("cpu");
  PB::Metrics::Metric *m = snapshot::string_metric(b, "core", "performance");
  snapshot::labelled(m, "core", "0");

  std::vector<std::string> problems;
  const std::string body = render(s, &problems);

  EXPECT_EQ(body,
            "# TYPE cpu info\n"
            "cpu_info{core=\"0\"} 1\n"
            "# EOF\n");
  ASSERT_EQ(problems.size(), 1u);
  EXPECT_TRUE(contains(problems[0], "already on this bundle's info series"));
}

TEST(OpenmetricsRenderer, TwoStringsOfOneSeriesCannotShareALabelName) {
  // The same check on the path that was already covered: two keys sanitising
  // to one label name.
  snapshot s;
  PB::Metrics::MetricsBundle *b = s.bundle("net");
  snapshot::string_metric(b, "link.state", "up");
  snapshot::string_metric(b, "link state", "down");

  std::vector<std::string> problems;
  const std::string body = render(s, &problems);

  EXPECT_EQ(body,
            "# TYPE net info\n"
            "net_info{link_state=\"up\"} 1\n"
            "# EOF\n");
  EXPECT_EQ(problems.size(), 1u);
}

// --- both bodies from one walk ----------------------------------------------

TEST(OpenmetricsRenderer, RenderBothMatchesRenderingEachOnItsOwn) {
  // The endpoint walks the snapshot once and emits twice. That has to be the
  // same output as two independent renders, or the negotiated bodies drift
  // apart from what the tests above pin.
  snapshot s;
  PB::Metrics::MetricsBundle *system = s.bundle("system");
  PB::Metrics::MetricsBundle *mem = snapshot::child(system, "mem");
  snapshot::described(snapshot::gauge(mem, "physical.used", 17175158784.0), "Physical memory in use", "bytes");
  snapshot::described(snapshot::counter(system, "jobs", 7.0), "Jobs run since start");
  snapshot::string_metric(system, "uptime", "1d 12:30");

  const openmetrics::exposition both = openmetrics::render_both(s.message());

  EXPECT_EQ(both.openmetrics, render(s));
  EXPECT_EQ(both.prometheus_text, render_text(s));
}

TEST(OpenmetricsRenderer, RenderBothReportsEachProblemOnce) {
  // Two walks would find every producer problem twice and log it twice.
  snapshot s;
  PB::Metrics::MetricsBundle *mem = s.bundle("mem");
  snapshot::gauge(mem, "used.%", 73.0);
  snapshot::gauge(mem, "used percent", 99.0);

  std::vector<std::string> problems;
  openmetrics::render_both(s.message(), &problems);

  EXPECT_EQ(problems.size(), 1u);
}

// --- the aggregate types ----------------------------------------------------

TEST(OpenmetricsRenderer, ASummaryRendersItsQuantilesSumAndCount) {
  // No producer builds one yet, but the message has been in the schema since
  // 2015 and a renderer that dropped it would emit a family with a `# TYPE`
  // and no samples.
  snapshot s;
  PB::Metrics::MetricsBundle *b = s.bundle("rpc");
  PB::Metrics::Metric *m = b->add_value();
  m->set_key("duration");
  m->set_desc("How long a call took");
  PB::Metrics::Summary *summary = m->mutable_summary_value();
  summary->set_sample_count(4);
  summary->set_sample_sum(10.5);
  PB::Metrics::Quantile *q = summary->add_quantile();
  q->set_quantile(0.5);
  q->set_value(2.0);

  EXPECT_EQ(render(s),
            "# HELP rpc_duration How long a call took\n"
            "# TYPE rpc_duration summary\n"
            "rpc_duration{quantile=\"0.5\"} 2\n"
            "rpc_duration_sum 10.5\n"
            "rpc_duration_count 4\n"
            "# EOF\n");
}

TEST(OpenmetricsRenderer, AHistogramRendersItsBucketsSumAndCount) {
  snapshot s;
  PB::Metrics::MetricsBundle *b = s.bundle("rpc");
  PB::Metrics::Metric *m = b->add_value();
  m->set_key("size");
  PB::Metrics::Histogram *histogram = m->mutable_histogram_value();
  histogram->set_sample_count(3);
  histogram->set_sample_sum(30.0);
  PB::Metrics::Bucket *bucket = histogram->add_bucket();
  bucket->set_upper_bound(10.0);
  bucket->set_cumulative_count(2);

  EXPECT_EQ(render(s),
            "# TYPE rpc_size histogram\n"
            "rpc_size_bucket{le=\"10\"} 2\n"
            "rpc_size_sum 30\n"
            "rpc_size_count 3\n"
            "# EOF\n");
}

// --- what a producer that declares nothing still gets -----------------------

TEST(OpenmetricsRenderer, AMetricWithNoMetadataRendersExactlyAsItAlwaysDid) {
  // Every out-of-tree module, and every module here before the sweep: a key
  // and a number. It must keep working, without a `# HELP` or `# UNIT` line
  // invented for it.
  snapshot s;
  PB::Metrics::MetricsBundle *b = s.bundle("plugin");
  snapshot::gauge(b, "value", 5.0);

  EXPECT_EQ(render(s),
            "# TYPE plugin_value gauge\n"
            "plugin_value 5\n"
            "# EOF\n");
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
