// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The exposition parser CheckOpenMetrics scrapes exporters with.
//
// The body comes off the network from a process the agent does not control,
// so beyond the grammar these pin the two promises the scraper is built on:
// the parser stops at the first line it cannot read and says which, and no
// input - a truncated body, one enormous line, a hundred thousand series -
// costs more than a single linear pass over it.
//
// The round trip against the agent's own renderer lives beside the renderer,
// in modules/WEBServer/openmetrics_roundtrip_test.cpp.

#include <gtest/gtest.h>

#include <cmath>
#include <initializer_list>
#include <metrics/openmetrics_parser.hpp>
#include <string>

namespace om = metrics::openmetrics;

namespace {

const om::family &only_family(const om::result &parsed) {
  EXPECT_EQ(parsed.families.size(), 1u);
  return parsed.families.front();
}

om::label_list labels(std::initializer_list<std::pair<std::string, std::string> > items) { return om::label_list(items); }

}  // namespace

// --- families and metadata --------------------------------------------------

TEST(OpenmetricsParser, ReadsAGaugeWithItsMetadata) {
  const om::result parsed = om::parse(
      "# HELP node_memory_free_bytes Free memory.\n"
      "# TYPE node_memory_free_bytes gauge\n"
      "# UNIT node_memory_free_bytes bytes\n"
      "node_memory_free_bytes 1.6554e+10\n"
      "# EOF\n");
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  EXPECT_TRUE(parsed.saw_eof);
  EXPECT_EQ(parsed.sample_count, 1u);
  const om::family &f = only_family(parsed);
  EXPECT_EQ(f.name, "node_memory_free_bytes");
  EXPECT_EQ(f.type, om::family_type::gauge);
  EXPECT_EQ(f.help, "Free memory.");
  EXPECT_EQ(f.unit, "bytes");
  ASSERT_EQ(f.samples.size(), 1u);
  EXPECT_EQ(f.samples[0].name, "node_memory_free_bytes");
  EXPECT_DOUBLE_EQ(f.samples[0].value, 16554000000.0);
  EXPECT_FALSE(f.samples[0].timestamp.has_value());
}

TEST(OpenmetricsParser, OpenMetricsCounterKeepsItsFamilyName) {
  // OpenMetrics names the family and puts `_total` on the sample.
  const om::result parsed = om::parse(
      "# TYPE http_requests counter\n"
      "http_requests_total{code=\"200\"} 1027\n"
      "http_requests_created{code=\"200\"} 1.7e9\n"
      "# EOF\n");
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  const om::family &f = only_family(parsed);
  EXPECT_EQ(f.name, "http_requests");
  EXPECT_EQ(f.type, om::family_type::counter);
  ASSERT_EQ(f.samples.size(), 2u);
  EXPECT_EQ(f.samples[0].name, "http_requests_total");
  EXPECT_EQ(f.samples[1].name, "http_requests_created");
}

TEST(OpenmetricsParser, PrometheusTextCounterIsFoldedOntoTheSameFamily) {
  // The older format names the sample in every metadata line. Folding the
  // `_total` off is what makes both dialects of one exposition one family.
  const om::result parsed = om::parse(
      "# HELP http_requests_total Requests served.\n"
      "# TYPE http_requests_total counter\n"
      "http_requests_total{code=\"200\"} 1027\n"
      "http_requests_total{code=\"500\"} 3\n");
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  EXPECT_FALSE(parsed.saw_eof);
  const om::family &f = only_family(parsed);
  EXPECT_EQ(f.name, "http_requests");
  EXPECT_EQ(f.type, om::family_type::counter);
  EXPECT_EQ(f.help, "Requests served.");
  ASSERT_EQ(f.samples.size(), 2u);
  EXPECT_EQ(f.samples[1].labels, labels({{"code", "500"}}));
}

TEST(OpenmetricsParser, PrometheusTextUnitLineAfterTheFoldStillFindsTheFamily) {
  const om::result parsed = om::parse(
      "# TYPE cpu_seconds_total counter\n"
      "# UNIT cpu_seconds_total seconds\n"
      "cpu_seconds_total 12.5\n");
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  const om::family &f = only_family(parsed);
  EXPECT_EQ(f.name, "cpu_seconds");
  EXPECT_EQ(f.unit, "seconds");
}

TEST(OpenmetricsParser, CounterWithoutTotalIsStillItsOwnSample) {
  // Older exporters declare a counter and serve it under the bare name.
  const om::result parsed = om::parse(
      "# TYPE jobs counter\n"
      "jobs 4\n");
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  const om::family &f = only_family(parsed);
  EXPECT_EQ(f.name, "jobs");
  ASSERT_EQ(f.samples.size(), 1u);
  EXPECT_EQ(f.samples[0].name, "jobs");
}

TEST(OpenmetricsParser, HistogramSamplesAttachToTheirFamily) {
  const om::result parsed = om::parse(
      "# HELP latency_seconds Request latency.\n"
      "# TYPE latency_seconds histogram\n"
      "latency_seconds_bucket{le=\"0.1\"} 3\n"
      "latency_seconds_bucket{le=\"1\"} 7\n"
      "latency_seconds_bucket{le=\"+Inf\"} 8\n"
      "latency_seconds_sum 4.25\n"
      "latency_seconds_count 8\n"
      "# EOF\n");
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  const om::family &f = only_family(parsed);
  EXPECT_EQ(f.name, "latency_seconds");
  EXPECT_EQ(f.type, om::family_type::histogram);
  ASSERT_EQ(f.samples.size(), 5u);
  EXPECT_EQ(om::find_label(f.samples[2], "le").value(), "+Inf");
  EXPECT_EQ(f.samples[3].name, "latency_seconds_sum");
  EXPECT_DOUBLE_EQ(f.samples[4].value, 8);
}

TEST(OpenmetricsParser, SummaryQuantilesSumAndCountAttachToTheirFamily) {
  const om::result parsed = om::parse(
      "# TYPE rpc_duration summary\n"
      "rpc_duration{quantile=\"0.5\"} 0.2\n"
      "rpc_duration{quantile=\"0.99\"} 1.4\n"
      "rpc_duration_sum 120\n"
      "rpc_duration_count 400\n");
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  const om::family &f = only_family(parsed);
  EXPECT_EQ(f.type, om::family_type::summary);
  ASSERT_EQ(f.samples.size(), 4u);
  EXPECT_EQ(om::find_label(f.samples[1], "quantile").value(), "0.99");
}

TEST(OpenmetricsParser, InfoFamilyIsNamedWithoutItsSuffix) {
  const om::result parsed = om::parse(
      "# TYPE build info\n"
      "build_info{version=\"0.12.5\",revision=\"abc\"} 1\n"
      "# EOF\n");
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  const om::family &f = only_family(parsed);
  EXPECT_EQ(f.name, "build");
  EXPECT_EQ(f.type, om::family_type::info);
  EXPECT_EQ(f.samples[0].labels, labels({{"version", "0.12.5"}, {"revision", "abc"}}));
}

TEST(OpenmetricsParser, UntypedIsUnknown) {
  const om::result parsed = om::parse(
      "# TYPE legacy untyped\n"
      "legacy 1\n");
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  EXPECT_EQ(only_family(parsed).type, om::family_type::unknown);
  EXPECT_STREQ(om::type_name(om::family_type::unknown), "unknown");
}

TEST(OpenmetricsParser, SamplesWithoutMetadataAreGroupedByName) {
  // Nothing says `a_total` is a counter, so it is its own unknown family;
  // `a` is grouped across the interruption rather than split in two.
  const om::result parsed = om::parse(
      "a{x=\"1\"} 1\n"
      "a{x=\"2\"} 2\n"
      "a_total 3\n"
      "a{x=\"3\"} 4\n");
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  ASSERT_EQ(parsed.families.size(), 2u);
  EXPECT_EQ(parsed.families[0].name, "a");
  EXPECT_EQ(parsed.families[0].type, om::family_type::unknown);
  EXPECT_EQ(parsed.families[0].samples.size(), 3u);
  EXPECT_EQ(parsed.families[1].name, "a_total");
  EXPECT_EQ(parsed.sample_count, 4u);
}

TEST(OpenmetricsParser, GaugeDoesNotSwallowASampleWithACounterSuffix) {
  const om::result parsed = om::parse(
      "# TYPE temp gauge\n"
      "temp 21\n"
      "temp_total 4\n");
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  ASSERT_EQ(parsed.families.size(), 2u);
  EXPECT_EQ(parsed.families[1].name, "temp_total");
}

TEST(OpenmetricsParser, HelpIsUnescaped) {
  const om::result parsed = om::parse(
      "# HELP g Line one\\nline two, a \\\\ and a \\\" and a stray \\d.\n"
      "g 1\n");
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  EXPECT_EQ(only_family(parsed).help, "Line one\nline two, a \\ and a \" and a stray \\d.");
}

TEST(OpenmetricsParser, OtherCommentsAndBlankLinesAreIgnored) {
  const om::result parsed = om::parse(
      "# Exported by something.\n"
      "\n"
      "#nothing to see\n"
      "   \n"
      "g 1\n"
      "\n");
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  EXPECT_EQ(only_family(parsed).name, "g");
}

TEST(OpenmetricsParser, CrlfLineEndingsAreAccepted) {
  const om::result parsed = om::parse("# TYPE g gauge\r\ng{a=\"b\"} 1\r\n# EOF\r\n");
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  EXPECT_TRUE(parsed.saw_eof);
  EXPECT_DOUBLE_EQ(only_family(parsed).samples[0].value, 1);
}

// --- samples ----------------------------------------------------------------

TEST(OpenmetricsParser, LabelValuesAreUnescaped) {
  const om::result parsed = om::parse("disk{path=\"\\\\Device\\\\HarddiskVolume1\",desc=\"a \\\"quoted\\\" name\\nsecond\"} 1\n");
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  EXPECT_EQ(only_family(parsed).samples[0].labels, labels({{"path", "\\Device\\HarddiskVolume1"}, {"desc", "a \"quoted\" name\nsecond"}}));
}

TEST(OpenmetricsParser, LabelSetsMayHaveBlanksATrailingCommaOrBeEmpty) {
  const om::result parsed = om::parse(
      "a{ x = \"1\" , y=\"2\", } 1\n"
      "b{} 2\n"
      "c {z=\"#{}\"}\t3\n");
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  ASSERT_EQ(parsed.families.size(), 3u);
  EXPECT_EQ(parsed.families[0].samples[0].labels, labels({{"x", "1"}, {"y", "2"}}));
  EXPECT_TRUE(parsed.families[1].samples[0].labels.empty());
  EXPECT_EQ(parsed.families[2].samples[0].labels, labels({{"z", "#{}"}}));
  EXPECT_DOUBLE_EQ(parsed.families[2].samples[0].value, 3);
}

TEST(OpenmetricsParser, NonFiniteValuesArePreserved) {
  const om::result parsed = om::parse(
      "v{k=\"a\"} NaN\n"
      "v{k=\"b\"} +Inf\n"
      "v{k=\"c\"} -Inf\n"
      "v{k=\"d\"} Inf\n"
      "v{k=\"e\"} -1.5e-3\n"
      "v{k=\"f\"} .5\n");
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  const om::family &f = only_family(parsed);
  ASSERT_EQ(f.samples.size(), 6u);
  EXPECT_TRUE(std::isnan(f.samples[0].value));
  EXPECT_TRUE(std::isinf(f.samples[1].value) && f.samples[1].value > 0);
  EXPECT_TRUE(std::isinf(f.samples[2].value) && f.samples[2].value < 0);
  EXPECT_TRUE(std::isinf(f.samples[3].value) && f.samples[3].value > 0);
  EXPECT_DOUBLE_EQ(f.samples[4].value, -0.0015);
  EXPECT_DOUBLE_EQ(f.samples[5].value, 0.5);
}

TEST(OpenmetricsParser, TimestampIsKeptAndExemplarSkipped) {
  const om::result parsed = om::parse(
      "# TYPE lat histogram\n"
      "lat_bucket{le=\"1\"} 3 1700000000.5 # {trace_id=\"abc\"} 0.4 1700000000.1\n"
      "lat_bucket{le=\"+Inf\"} 4 # {trace_id=\"def\"} 2\n"
      "lat_count 4 1700000000500\n"
      "# EOF\n");
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  const om::family &f = only_family(parsed);
  ASSERT_EQ(f.samples.size(), 3u);
  EXPECT_DOUBLE_EQ(f.samples[0].timestamp.value(), 1700000000.5);
  EXPECT_FALSE(f.samples[1].timestamp.has_value());
  EXPECT_DOUBLE_EQ(f.samples[1].value, 4);
  EXPECT_DOUBLE_EQ(f.samples[2].timestamp.value(), 1700000000500.0);
}

TEST(OpenmetricsParser, ColonsAreAllowedInMetricNames) {
  const om::result parsed = om::parse("job:requests:rate5m 12\n");
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  EXPECT_EQ(only_family(parsed).name, "job:requests:rate5m");
}

TEST(OpenmetricsParser, FindLabelReportsAMissingLabel) {
  const om::result parsed = om::parse("a{x=\"1\"} 1\n");
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  EXPECT_EQ(om::find_label(parsed.families[0].samples[0], "x").value(), "1");
  EXPECT_FALSE(om::find_label(parsed.families[0].samples[0], "y").has_value());
}

// --- errors -----------------------------------------------------------------

namespace {

// Parses `body` and expects it to fail on `line`, with an error mentioning
// `fragment`.
void expect_failure(const std::string &body, const std::size_t line, const std::string &fragment) {
  const om::result parsed = om::parse(body);
  EXPECT_FALSE(parsed.ok()) << body;
  EXPECT_EQ(parsed.error_line, line) << body << " -> " << parsed.error;
  EXPECT_NE(parsed.error.find(fragment), std::string::npos) << body << " -> " << parsed.error;
}

}  // namespace

TEST(OpenmetricsParser, MalformedSamplesAreReportedNotGuessed) {
  expect_failure("ok 1\n{a=\"b\"} 1\n", 2, "expected a metric name");
  expect_failure("ok 1\nfoo\n", 2, "missing value");
  expect_failure("foo{a=\"b\"}1\n", 1, "invalid character");
  expect_failure("foo-bar 1\n", 1, "invalid character");
  expect_failure("foo 12ms\n", 1, "invalid value '12ms'");
  expect_failure("foo 0x10\n", 1, "invalid value");
  expect_failure("foo 1e\n", 1, "invalid value");
  expect_failure("foo 1 soon\n", 1, "invalid timestamp");
  expect_failure("foo 1 2 3\n", 1, "unexpected text");
  expect_failure("foo{a=b} 1\n", 1, "quoted value");
  expect_failure("foo{a=\"b\" c=\"d\"} 1\n", 1, "expected ',' or '}'");
  expect_failure("foo{a=\"b\",a=\"c\"} 1\n", 1, "label 'a' twice");
  expect_failure("foo{1a=\"b\"} 1\n", 1, "invalid label name");
  expect_failure("foo{a=\"\\t\"} 1\n", 1, "badly escaped");
  expect_failure("foo{a=\"b\n", 1, "badly escaped");
}

TEST(OpenmetricsParser, MalformedMetadataIsReported) {
  expect_failure("# TYPE foo sometimes\n", 1, "unknown type 'sometimes'");
  expect_failure("# TYPE foo gauge\n# TYPE foo gauge\n", 2, "second '# TYPE'");
  expect_failure("# HELP foo a\n# HELP foo b\n", 2, "second '# HELP'");
  expect_failure("# TYPE foo gauge extra\n", 1, "unexpected text");
  expect_failure("# TYPE\n", 1, "expected a metric name");
  expect_failure("# UNIT 9foo bytes\n", 1, "expected a metric name");
}

TEST(OpenmetricsParser, MetadataAfterSamplesOrSplitFamiliesAreReported) {
  // Both formats keep a family's lines together and its metadata first.
  expect_failure("foo 1\n# TYPE foo gauge\n", 2, "already declared or sampled");
  expect_failure("# TYPE foo gauge\nfoo 1\n# TYPE bar gauge\nbar 1\n# HELP foo late\n", 5, "already declared or sampled");
  expect_failure("# TYPE foo gauge\n# TYPE foo_total counter\n", 2, "already a family");
}

TEST(OpenmetricsParser, NothingMayFollowEof) {
  expect_failure("foo 1\n# EOF\nbar 2\n", 3, "after '# EOF'");
  expect_failure("foo 1\n# EOF now\n", 2, "after '# EOF'");
  const om::result trailing = om::parse("foo 1\n# EOF\n\n");
  EXPECT_TRUE(trailing.ok()) << trailing.error;
}

TEST(OpenmetricsParser, ErrorKeepsTheFamiliesReadBeforeIt) {
  const om::result parsed = om::parse(
      "# TYPE good gauge\n"
      "good 1\n"
      "bad{ 2\n"
      "later 3\n");
  EXPECT_FALSE(parsed.ok());
  EXPECT_EQ(parsed.error_line, 3u);
  ASSERT_EQ(parsed.families.size(), 1u);
  EXPECT_EQ(parsed.families[0].name, "good");
  EXPECT_EQ(parsed.sample_count, 1u);
}

TEST(OpenmetricsParser, EmptyBodyIsAnEmptyScrape) {
  const om::result parsed = om::parse("");
  EXPECT_TRUE(parsed.ok());
  EXPECT_TRUE(parsed.families.empty());
  EXPECT_FALSE(parsed.saw_eof);
}

// --- bounded input ----------------------------------------------------------

TEST(OpenmetricsParser, TruncatedBodyIsReportedEvenWhenTheLastLineReads) {
  // `foo 12` cut to `foo 1` is a perfectly good sample, so a body that stops
  // without a line feed is refused rather than read.
  expect_failure("# TYPE foo gauge\nfoo 1", 2, "middle of a line");
  expect_failure("# TYPE foo gauge\nfoo{a=\"some", 2, "middle of a line");
  expect_failure("# TYPE foo gauge\nfoo 1\n# HE", 3, "middle of a line");
}

TEST(OpenmetricsParser, TruncatedAtALineBoundaryIsMissingOnlyItsEof) {
  // Nothing in the body itself can tell this apart from a Prometheus text
  // exposition, which has no terminator; the caller knows which content type
  // it was answered with.
  const om::result parsed = om::parse("# TYPE foo gauge\nfoo 1\n");
  EXPECT_TRUE(parsed.ok());
  EXPECT_FALSE(parsed.saw_eof);
}

TEST(OpenmetricsParser, EofWithoutItsLineFeedIsAccepted) {
  const om::result parsed = om::parse("foo 1\n# EOF");
  EXPECT_TRUE(parsed.ok()) << parsed.error;
  EXPECT_TRUE(parsed.saw_eof);
}

TEST(OpenmetricsParser, SixtyFourMegabyteLineIsReadInOnePass) {
  const std::size_t size = 64u * 1024u * 1024u;
  std::string body = "big{blob=\"";
  body.append(size, 'x');
  body += "\"} 1\n";
  const om::result parsed = om::parse(body);
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  ASSERT_EQ(parsed.families.size(), 1u);
  EXPECT_EQ(parsed.families[0].samples[0].labels[0].second.size(), size);
}

TEST(OpenmetricsParser, LineOverTheLimitIsRefusedUnread) {
  std::string body = "small 1\nbig{blob=\"";
  body.append(64u * 1024u * 1024u, 'x');
  body += "\"} 1\n";
  om::limits bounds;
  bounds.max_line_bytes = 1024 * 1024;
  const om::result parsed = om::parse(body, bounds);
  EXPECT_FALSE(parsed.ok());
  EXPECT_EQ(parsed.error_line, 2u);
  EXPECT_NE(parsed.error.find("longer than 1048576 bytes"), std::string::npos) << parsed.error;
  EXPECT_EQ(parsed.sample_count, 1u);
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
  const om::result parsed = om::parse(many_series(100000));
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  EXPECT_EQ(parsed.sample_count, 100000u);
  const om::family &f = only_family(parsed);
  EXPECT_EQ(f.samples.size(), 100000u);
  EXPECT_DOUBLE_EQ(f.samples.back().value, 99999);
}

TEST(OpenmetricsParser, SeriesOverTheLimitStopTheParse) {
  om::limits bounds;
  bounds.max_series = 50000;
  const om::result parsed = om::parse(many_series(100000), bounds);
  EXPECT_FALSE(parsed.ok());
  EXPECT_NE(parsed.error.find("more than 50000 series"), std::string::npos) << parsed.error;
  EXPECT_EQ(parsed.sample_count, 50000u);
  // Two metadata lines, then the 50001st sample.
  EXPECT_EQ(parsed.error_line, 50003u);
}
