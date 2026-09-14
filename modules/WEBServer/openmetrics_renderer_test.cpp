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

TEST(OpenmetricsRenderer, ALeadingDigitGetsAnUnderscorePrefix) {
  // A PDH counter or a Python script can name a metric anything at all.
  EXPECT_EQ(openmetrics::sanitize_name("5m_load"), "_5m_load");
  EXPECT_EQ(openmetrics::sanitize_name("0"), "_0");
}

TEST(OpenmetricsRenderer, ANameThatSanitisesAwayEntirelyStaysANameAtAll) {
  // An empty name is not a valid sample line, so it must never be emitted -
  // even for a key that is nothing but punctuation.
  EXPECT_EQ(openmetrics::sanitize_name(""), "_");
  EXPECT_EQ(openmetrics::sanitize_name("..."), "_");
}

TEST(OpenmetricsRenderer, SanitisingIsDeterministic) {
  // The same input always maps to the same name: a scraper that sees a family
  // rename between two scrapes loses the series' history.
  EXPECT_EQ(openmetrics::sanitize_name("system.mem.commited.%"), openmetrics::sanitize_name("system.mem.commited.%"));
}

// --- escaping ---------------------------------------------------------------

TEST(OpenmetricsRenderer, HelpTextEscapesBackslashAndNewline) {
  // A quote is legal unescaped in help text; a raw newline would end the line
  // and turn the rest of the text into garbage the parser tries to read as a
  // sample.
  EXPECT_EQ(openmetrics::escape_help("C:\\Windows"), "C:\\\\Windows");
  EXPECT_EQ(openmetrics::escape_help("one\ntwo"), "one\\ntwo");
  EXPECT_EQ(openmetrics::escape_help("say \"hi\""), "say \"hi\"");
}

TEST(OpenmetricsRenderer, LabelValuesEscapeQuotesToo) {
  // Label values are quoted, so an unescaped quote closes the value early -
  // and WMI adapter descriptions and device paths are exactly where one turns
  // up.
  EXPECT_EQ(openmetrics::escape_label_value("\\Device\\HarddiskVolume1"), "\\\\Device\\\\HarddiskVolume1");
  EXPECT_EQ(openmetrics::escape_label_value("Intel(R) \"Pro\" 1000"), "Intel(R) \\\"Pro\\\" 1000");
  EXPECT_EQ(openmetrics::escape_label_value("a\nb"), "a\\nb");
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

TEST(OpenmetricsRenderer, SamplesOfOneFamilyAreContiguousUnderASingleType) {
  // Two bundles can contribute to one family once instances become labels. The
  // grouping has to hold today already: a second `# TYPE` for a family, or a
  // sample appearing after another family started, makes the document invalid.
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
