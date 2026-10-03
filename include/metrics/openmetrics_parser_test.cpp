// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The exposition parser CheckOpenMetrics scrapes exporters with.
//
// The body comes off the network from a process the agent does not control,
// so beyond the grammar these pin the two promises the scraper is built on:
// the parser stops at the first line it cannot read and says which, and no
// input - a truncated body, one enormous line, a hundred thousand series, a
// million metadata lines - costs more than a single linear pass over it or
// keeps more than the caller's limits allow.
//
// Every test names the format it parses: the two formats disagree about family
// names, `# EOF`, timestamps and which types exist, and a case that holds in
// one is often wrong in the other.
//
// The round trip against the agent's own renderer lives beside the renderer,
// in modules/WEBServer/openmetrics_roundtrip_test.cpp.

#include <gtest/gtest.h>

#include <clocale>
#include <cmath>
#include <initializer_list>
#include <metrics/openmetrics_parser.hpp>
#include <ostream>
#include <random>
#include <set>
#include <string>
#include <vector>

namespace om = metrics::openmetrics;

namespace {

const om::format openmetrics = om::format::openmetrics_1_0;
const om::format text = om::format::prometheus_text_0_0_4;

// The single family a body was expected to produce. A failed count is reported
// and an empty family returned, so the test carries on to report what else is
// wrong instead of reading past the end of the vector.
const om::family &only_family(const om::result &parsed) {
  static const om::family none;
  if (parsed.families.size() != 1) {
    ADD_FAILURE() << "expected exactly one family, got " << parsed.families.size();
    return none;
  }
  return parsed.families.front();
}

const om::family &family_named(const om::result &parsed, const std::string &name) {
  static const om::family none;
  for (const om::family &f : parsed.families) {
    if (f.name == name) return f;
  }
  ADD_FAILURE() << "no family '" << name << "'";
  return none;
}

om::label_list labels(std::initializer_list<std::pair<std::string, std::string> > items) { return om::label_list(items); }

}  // namespace

// --- choosing the format ------------------------------------------------------

TEST(OpenmetricsParser, FormatFollowsTheContentType) {
  EXPECT_EQ(om::format_for_content_type("application/openmetrics-text; version=1.0.0; charset=utf-8"), openmetrics);
  EXPECT_EQ(om::format_for_content_type("Application/OpenMetrics-Text"), openmetrics);
  EXPECT_EQ(om::format_for_content_type("text/plain; version=0.0.4; charset=utf-8"), text);
  EXPECT_EQ(om::format_for_content_type(""), text);
  EXPECT_EQ(om::format_for_content_type("application/json"), text);
}

// --- families and metadata ----------------------------------------------------

TEST(OpenmetricsParser, ReadsAGaugeWithItsMetadata) {
  for (const om::format f : {openmetrics, text}) {
    const om::result parsed = om::parse(
        "# HELP node_memory_free_bytes Free memory.\n"
        "# TYPE node_memory_free_bytes gauge\n"
        "# UNIT node_memory_free_bytes bytes\n"
        "node_memory_free_bytes 1.6554e+10\n",
        f);
    ASSERT_TRUE(parsed.ok()) << parsed.error;
    EXPECT_FALSE(parsed.saw_eof);
    EXPECT_EQ(parsed.sample_count, 1u);
    const om::family &g = only_family(parsed);
    EXPECT_EQ(g.name, "node_memory_free_bytes");
    EXPECT_EQ(g.type, om::family_type::gauge);
    EXPECT_EQ(g.help, "Free memory.");
    EXPECT_EQ(g.unit, "bytes");
    ASSERT_EQ(g.samples.size(), 1u);
    EXPECT_EQ(g.samples.at(0).name, "node_memory_free_bytes");
    EXPECT_DOUBLE_EQ(g.samples.at(0).value, 16554000000.0);
    EXPECT_FALSE(g.samples.at(0).timestamp.has_value());
  }
}

TEST(OpenmetricsParser, OpenMetricsCounterIsNamedWithoutItsSuffix) {
  // OpenMetrics names the family and puts `_total` on the sample.
  const om::result parsed = om::parse(
      "# TYPE http_requests counter\n"
      "http_requests_total{code=\"200\"} 1027\n"
      "http_requests_created{code=\"200\"} 1.7e9\n"
      "# EOF\n",
      openmetrics);
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  EXPECT_TRUE(parsed.saw_eof);
  const om::family &f = only_family(parsed);
  EXPECT_EQ(f.name, "http_requests");
  EXPECT_EQ(f.type, om::family_type::counter);
  ASSERT_EQ(f.samples.size(), 2u);
  EXPECT_EQ(f.samples.at(0).name, "http_requests_total");
  EXPECT_EQ(f.samples.at(1).name, "http_requests_created");
}

TEST(OpenmetricsParser, PrometheusTextCounterIsNamedAsDeclared) {
  // The older format names the sample in every metadata line, so that is the
  // family name too. Nothing is renamed.
  const om::result parsed = om::parse(
      "# HELP http_requests_total Requests served.\n"
      "# TYPE http_requests_total counter\n"
      "http_requests_total{code=\"200\"} 1027\n"
      "http_requests_total{code=\"500\"} 3\n",
      text);
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  const om::family &f = only_family(parsed);
  EXPECT_EQ(f.name, "http_requests_total");
  EXPECT_EQ(f.type, om::family_type::counter);
  EXPECT_EQ(f.help, "Requests served.");
  ASSERT_EQ(f.samples.size(), 2u);
  EXPECT_EQ(f.samples.at(1).labels, labels({{"code", "500"}}));
}

TEST(OpenmetricsParser, PrometheusTextGaugeAndTotalCounterAreTwoFamilies) {
  // What client_golang serves by default on every Go exporter. The two used to
  // collide and stop the parse, losing every family sorted after them.
  const std::string gauge_first =
      "# HELP go_memstats_alloc_bytes Bytes allocated and still in use.\n"
      "# TYPE go_memstats_alloc_bytes gauge\n"
      "go_memstats_alloc_bytes 1.2e+06\n"
      "# HELP go_memstats_alloc_bytes_total Total bytes allocated.\n"
      "# TYPE go_memstats_alloc_bytes_total counter\n"
      "go_memstats_alloc_bytes_total 9.8e+07\n"
      "# TYPE node_load1 gauge\n"
      "node_load1 0.5\n";
  const std::string counter_first =
      "# TYPE go_memstats_alloc_bytes_total counter\n"
      "go_memstats_alloc_bytes_total 9.8e+07\n"
      "# TYPE go_memstats_alloc_bytes gauge\n"
      "go_memstats_alloc_bytes 1.2e+06\n"
      "# TYPE node_load1 gauge\n"
      "node_load1 0.5\n";
  for (const std::string &body : {gauge_first, counter_first}) {
    const om::result parsed = om::parse(body, text);
    ASSERT_TRUE(parsed.ok()) << parsed.error;
    ASSERT_EQ(parsed.families.size(), 3u);
    EXPECT_EQ(family_named(parsed, "go_memstats_alloc_bytes").type, om::family_type::gauge);
    EXPECT_EQ(family_named(parsed, "go_memstats_alloc_bytes_total").type, om::family_type::counter);
    EXPECT_DOUBLE_EQ(family_named(parsed, "node_load1").samples.at(0).value, 0.5);
  }
}

TEST(OpenmetricsParser, PrometheusTextCounterWithoutTotal) {
  // Older exporters declare a counter and serve it under the bare name.
  const om::result parsed = om::parse("# TYPE jobs counter\njobs 4\n", text);
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  EXPECT_EQ(only_family(parsed).samples.at(0).name, "jobs");
}

TEST(OpenmetricsParser, PrometheusTextSampleBeforeItsTypeIsAnotherMetric) {
  // In the older format `foo_total` and the counter `foo` are unrelated names.
  const om::result parsed = om::parse("foo_total 1\n# TYPE foo counter\nfoo 2\n", text);
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  ASSERT_EQ(parsed.families.size(), 2u);
  EXPECT_EQ(parsed.families.at(1).type, om::family_type::counter);
}

TEST(OpenmetricsParser, HistogramSamplesAttachToTheirFamily) {
  for (const om::format f : {openmetrics, text}) {
    const om::result parsed = om::parse(
        "# HELP latency_seconds Request latency.\n"
        "# TYPE latency_seconds histogram\n"
        "latency_seconds_bucket{le=\"0.1\"} 3\n"
        "latency_seconds_bucket{le=\"1\"} 7\n"
        "latency_seconds_bucket{le=\"+Inf\"} 8\n"
        "latency_seconds_sum 4.25\n"
        "latency_seconds_count 8\n",
        f);
    ASSERT_TRUE(parsed.ok()) << parsed.error;
    const om::family &h = only_family(parsed);
    EXPECT_EQ(h.name, "latency_seconds");
    EXPECT_EQ(h.type, om::family_type::histogram);
    ASSERT_EQ(h.samples.size(), 5u);
    EXPECT_EQ(om::find_label(h.samples.at(2), "le").value(), "+Inf");
    EXPECT_EQ(h.samples.at(3).name, "latency_seconds_sum");
    EXPECT_DOUBLE_EQ(h.samples.at(4).value, 8);
  }
}

TEST(OpenmetricsParser, SummaryQuantilesSumAndCountAttachToTheirFamily) {
  for (const om::format f : {openmetrics, text}) {
    const om::result parsed = om::parse(
        "# TYPE rpc_duration summary\n"
        "rpc_duration{quantile=\"0.5\"} 0.2\n"
        "rpc_duration{quantile=\"0.99\"} 1.4\n"
        "rpc_duration_sum 120\n"
        "rpc_duration_count 400\n",
        f);
    ASSERT_TRUE(parsed.ok()) << parsed.error;
    const om::family &s = only_family(parsed);
    EXPECT_EQ(s.type, om::family_type::summary);
    ASSERT_EQ(s.samples.size(), 4u);
    EXPECT_EQ(om::find_label(s.samples.at(1), "quantile").value(), "0.99");
  }
}

TEST(OpenmetricsParser, OpenMetricsOnlyTypes) {
  const om::result parsed = om::parse(
      "# TYPE build info\n"
      "build_info{version=\"0.12.5\",revision=\"abc\"} 1\n"
      "# TYPE queue gaugehistogram\n"
      "queue_bucket{le=\"+Inf\"} 3\n"
      "queue_gsum 7\n"
      "queue_gcount 3\n"
      "# TYPE state stateset\n"
      "state{state=\"a\"} 1\n"
      "# TYPE legacy unknown\n"
      "legacy 1\n"
      "# EOF\n",
      openmetrics);
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  ASSERT_EQ(parsed.families.size(), 4u);
  EXPECT_EQ(parsed.families.at(0).name, "build");
  EXPECT_EQ(parsed.families.at(0).type, om::family_type::info);
  EXPECT_EQ(parsed.families.at(0).samples.at(0).labels, labels({{"version", "0.12.5"}, {"revision", "abc"}}));
  EXPECT_EQ(parsed.families.at(1).type, om::family_type::gaugehistogram);
  EXPECT_EQ(parsed.families.at(1).samples.size(), 3u);
  EXPECT_EQ(parsed.families.at(2).type, om::family_type::stateset);
  EXPECT_EQ(parsed.families.at(3).type, om::family_type::unknown);
}

TEST(OpenmetricsParser, PrometheusTextUntypedIsUnknown) {
  const om::result parsed = om::parse("# TYPE legacy untyped\nlegacy 1\n", text);
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  EXPECT_EQ(only_family(parsed).type, om::family_type::unknown);
  EXPECT_STREQ(om::type_name(om::family_type::unknown), "unknown");
}

TEST(OpenmetricsParser, SamplesWithoutMetadataAreGroupedByName) {
  // Nothing says `a_total` is a counter, so it is its own unknown family;
  // `a` is grouped across the interruption rather than split in two.
  for (const om::format f : {openmetrics, text}) {
    const om::result parsed = om::parse(
        "a{x=\"1\"} 1\n"
        "a{x=\"2\"} 2\n"
        "a_total 3\n"
        "a{x=\"3\"} 4\n",
        f);
    ASSERT_TRUE(parsed.ok()) << parsed.error;
    ASSERT_EQ(parsed.families.size(), 2u);
    EXPECT_EQ(parsed.families.at(0).name, "a");
    EXPECT_EQ(parsed.families.at(0).type, om::family_type::unknown);
    EXPECT_EQ(parsed.families.at(0).samples.size(), 3u);
    EXPECT_EQ(parsed.families.at(1).name, "a_total");
    EXPECT_EQ(parsed.sample_count, 4u);
  }
}

TEST(OpenmetricsParser, GaugeDoesNotSwallowASampleWithACounterSuffix) {
  for (const om::format f : {openmetrics, text}) {
    const om::result parsed = om::parse("# TYPE temp gauge\ntemp 21\ntemp_total 4\n", f);
    ASSERT_TRUE(parsed.ok()) << parsed.error;
    ASSERT_EQ(parsed.families.size(), 2u);
    EXPECT_EQ(parsed.families.at(1).name, "temp_total");
  }
}

TEST(OpenmetricsParser, HelpIsUnescaped) {
  const om::result parsed = om::parse("# HELP g Line one\\nline two, a \\\\ and a \\\" and a stray \\d.\ng 1\n", text);
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  EXPECT_EQ(only_family(parsed).help, "Line one\nline two, a \\ and a \" and a stray \\d.");
}

TEST(OpenmetricsParser, KeywordGluedToItsNameIsJustAComment) {
  // `# TYPEfoo` is not a `# TYPE` line, so it declares nothing - the sample
  // after it is an unknown family, not a gauge.
  const om::result parsed = om::parse("# TYPEfoo gauge\nfoo 1\n", text);
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  EXPECT_EQ(only_family(parsed).type, om::family_type::unknown);
}

TEST(OpenmetricsParser, OtherCommentsAndBlankLinesAreIgnored) {
  const om::result parsed = om::parse(
      "# Exported by something.\n"
      "\n"
      "#nothing to see\n"
      "   \n"
      "g 1\n"
      "\n",
      text);
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  EXPECT_EQ(only_family(parsed).name, "g");
}

TEST(OpenmetricsParser, CrlfLineEndingsAreAccepted) {
  const om::result parsed = om::parse("# TYPE g gauge\r\ng{a=\"b\"} 1\r\n# EOF\r\n", openmetrics);
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  EXPECT_TRUE(parsed.saw_eof);
  EXPECT_DOUBLE_EQ(only_family(parsed).samples.at(0).value, 1);
}

// --- # EOF ----------------------------------------------------------------------

TEST(OpenmetricsParser, PrometheusTextEofIsJustAComment) {
  // The older format has no terminator; a comment that reads `# EOF` - or
  // starts with it - ends nothing.
  const om::result parsed = om::parse(
      "# EOF of the header section\n"
      "a 1\n"
      "# EOF\n"
      "b 2\n",
      text);
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  EXPECT_FALSE(parsed.saw_eof);
  EXPECT_EQ(parsed.families.size(), 2u);
}

TEST(OpenmetricsParser, OpenMetricsEofMayBeFollowedByBlankLinesOnly) {
  const om::result trailing = om::parse("foo 1\n# EOF\n\n  \n", openmetrics);
  EXPECT_TRUE(trailing.ok()) << trailing.error;
  EXPECT_TRUE(trailing.saw_eof);
}

TEST(OpenmetricsParser, UnterminatedEofIsAcceptedInEveryShapeTheReaderAccepts) {
  // Whatever the line reader takes for `# EOF` when a line feed follows, it
  // must also take when the line feed was left off.
  for (const char *body : {"foo 1\n# EOF", "foo 1\n  # EOF  ", "foo 1\n#EOF", "foo 1\n# EOF\r", "foo 1\n# EOF\n   ", "foo 1\n# EOF\n\r"}) {
    const om::result parsed = om::parse(body, openmetrics);
    EXPECT_TRUE(parsed.ok()) << body << " -> " << parsed.error;
    EXPECT_TRUE(parsed.saw_eof) << body;
  }
}

// --- samples ----------------------------------------------------------------

TEST(OpenmetricsParser, LabelValuesAreUnescaped) {
  const om::result parsed = om::parse("disk{path=\"\\\\Device\\\\HarddiskVolume1\",desc=\"a \\\"quoted\\\" name\\nsecond\"} 1\n", text);
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  EXPECT_EQ(only_family(parsed).samples.at(0).labels, labels({{"path", "\\Device\\HarddiskVolume1"}, {"desc", "a \"quoted\" name\nsecond"}}));
}

TEST(OpenmetricsParser, LabelSetsMayHaveBlanksATrailingCommaOrBeEmpty) {
  const om::result parsed = om::parse(
      "a{ x = \"1\" , y=\"2\", } 1\n"
      "b{} 2\n"
      "c {z=\"#{}\"}\t3\n",
      text);
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  ASSERT_EQ(parsed.families.size(), 3u);
  EXPECT_EQ(parsed.families.at(0).samples.at(0).labels, labels({{"x", "1"}, {"y", "2"}}));
  EXPECT_TRUE(parsed.families.at(1).samples.at(0).labels.empty());
  EXPECT_EQ(parsed.families.at(2).samples.at(0).labels, labels({{"z", "#{}"}}));
  EXPECT_DOUBLE_EQ(parsed.families.at(2).samples.at(0).value, 3);
}

TEST(OpenmetricsParser, NonFiniteValuesArePreserved) {
  const om::result parsed = om::parse(
      "v{k=\"a\"} NaN\n"
      "v{k=\"b\"} +Inf\n"
      "v{k=\"c\"} -Inf\n"
      "v{k=\"d\"} Inf\n"
      "v{k=\"e\"} -1.5e-3\n"
      "v{k=\"f\"} .5\n",
      text);
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  const om::family &f = only_family(parsed);
  ASSERT_EQ(f.samples.size(), 6u);
  EXPECT_TRUE(std::isnan(f.samples.at(0).value));
  EXPECT_TRUE(std::isinf(f.samples.at(1).value) && f.samples.at(1).value > 0);
  EXPECT_TRUE(std::isinf(f.samples.at(2).value) && f.samples.at(2).value < 0);
  EXPECT_TRUE(std::isinf(f.samples.at(3).value) && f.samples.at(3).value > 0);
  EXPECT_DOUBLE_EQ(f.samples.at(4).value, -0.0015);
  EXPECT_DOUBLE_EQ(f.samples.at(5).value, 0.5);
}

TEST(OpenmetricsParser, ValuesBelowTheSmallestNormalAreReadOnEveryPlatform) {
  // A subnormal is what the renderer writes for one, and some C++ streams
  // refuse it while others read it. `strtod` reads it the same way everywhere.
  const om::result parsed = om::parse("x{k=\"a\"} 5e-324\nx{k=\"b\"} 1e-400\nx{k=\"c\"} -2.2250738585072014e-309\n", text);
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  const om::family &f = only_family(parsed);
  ASSERT_EQ(f.samples.size(), 3u);
  EXPECT_GT(f.samples.at(0).value, 0);
  EXPECT_LT(f.samples.at(0).value, 1e-320);
  EXPECT_EQ(f.samples.at(1).value, 0);
  EXPECT_LT(f.samples.at(2).value, 0);
}

TEST(OpenmetricsParser, DecimalPointDoesNotFollowTheProcessLocale) {
  // `strtod` reads the C locale's decimal point, and an agent whose locale
  // spells it `,` must still read `1.5` as one and a half.
  const char *previous = std::setlocale(LC_NUMERIC, nullptr);
  const std::string restore = previous == nullptr ? "C" : previous;
  bool switched = false;
  for (const char *name : {"de_DE.UTF-8", "de_DE.utf8", "sv_SE.UTF-8", "German_Germany.1252", "de-DE"}) {
    if (std::setlocale(LC_NUMERIC, name) != nullptr && std::string(std::localeconv()->decimal_point) == ",") {
      switched = true;
      break;
    }
  }
  if (!switched) {
    std::setlocale(LC_NUMERIC, restore.c_str());
    GTEST_SKIP() << "no locale with a comma decimal point is installed";
  }
  const om::result parsed = om::parse("x 1.5\ny 2,5\n", text);
  std::setlocale(LC_NUMERIC, restore.c_str());
  EXPECT_FALSE(parsed.ok());
  EXPECT_EQ(parsed.error_line, 2u) << parsed.error;
  EXPECT_DOUBLE_EQ(family_named(parsed, "x").samples.at(0).value, 1.5);
}

TEST(OpenmetricsParser, LongDecimalIsRead) {
  // Longer than the stack buffer the conversion uses.
  const std::string digits(200, '1');
  const om::result parsed = om::parse("x 0." + digits + "\n", text);
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  EXPECT_NEAR(only_family(parsed).samples.at(0).value, 0.1111111111, 1e-9);
}

TEST(OpenmetricsParser, OpenMetricsTimestampIsSecondsAndExemplarSkipped) {
  const om::result parsed = om::parse(
      "# TYPE lat histogram\n"
      "lat_bucket{le=\"1\"} 3 1700000000.5 # {trace_id=\"abc\"} 0.4 1700000000.1\n"
      "lat_bucket{le=\"+Inf\"} 4 # {trace_id=\"def\"} 2\n"
      "lat_count 4 1700000000\n"
      "# EOF\n",
      openmetrics);
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  const om::family &f = only_family(parsed);
  ASSERT_EQ(f.samples.size(), 3u);
  EXPECT_DOUBLE_EQ(f.samples.at(0).timestamp.value(), 1700000000.5);
  EXPECT_FALSE(f.samples.at(1).timestamp.has_value());
  EXPECT_DOUBLE_EQ(f.samples.at(1).value, 4);
  EXPECT_DOUBLE_EQ(f.samples.at(2).timestamp.value(), 1700000000.0);
}

TEST(OpenmetricsParser, PrometheusTextTimestampIsIntegerMilliseconds) {
  const om::result parsed = om::parse("foo 1 1700000000500\nfoo{a=\"b\"} 2 -5\n", text);
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  EXPECT_DOUBLE_EQ(only_family(parsed).samples.at(0).timestamp.value(), 1700000000500.0);
  EXPECT_DOUBLE_EQ(only_family(parsed).samples.at(1).timestamp.value(), -5.0);
}

TEST(OpenmetricsParser, ColonsAreAllowedInMetricNames) {
  const om::result parsed = om::parse("job:requests:rate5m 12\n", text);
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  EXPECT_EQ(only_family(parsed).name, "job:requests:rate5m");
}

TEST(OpenmetricsParser, FindLabelReportsAMissingLabel) {
  const om::result parsed = om::parse("a{x=\"1\"} 1\n", text);
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  const om::sample &s = only_family(parsed).samples.at(0);
  EXPECT_EQ(om::find_label(s, "x").value(), "1");
  EXPECT_FALSE(om::find_label(s, "y").has_value());
}

// --- errors -------------------------------------------------------------------

// Every way a line can be malformed that the grammar distinguishes, one named
// case each, so a regression reports which input it broke on rather than "one
// of the forty in this table". Each case asserts the line the parser stopped on
// and a fragment of the reason, because both are what reaches the operator,
// and runs in every format it is malformed in.
enum class in { both, openmetrics_only, text_only };

struct malformed {
  const char *name;
  std::string body;
  std::size_t line;
  const char *fragment;
  in formats = in::both;
};

void PrintTo(const malformed &m, std::ostream *os) { *os << m.name; }

class OpenmetricsParserMalformed : public ::testing::TestWithParam<malformed> {};

TEST_P(OpenmetricsParserMalformed, IsReportedOnItsLine) {
  const malformed &m = GetParam();
  for (const om::format f : {openmetrics, text}) {
    if (m.formats == in::openmetrics_only && f != openmetrics) continue;
    if (m.formats == in::text_only && f != text) continue;
    const char *label = f == openmetrics ? "openmetrics" : "text";
    const om::result parsed = om::parse(m.body, f);
    EXPECT_FALSE(parsed.ok()) << label << ": " << m.body;
    EXPECT_EQ(parsed.error_line, m.line) << label << ": " << m.body << " -> " << parsed.error;
    EXPECT_NE(parsed.error.find(m.fragment), std::string::npos) << label << ": " << m.body << " -> " << parsed.error;
  }
}

std::string case_name(const ::testing::TestParamInfo<malformed> &info) { return info.param.name; }

// Metric names: `[a-zA-Z_:][a-zA-Z0-9_:]*`, then a blank or a label set.
INSTANTIATE_TEST_SUITE_P(
    Names, OpenmetricsParserMalformed,
    ::testing::Values(malformed{"NoName", "ok 1\n{a=\"b\"} 1\n", 2, "expected a metric name"},
                      malformed{"LeadingDigit", "9lives 1\n", 1, "expected a metric name"}, malformed{"LeadingDash", "-foo 1\n", 1, "expected a metric name"},
                      malformed{"Dash", "foo-bar 1\n", 1, "invalid character after 'foo'"}, malformed{"Dot", "foo.bar 1\n", 1, "invalid character after 'foo'"},
                      malformed{"NonAscii", "f\xc3\xb6o 1\n", 1, "invalid character after 'f'"},
                      malformed{"Nul", std::string("foo\0 1\n", 7), 1, "invalid character after 'foo'"},
                      malformed{"QuoteInName", "foo\"bar\" 1\n", 1, "invalid character"}, malformed{"NameOnly", "ok 1\nfoo\n", 2, "missing value for 'foo'"},
                      malformed{"NameAndBlanks", "foo   \t\n", 1, "missing value for 'foo'"},
                      malformed{"NameAndLabelsOnly", "foo{a=\"b\"}\n", 1, "missing value"},
                      malformed{"NoBlankAfterLabels", "foo{a=\"b\"}1\n", 1, "invalid character after 'foo'"},
                      malformed{"DoubleLabelSet", "foo{a=\"b\"}{c=\"d\"} 1\n", 1, "invalid character"},
                      malformed{"ClosingBraceTwice", "foo{a=\"b\"}} 1\n", 1, "invalid character"}),
    case_name);

// Label sets: `{name="value",...}`, with exactly three escapes in the value.
INSTANTIATE_TEST_SUITE_P(
    Labels, OpenmetricsParserMalformed,
    ::testing::Values(
        malformed{"UnterminatedSet", "foo{a=\"b\" 1\n", 1, "expected ',' or '}'"},
        malformed{"UnterminatedAfterComma", "foo{a=\"b\", 1\n", 1, "invalid label name"}, malformed{"OpenBraceOnly", "foo{\n", 1, "unterminated label set"},
        malformed{"OpenBraceAndBlanks", "foo{   \n", 1, "unterminated label set"},
        malformed{"MissingEquals", "foo{a \"b\"} 1\n", 1, "expected '=' after label 'a'"},
        malformed{"NameOnlyLabel", "foo{a} 1\n", 1, "expected '=' after label 'a'"}, malformed{"MissingName", "foo{=\"b\"} 1\n", 1, "invalid label name"},
        malformed{"MissingValue", "foo{a=} 1\n", 1, "quoted value for label 'a'"}, malformed{"UnquotedValue", "foo{a=b} 1\n", 1, "quoted value for label 'a'"},
        malformed{"SingleQuotedValue", "foo{a='b'} 1\n", 1, "quoted value for label 'a'"},
        malformed{"LeadingDigitName", "foo{1a=\"b\"} 1\n", 1, "invalid label name"},
        malformed{"ColonInName", "foo{a:b=\"c\"} 1\n", 1, "expected '=' after label 'a'"},
        malformed{"DashInName", "foo{a-b=\"c\"} 1\n", 1, "expected '=' after label 'a'"},
        malformed{"MissingSeparator", "foo{a=\"b\" c=\"d\"} 1\n", 1, "expected ',' or '}' after label 'a'"},
        malformed{"SemicolonSeparator", "foo{a=\"b\";c=\"d\"} 1\n", 1, "expected ',' or '}'"}, malformed{"LoneComma", "foo{,} 1\n", 1, "invalid label name"},
        malformed{"DoubleComma", "foo{a=\"b\",,c=\"d\"} 1\n", 1, "invalid label name"},
        malformed{"DuplicateLabel", "foo{a=\"b\",a=\"c\"} 1\n", 1, "label 'a' twice"},
        malformed{"DuplicateLabelSameValue", "foo{a=\"b\",z=\"y\",a=\"b\"} 1\n", 1, "label 'a' twice"},
        malformed{"DuplicateLabelPastTheLinearScan", "foo{a=\"1\",b=\"2\",c=\"3\",d=\"4\",e=\"5\",f=\"6\",g=\"7\",h=\"8\",i=\"9\",j=\"10\",c=\"11\"} 1\n", 1,
                  "label 'c' twice"},
        malformed{"EscapeTab", "foo{a=\"\\t\"} 1\n", 1, "badly escaped value for label 'a'"},
        malformed{"EscapeHex", "foo{a=\"\\x41\"} 1\n", 1, "badly escaped"}, malformed{"EscapeUnicode", "foo{a=\"\\u0041\"} 1\n", 1, "badly escaped"},
        malformed{"EscapeSingleQuote", "foo{a=\"\\'\"} 1\n", 1, "badly escaped"}, malformed{"BackslashAtEndOfLine", "foo{a=\"b\\\n", 1, "badly escaped"},
        malformed{"EscapedClosingQuote", "foo{a=\"b\\\"} 1\n", 1, "badly escaped"},
        malformed{"UnterminatedValue", "foo{a=\"b} 1\n", 1, "unterminated or badly escaped"},
        malformed{"ValueRunsIntoNextLine", "foo{a=\"b\nc\"} 1\n", 1, "unterminated or badly escaped"}),
    case_name);

// Values and timestamps: the decimal float grammar, or a non-finite word for a
// value; a finite decimal (OpenMetrics) or integer (text) for a timestamp.
INSTANTIATE_TEST_SUITE_P(
    Values, OpenmetricsParserMalformed,
    ::testing::Values(
        malformed{"Word", "foo abc\n", 1, "invalid value 'abc' for 'foo'"}, malformed{"Unit", "foo 12ms\n", 1, "invalid value '12ms'"},
        malformed{"Hex", "foo 0x10\n", 1, "invalid value '0x10'"}, malformed{"HexFloat", "foo 0x1p3\n", 1, "invalid value"},
        malformed{"TwoDots", "foo 1.2.3\n", 1, "invalid value '1.2.3'"}, malformed{"DecimalComma", "foo 1,5\n", 1, "invalid value '1,5'"},
        malformed{"DigitSeparator", "foo 1_000\n", 1, "invalid value '1_000'"}, malformed{"DoubleSign", "foo --1\n", 1, "invalid value '--1'"},
        malformed{"SignOnly", "foo +\n", 1, "invalid value '+'"}, malformed{"DotOnly", "foo .\n", 1, "invalid value '.'"},
        malformed{"ExponentOnly", "foo e5\n", 1, "invalid value 'e5'"}, malformed{"EmptyExponent", "foo 1e\n", 1, "invalid value '1e'"},
        malformed{"SignedEmptyExponent", "foo 1e+\n", 1, "invalid value '1e+'"}, malformed{"FractionalExponent", "foo 1e2.5\n", 1, "invalid value"},
        malformed{"Overflow", "foo 1e400\n", 1, "invalid value '1e400'"}, malformed{"NegativeOverflow", "foo -1e400\n", 1, "invalid value '-1e400'"},
        malformed{"NanSuffix", "foo nanx\n", 1, "invalid value 'nanx'"}, malformed{"Infinite", "foo infinite\n", 1, "invalid value 'infinite'"},
        malformed{"DoubleSignInf", "foo ++Inf\n", 1, "invalid value '++Inf'"}, malformed{"BareCarriageReturn", "foo 1\r\r\n", 1, "invalid value"},
        malformed{"TimestampWord", "foo 1 soon\n", 1, "invalid timestamp 'soon'"},
        malformed{"TimestampWithUnit", "foo 1 1700000000s\n", 1, "invalid timestamp"}, malformed{"TimestampOverflow", "foo 1 1e400\n", 1, "invalid timestamp"},
        malformed{"TimestampNaN", "foo 1 NaN\n", 1, "invalid timestamp 'NaN'"}, malformed{"TimestampInf", "foo 1 +Inf\n", 1, "invalid timestamp '+Inf'"},
        malformed{"TimestampNegativeInf", "foo 1 -inf\n", 1, "invalid timestamp '-inf'"},
        malformed{"TimestampFractionalInText", "foo 1 1700000000.5\n", 1, "invalid timestamp", in::text_only},
        malformed{"TimestampExponentInText", "foo 1 17e11\n", 1, "invalid timestamp", in::text_only},
        malformed{"ExemplarInText", "foo 1 # {a=\"b\"} 1\n", 1, "unexpected text", in::text_only},
        malformed{"ThirdToken", "foo 1 2 3\n", 1, "unexpected text after the value of 'foo'"},
        malformed{"ThirdTokenAfterLabels", "foo{a=\"b\"} 1 2 x\n", 1, "unexpected text"}),
    case_name);

// Metadata lines and how they relate to the families around them.
INSTANTIATE_TEST_SUITE_P(
    Metadata, OpenmetricsParserMalformed,
    ::testing::Values(
        malformed{"UnknownType", "# TYPE foo sometimes\n", 1, "unknown type 'sometimes'"},
        malformed{"TypeCaseMatters", "# TYPE foo Counter\n", 1, "unknown type 'Counter'"}, malformed{"MissingType", "# TYPE foo\n", 1, "unknown type ''"},
        malformed{"UntypedInOpenMetrics", "# TYPE foo untyped\n", 1, "unknown type 'untyped'", in::openmetrics_only},
        malformed{"UnknownInText", "# TYPE foo unknown\n", 1, "unknown type 'unknown'", in::text_only},
        malformed{"InfoInText", "# TYPE foo info\n", 1, "unknown type 'info'", in::text_only},
        malformed{"StatesetInText", "# TYPE foo stateset\n", 1, "unknown type 'stateset'", in::text_only},
        malformed{"GaugehistogramInText", "# TYPE foo gaugehistogram\n", 1, "unknown type 'gaugehistogram'", in::text_only},
        malformed{"TypeTrailingText", "# TYPE foo gauge extra\n", 1, "unexpected text after the type of 'foo'"},
        malformed{"TypeWithoutName", "# TYPE\n", 1, "expected a metric name after '# TYPE'"},
        malformed{"TypeWithoutNameButBlank", "# TYPE \n", 1, "expected a metric name after '# TYPE'"},
        malformed{"HelpWithoutName", "# HELP\n", 1, "expected a metric name after '# HELP'"},
        malformed{"UnitWithoutName", "# UNIT\n", 1, "expected a metric name after '# UNIT'"},
        malformed{"NameLeadingDigit", "# UNIT 9foo bytes\n", 1, "expected a metric name"},
        malformed{"NameWithDot", "# TYPE foo.bar gauge\n", 1, "invalid character in metric name 'foo'"},
        malformed{"UnitTrailingText", "# UNIT foo bytes extra\n", 1, "unexpected text after the unit of 'foo'"},
        malformed{"SecondType", "# TYPE foo gauge\n# TYPE foo gauge\n", 2, "second '# TYPE' line for 'foo'"},
        malformed{"SecondHelp", "# HELP foo a\n# HELP foo b\n", 2, "second '# HELP' line for 'foo'"},
        malformed{"SecondUnit", "# UNIT foo s\n# UNIT foo s\n", 2, "second '# UNIT' line for 'foo'"},
        malformed{"TypeAfterSamples", "foo 1\n# TYPE foo gauge\n", 2, "already declared or sampled"},
        malformed{"HelpAfterSamples", "# TYPE foo gauge\nfoo 1\n# HELP foo late\n", 3, "already declared or sampled"},
        malformed{"FamilySplitInTwo", "# TYPE foo gauge\nfoo 1\n# TYPE bar gauge\nbar 1\n# HELP foo late\n", 5, "already declared or sampled"},
        malformed{"BucketBeforeType", "lat_bucket{le=\"1\"} 1\n# TYPE lat histogram\n", 2, "'lat_bucket' came before the '# TYPE' line of 'lat'"},
        malformed{"CountBeforeType", "rpc_count 1\n# TYPE rpc summary\n", 2, "'rpc_count' came before the '# TYPE' line of 'rpc'"},
        malformed{"TotalBeforeType", "foo_total 1\n# TYPE foo counter\n", 2, "'foo_total' came before the '# TYPE' line of 'foo'", in::openmetrics_only},
        malformed{"InfoBeforeType", "build_info 1\n# TYPE build info\n", 2, "came before the '# TYPE' line", in::openmetrics_only},
        malformed{"MetadataForAnOwnedSample", "# TYPE rpc summary\nrpc_count 1\n# TYPE rpc_count gauge\n", 3, "'rpc_count' is a sample of the family 'rpc'"},
        malformed{"MetadataForACounterSample", "# TYPE foo counter\nfoo_total 1\n# HELP foo_total again\n", 3, "'foo_total' is a sample of the family 'foo'",
                  in::openmetrics_only},
        malformed{"BareSampleInHistogram", "# TYPE h histogram\nh_bucket{le=\"1\"} 1\nh 2\nh_count 1\n", 3, "'h' is not a sample of the histogram family 'h'"},
        malformed{"BareSampleInGaugehistogram", "# TYPE q gaugehistogram\nq 1\n", 2, "not a sample of the gaugehistogram family 'q'", in::openmetrics_only},
        malformed{"BareSampleInInfo", "# TYPE b info\nb 1\n", 2, "not a sample of the info family 'b'", in::openmetrics_only},
        malformed{"BareSampleInOpenMetricsCounter", "# TYPE c counter\nc 1\n", 2, "not a sample of the counter family 'c'", in::openmetrics_only}),
    case_name);

// `# EOF` ends an OpenMetrics document; nothing but blank lines may follow it.
INSTANTIATE_TEST_SUITE_P(Eof, OpenmetricsParserMalformed,
                         ::testing::Values(malformed{"SampleAfterEof", "foo 1\n# EOF\nbar 2\n", 3, "text after '# EOF'", in::openmetrics_only},
                                           malformed{"CommentAfterEof", "foo 1\n# EOF\n# comment\n", 3, "text after '# EOF'", in::openmetrics_only},
                                           malformed{"SecondEof", "foo 1\n# EOF\n# EOF\n", 3, "text after '# EOF'", in::openmetrics_only},
                                           malformed{"MetadataAfterEof", "# EOF\n# TYPE foo gauge\n", 2, "text after '# EOF'", in::openmetrics_only},
                                           malformed{"EofTrailingText", "foo 1\n# EOF now\n", 2, "unexpected text after '# EOF'", in::openmetrics_only}),
                         case_name);

// A body that stops without its final line feed was cut off.
INSTANTIATE_TEST_SUITE_P(Truncation, OpenmetricsParserMalformed,
                         ::testing::Values(malformed{"MidValue", "# TYPE foo gauge\nfoo 1", 2, "middle of a line"},
                                           malformed{"MidLabelValue", "# TYPE foo gauge\nfoo{a=\"some", 2, "middle of a line"},
                                           malformed{"MidName", "foo 1\nfo", 2, "middle of a line"},
                                           malformed{"MidMetadata", "foo 1\n# HE", 2, "middle of a line"},
                                           malformed{"MidEof", "foo 1\n# EO", 2, "middle of a line"},
                                           malformed{"MidComment", "foo 1\n# just a comment", 2, "middle of a line"},
                                           malformed{"SingleLineNoFeed", "foo 1", 1, "middle of a line"}),
                         case_name);

TEST(OpenmetricsParser, ErrorKeepsTheFamiliesReadBeforeIt) {
  const om::result parsed = om::parse("# TYPE good gauge\ngood 1\nbad{ 2\nlater 3\n", text);
  EXPECT_FALSE(parsed.ok());
  EXPECT_EQ(parsed.error_line, 3u);
  ASSERT_EQ(parsed.families.size(), 1u);
  EXPECT_EQ(parsed.families.at(0).name, "good");
  EXPECT_EQ(parsed.sample_count, 1u);
}

TEST(OpenmetricsParser, FailedMetadataLineLeavesNoFamilyBehind) {
  // Each line is read in full before it may create a family, so the families
  // left after a failure are exactly the ones read before the failing line.
  for (const char *line : {"# TYPE foo sometimes\n", "# TYPE foo gauge extra\n", "# UNIT foo bytes extra\n", "# TYPE lat histogram\n"}) {
    const om::result parsed = om::parse(std::string("good 1\nlat_bucket{le=\"1\"} 1\n") + line, text);
    EXPECT_FALSE(parsed.ok()) << line;
    ASSERT_EQ(parsed.families.size(), 2u) << line;
    EXPECT_EQ(parsed.families.at(0).name, "good");
    EXPECT_EQ(parsed.families.at(1).name, "lat_bucket");
  }
}

TEST(OpenmetricsParser, FailingSampleIsNotAddedToItsFamily) {
  // The label set is read before the duplicate is found; none of it may leak
  // into the family the sample would have joined.
  const om::result parsed = om::parse("# TYPE foo gauge\nfoo{a=\"1\"} 1\nfoo{a=\"2\",a=\"3\"} 2\n", text);
  EXPECT_FALSE(parsed.ok());
  ASSERT_EQ(parsed.families.size(), 1u);
  EXPECT_EQ(parsed.families.at(0).samples.size(), 1u);
  EXPECT_EQ(parsed.sample_count, 1u);
}

TEST(OpenmetricsParser, ErrorLineCountsBlankCommentAndCrlfLines) {
  const om::result parsed = om::parse("# hello\r\n\r\n\nfoo 1\r\n   \nbar x\r\n", text);
  EXPECT_FALSE(parsed.ok());
  EXPECT_EQ(parsed.error_line, 6u) << parsed.error;
}

TEST(OpenmetricsParser, OnlyTheFirstErrorIsReported) {
  const om::result parsed = om::parse("a 1\nb x\nc{ 3\n", text);
  EXPECT_EQ(parsed.error_line, 2u);
  EXPECT_NE(parsed.error.find("invalid value 'x'"), std::string::npos) << parsed.error;
}

TEST(OpenmetricsParser, EmptyBodyIsAnEmptyScrape) {
  for (const om::format f : {openmetrics, text}) {
    const om::result parsed = om::parse("", f);
    EXPECT_TRUE(parsed.ok());
    EXPECT_TRUE(parsed.families.empty());
    EXPECT_FALSE(parsed.saw_eof);
  }
}

// --- bounded input ------------------------------------------------------------

TEST(OpenmetricsParser, TruncatedAtALineBoundaryIsMissingOnlyItsEof) {
  // Nothing in an OpenMetrics body cut on a line boundary can tell it apart
  // from a whole one except the missing terminator, which the caller checks.
  const om::result parsed = om::parse("# TYPE foo gauge\nfoo 1\n", openmetrics);
  EXPECT_TRUE(parsed.ok());
  EXPECT_FALSE(parsed.saw_eof);
}

TEST(OpenmetricsParser, SixtyFourMegabyteLineIsReadInOnePass) {
  const std::size_t size = 64u * 1024u * 1024u;
  std::string body = "big{blob=\"";
  body.append(size, 'x');
  body += "\"} 1\n";
  const om::result parsed = om::parse(body, text);
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  EXPECT_EQ(only_family(parsed).samples.at(0).labels.at(0).second.size(), size);
}

TEST(OpenmetricsParser, LineOverTheLimitIsRefusedUnread) {
  std::string body = "small 1\nbig{blob=\"";
  body.append(64u * 1024u * 1024u, 'x');
  body += "\"} 1\n";
  om::limits bounds;
  bounds.max_line_bytes = 1024 * 1024;
  const om::result parsed = om::parse(body, text, bounds);
  EXPECT_FALSE(parsed.ok());
  EXPECT_EQ(parsed.error_line, 2u);
  EXPECT_NE(parsed.error.find("longer than 1048576 bytes"), std::string::npos) << parsed.error;
  EXPECT_EQ(parsed.sample_count, 1u);
}

TEST(OpenmetricsParser, LineOfAHundredThousandLabelsIsReadLinearly) {
  // A repeated label name is looked up through a hash set once a sample has
  // more than a handful, so this is one pass rather than five billion
  // comparisons. The repeat at the end proves the set is the one consulted.
  std::string body = "wide{";
  for (int i = 0; i < 100000; ++i) body += "l" + std::to_string(i) + "=\"v\",";
  body += "l0=\"again\"} 1\n";
  const om::result parsed = om::parse(body, text);
  EXPECT_FALSE(parsed.ok());
  EXPECT_NE(parsed.error.find("label 'l0' twice"), std::string::npos) << parsed.error;

  const std::string accepted = body.substr(0, body.size() - std::string("l0=\"again\"} 1\n").size()) + "} 1\n";
  const om::result read = om::parse(accepted, text);
  ASSERT_TRUE(read.ok()) << read.error;
  EXPECT_EQ(only_family(read).samples.at(0).labels.size(), 100000u);
}

namespace {

std::string many_series(const std::size_t count) {
  std::string body = "# HELP series_value One of many.\n# TYPE series_value gauge\n";
  for (std::size_t i = 0; i < count; ++i) {
    body += "series_value{instance=\"" + std::to_string(i) + "\",job=\"bench\"} " + std::to_string(i) + "\n";
  }
  body += "# EOF\n";
  return body;
}

}  // namespace

TEST(OpenmetricsParser, HundredThousandSeriesParse) {
  const om::result parsed = om::parse(many_series(100000), openmetrics);
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  EXPECT_EQ(parsed.sample_count, 100000u);
  const om::family &f = only_family(parsed);
  EXPECT_EQ(f.samples.size(), 100000u);
  EXPECT_DOUBLE_EQ(f.samples.at(99999).value, 99999);
}

TEST(OpenmetricsParser, SeriesOverTheLimitStopTheParse) {
  om::limits bounds;
  bounds.max_series = 50000;
  const om::result parsed = om::parse(many_series(100000), openmetrics, bounds);
  EXPECT_FALSE(parsed.ok());
  EXPECT_NE(parsed.error.find("more than 50000 series"), std::string::npos) << parsed.error;
  EXPECT_EQ(parsed.sample_count, 50000u);
  // Two metadata lines, then the 50001st sample.
  EXPECT_EQ(parsed.error_line, 50003u);
}

TEST(OpenmetricsParser, MetadataOnlyFamiliesCountAgainstTheFamilyLimit) {
  // A body of nothing but `# TYPE` lines has no samples for `max_series` to
  // count, and would otherwise grow the result without bound.
  std::string body;
  for (int i = 0; i < 100000; ++i) body += "# TYPE m" + std::to_string(i) + " gauge\n";
  om::limits bounds;
  bounds.max_series = 10;
  bounds.max_families = 1000;
  const om::result parsed = om::parse(body, text, bounds);
  EXPECT_FALSE(parsed.ok());
  EXPECT_NE(parsed.error.find("more than 1000 families"), std::string::npos) << parsed.error;
  EXPECT_EQ(parsed.error_line, 1001u);
  EXPECT_EQ(parsed.families.size(), 1000u);
}

TEST(OpenmetricsParser, SampledFamiliesCountAgainstTheFamilyLimit) {
  om::limits bounds;
  bounds.max_families = 2;
  const om::result parsed = om::parse("a 1\nb 1\na 2\nc 1\n", text, bounds);
  EXPECT_FALSE(parsed.ok());
  EXPECT_EQ(parsed.error_line, 4u);
  EXPECT_EQ(parsed.families.size(), 2u);
  EXPECT_EQ(parsed.sample_count, 3u);
}

// --- hostile input ------------------------------------------------------------
//
// The cases above pin what the parser says about inputs someone thought of.
// These pin what it does with inputs nobody did: whatever the bytes, it must
// return, and what it returns must be internally consistent - which is what
// every caller of `parse()` relies on without checking.

namespace {

// What a result promises regardless of input.
void expect_consistent(const om::result &parsed, const std::string &body) {
  std::size_t lines = 1;
  for (const char c : body) {
    if (c == '\n') ++lines;
  }
  EXPECT_EQ(parsed.ok(), parsed.error.empty());
  if (parsed.ok()) {
    EXPECT_EQ(parsed.error_line, 0u);
  } else {
    EXPECT_GE(parsed.error_line, 1u);
    EXPECT_LE(parsed.error_line, lines);
  }
  std::set<std::string> names;
  std::size_t samples = 0;
  for (const om::family &f : parsed.families) {
    EXPECT_FALSE(f.name.empty());
    // Anything keyed on the family name - the scraper's republication, a
    // check's `name` keyword - relies on there being one family per name.
    EXPECT_TRUE(names.insert(f.name).second) << "family '" << f.name << "' twice";
    for (const om::sample &s : f.samples) {
      EXPECT_EQ(s.name.compare(0, f.name.size(), f.name), 0) << s.name << " in " << f.name;
      if (s.timestamp.has_value()) {
        EXPECT_TRUE(std::isfinite(s.timestamp.value())) << s.name;
      }
    }
    samples += f.samples.size();
  }
  EXPECT_EQ(samples, parsed.sample_count);
}

const char *const exposition =
    "# HELP http_requests Requests served.\n"
    "# TYPE http_requests counter\n"
    "http_requests_total{code=\"200\",path=\"/a \\\"b\\\"\"} 1027 1700000000\n"
    "http_requests_created{code=\"200\",path=\"/a \\\"b\\\"\"} 1.7e9\n"
    "# TYPE latency_seconds histogram\n"
    "# UNIT latency_seconds seconds\n"
    "latency_seconds_bucket{le=\"0.1\"} 3 # {trace_id=\"x\"} 0.05\n"
    "latency_seconds_bucket{le=\"+Inf\"} 8\n"
    "latency_seconds_sum 4.25\n"
    "latency_seconds_count 8\n"
    "# TYPE build info\n"
    "build_info{version=\"1\\n2\\\\3\"} 1\n"
    "untyped_thing NaN\n"
    "# EOF\n";

// A body that is valid in both formats, for the tests that run both.
const char *const shared_exposition =
    "# HELP h Latency.\n"
    "# TYPE h histogram\n"
    "h_bucket{le=\"1\"} 3\n"
    "h_bucket{le=\"+Inf\"} 4\n"
    "h_sum 2.5\n"
    "h_count 4\n"
    "# TYPE s summary\n"
    "s{quantile=\"0.5\"} 1\n"
    "s_sum 9\n"
    "s_count 3\n"
    "g{a=\"x\"} 1 1700000000\n"
    "g{a=\"y\"} -Inf\n";

}  // namespace

TEST(OpenmetricsParser, EveryTruncationIsRefusedOrEndsOnALine) {
  // Cut a valid exposition at every byte. A prefix that ends on a line feed is
  // a shorter valid exposition; any other prefix was cut mid-line and must be
  // refused - never read as a sample that happens to parse.
  const std::string body = exposition;
  ASSERT_TRUE(om::parse(body, openmetrics).ok());
  for (std::size_t length = 0; length < body.size(); ++length) {
    const std::string prefix = body.substr(0, length);
    const om::result parsed = om::parse(prefix, openmetrics);
    expect_consistent(parsed, prefix);
    const bool bare_eof = length == body.size() - 1;
    const bool on_a_line = prefix.empty() || prefix[prefix.size() - 1] == '\n' || bare_eof;
    if (on_a_line) {
      EXPECT_TRUE(parsed.ok()) << "cut at " << length << ": " << parsed.error;
    } else {
      EXPECT_FALSE(parsed.ok()) << "cut at " << length << " was read";
      EXPECT_NE(parsed.error.find("middle of a line"), std::string::npos) << "cut at " << length << ": " << parsed.error;
    }
    EXPECT_EQ(parsed.saw_eof, bare_eof) << "cut at " << length;
  }
}

TEST(OpenmetricsParser, EverySingleByteCorruptionIsHandled) {
  // Overwrite every byte with each character the grammar gives a meaning to,
  // and a few it does not. Most results are errors; all must be consistent.
  const char replacements[] = {'"', '\\', '{', '}', '=', ',', ' ', '\t', '#', '\n', '\r', '\0', '\x7f', '\xff', 'x', '1', '.', 'e', '-', '+'};
  const std::pair<const char *, om::format> bodies[] = {{exposition, openmetrics}, {shared_exposition, openmetrics}, {shared_exposition, text}};
  for (const std::pair<const char *, om::format> &entry : bodies) {
    const std::string body = entry.first;
    ASSERT_TRUE(om::parse(body, entry.second).ok());
    std::size_t failures = 0;
    for (std::size_t at = 0; at < body.size(); ++at) {
      for (const char replacement : replacements) {
        std::string corrupt = body;
        corrupt[at] = replacement;
        const om::result parsed = om::parse(corrupt, entry.second);
        expect_consistent(parsed, corrupt);
        if (!parsed.ok()) ++failures;
      }
    }
    // Not a precise number - only proof that the corruption is being noticed
    // rather than read through.
    EXPECT_GT(failures, body.size());
  }
}

TEST(OpenmetricsParser, RandomBytesAreHandled) {
  // Short random documents drawn mostly from the grammar's own characters, so
  // they get past the first token often enough to reach the deeper states -
  // including the names that clash (`h`, `h_bucket`, `h_total`). Fixed seed: a
  // failure here reproduces.
  const std::string alphabet = "h_{}=,\"\\ \t#\n\r.019eE+-InfNa\xff";
  const char *const fragments[] = {
      "# TYPE h histogram\n", "# TYPE h counter\n", "# TYPE h summary\n", "# TYPE h_total gauge\n", "# EOF\n", "h_bucket{le=\"1\"} 1\n", "h_total 1\n", "h 1\n",
      "h_count 2\n",          "# HELP h x\n"};
  std::mt19937 random(1499);
  std::uniform_int_distribution<std::size_t> pick(0, alphabet.size() - 1);
  std::uniform_int_distribution<std::size_t> pick_fragment(0, sizeof(fragments) / sizeof(fragments[0]) - 1);
  std::uniform_int_distribution<std::size_t> length(0, 12);
  for (int round = 0; round < 20000; ++round) {
    std::string body;
    const std::size_t pieces = length(random);
    for (std::size_t i = 0; i < pieces; ++i) {
      if (random() % 2 == 0) {
        body += fragments[pick_fragment(random)];
      } else {
        for (std::size_t j = 0; j < 8; ++j) body += alphabet[pick(random)];
      }
    }
    for (const om::format f : {openmetrics, text}) {
      const om::result parsed = om::parse(body, f);
      expect_consistent(parsed, body);
    }
    if (::testing::Test::HasFailure()) {
      ADD_FAILURE() << "round " << round << " body: " << body;
      return;
    }
  }
}

TEST(OpenmetricsParser, RandomLinesSplicedIntoAValidBodyAreHandled) {
  // A valid body with a random line after it: the family before the splice
  // must survive whatever the spliced line does.
  const std::string alphabet = "abc_:{}=,\"\\ \t#.0123456789eE+-";
  std::mt19937 random(1623);
  std::uniform_int_distribution<std::size_t> pick(0, alphabet.size() - 1);
  std::uniform_int_distribution<std::size_t> length(1, 40);
  for (int round = 0; round < 5000; ++round) {
    std::string line;
    const std::size_t size = length(random);
    for (std::size_t i = 0; i < size; ++i) line += alphabet[pick(random)];
    const std::string body = std::string("# TYPE kept gauge\nkept 1\n") + line + "\n";
    for (const om::format f : {openmetrics, text}) {
      const om::result parsed = om::parse(body, f);
      expect_consistent(parsed, body);
      ASSERT_FALSE(parsed.families.empty()) << line;
      EXPECT_EQ(parsed.families.at(0).name, "kept") << line;
      EXPECT_GE(parsed.families.at(0).samples.size(), 1u) << line;
      if (!parsed.ok()) {
        EXPECT_EQ(parsed.error_line, 3u) << line << " -> " << parsed.error;
      }
    }
  }
}
