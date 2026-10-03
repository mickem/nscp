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
#include <ostream>
#include <random>
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

TEST(OpenmetricsParser, KeywordGluedToItsNameIsJustAComment) {
  // `# TYPEfoo` is not a `# TYPE` line, so it declares nothing - the sample
  // after it is an unknown family, not a gauge.
  const om::result parsed = om::parse("# TYPEfoo gauge\nfoo 1\n");
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

// Every way a line can be malformed that the grammar distinguishes, one named
// case each, so a regression reports which input it broke on rather than "one
// of the forty in this table". Each case asserts the line the parser stopped on
// and a fragment of the reason, because both are what reaches the operator.
struct malformed {
  const char *name;
  std::string body;
  std::size_t line;
  const char *fragment;
};

void PrintTo(const malformed &m, std::ostream *os) { *os << m.name; }

class OpenmetricsParserMalformed : public ::testing::TestWithParam<malformed> {};

TEST_P(OpenmetricsParserMalformed, IsReportedOnItsLine) {
  const malformed &m = GetParam();
  const om::result parsed = om::parse(m.body);
  EXPECT_FALSE(parsed.ok()) << m.body;
  EXPECT_EQ(parsed.error_line, m.line) << m.body << " -> " << parsed.error;
  EXPECT_NE(parsed.error.find(m.fragment), std::string::npos) << m.body << " -> " << parsed.error;
}

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
    [](const ::testing::TestParamInfo<malformed> &info) { return std::string(info.param.name); });

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
        malformed{"EscapeTab", "foo{a=\"\\t\"} 1\n", 1, "badly escaped value for label 'a'"},
        malformed{"EscapeHex", "foo{a=\"\\x41\"} 1\n", 1, "badly escaped"}, malformed{"EscapeUnicode", "foo{a=\"\\u0041\"} 1\n", 1, "badly escaped"},
        malformed{"EscapeSingleQuote", "foo{a=\"\\'\"} 1\n", 1, "badly escaped"}, malformed{"BackslashAtEndOfLine", "foo{a=\"b\\\n", 1, "badly escaped"},
        malformed{"EscapedClosingQuote", "foo{a=\"b\\\"} 1\n", 1, "badly escaped"},
        malformed{"UnterminatedValue", "foo{a=\"b} 1\n", 1, "unterminated or badly escaped"},
        malformed{"ValueRunsIntoNextLine", "foo{a=\"b\nc\"} 1\n", 1, "unterminated or badly escaped"}),
    [](const ::testing::TestParamInfo<malformed> &info) { return std::string(info.param.name); });

// Values and timestamps: the decimal float grammar, or a non-finite word.
INSTANTIATE_TEST_SUITE_P(
    Values, OpenmetricsParserMalformed,
    ::testing::Values(malformed{"Word", "foo abc\n", 1, "invalid value 'abc' for 'foo'"}, malformed{"Unit", "foo 12ms\n", 1, "invalid value '12ms'"},
                      malformed{"Hex", "foo 0x10\n", 1, "invalid value '0x10'"}, malformed{"HexFloat", "foo 0x1p3\n", 1, "invalid value"},
                      malformed{"TwoDots", "foo 1.2.3\n", 1, "invalid value '1.2.3'"}, malformed{"DecimalComma", "foo 1,5\n", 1, "invalid value '1,5'"},
                      malformed{"DigitSeparator", "foo 1_000\n", 1, "invalid value '1_000'"}, malformed{"DoubleSign", "foo --1\n", 1, "invalid value '--1'"},
                      malformed{"SignOnly", "foo +\n", 1, "invalid value '+'"}, malformed{"DotOnly", "foo .\n", 1, "invalid value '.'"},
                      malformed{"ExponentOnly", "foo e5\n", 1, "invalid value 'e5'"}, malformed{"EmptyExponent", "foo 1e\n", 1, "invalid value '1e'"},
                      malformed{"SignedEmptyExponent", "foo 1e+\n", 1, "invalid value '1e+'"},
                      malformed{"FractionalExponent", "foo 1e2.5\n", 1, "invalid value"}, malformed{"Overflow", "foo 1e400\n", 1, "invalid value '1e400'"},
                      malformed{"NegativeOverflow", "foo -1e400\n", 1, "invalid value '-1e400'"},
                      malformed{"NanSuffix", "foo nanx\n", 1, "invalid value 'nanx'"}, malformed{"Infinite", "foo infinite\n", 1, "invalid value 'infinite'"},
                      malformed{"DoubleSignInf", "foo ++Inf\n", 1, "invalid value '++Inf'"}, malformed{"BareCarriageReturn", "foo 1\r\r\n", 1, "invalid value"},
                      malformed{"TimestampWord", "foo 1 soon\n", 1, "invalid timestamp 'soon'"},
                      malformed{"TimestampWithUnit", "foo 1 1700000000s\n", 1, "invalid timestamp"},
                      malformed{"TimestampOverflow", "foo 1 1e400\n", 1, "invalid timestamp"},
                      malformed{"ThirdToken", "foo 1 2 3\n", 1, "unexpected text after the value of 'foo'"},
                      malformed{"ThirdTokenAfterLabels", "foo{a=\"b\"} 1 2 x\n", 1, "unexpected text"}),
    [](const ::testing::TestParamInfo<malformed> &info) { return std::string(info.param.name); });

// Metadata lines and how they relate to the families around them.
INSTANTIATE_TEST_SUITE_P(
    Metadata, OpenmetricsParserMalformed,
    ::testing::Values(malformed{"UnknownType", "# TYPE foo sometimes\n", 1, "unknown type 'sometimes'"},
                      malformed{"TypeCaseMatters", "# TYPE foo Counter\n", 1, "unknown type 'Counter'"},
                      malformed{"MissingType", "# TYPE foo\n", 1, "unknown type ''"},
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
                      malformed{"SecondTypeUnderSampleName", "# TYPE foo_total counter\n# TYPE foo_total counter\n", 2, "second '# TYPE'"},
                      malformed{"TypeAfterSamples", "foo 1\n# TYPE foo gauge\n", 2, "already declared or sampled"},
                      malformed{"HelpAfterSamples", "# TYPE foo gauge\nfoo 1\n# HELP foo late\n", 3, "already declared or sampled"},
                      malformed{"FamilySplitInTwo", "# TYPE foo gauge\nfoo 1\n# TYPE bar gauge\nbar 1\n# HELP foo late\n", 5, "already declared or sampled"},
                      malformed{"FoldCollidesWithFamily", "# TYPE foo gauge\n# TYPE foo_total counter\n", 2, "folds onto 'foo', which is already a family"},
                      malformed{"InfoFoldCollidesWithFamily", "# TYPE build gauge\nbuild 1\n# TYPE build_info info\n", 3, "already a family"},
                      malformed{"RedeclaredUnderSampleName", "# TYPE foo_total counter\nfoo_total 1\n# HELP foo_total again\n", 3,
                                "already declared or sampled"}),
    [](const ::testing::TestParamInfo<malformed> &info) { return std::string(info.param.name); });

// `# EOF` ends the document; nothing but blank lines may follow it.
INSTANTIATE_TEST_SUITE_P(Eof, OpenmetricsParserMalformed,
                         ::testing::Values(malformed{"SampleAfterEof", "foo 1\n# EOF\nbar 2\n", 3, "text after '# EOF'"},
                                           malformed{"CommentAfterEof", "foo 1\n# EOF\n# comment\n", 3, "text after '# EOF'"},
                                           malformed{"SecondEof", "foo 1\n# EOF\n# EOF\n", 3, "text after '# EOF'"},
                                           malformed{"MetadataAfterEof", "# EOF\n# TYPE foo gauge\n", 2, "text after '# EOF'"},
                                           malformed{"EofTrailingText", "foo 1\n# EOF now\n", 2, "unexpected text after '# EOF'"}),
                         [](const ::testing::TestParamInfo<malformed> &info) { return std::string(info.param.name); });

// A body that stops without its final line feed was cut off.
INSTANTIATE_TEST_SUITE_P(
    Truncation, OpenmetricsParserMalformed,
    ::testing::Values(malformed{"MidValue", "# TYPE foo gauge\nfoo 1", 2, "middle of a line"},
                      malformed{"MidLabelValue", "# TYPE foo gauge\nfoo{a=\"some", 2, "middle of a line"},
                      malformed{"MidName", "foo 1\nfo", 2, "middle of a line"}, malformed{"MidMetadata", "foo 1\n# HE", 2, "middle of a line"},
                      malformed{"MidEof", "foo 1\n# EO", 2, "middle of a line"}, malformed{"MidComment", "foo 1\n# just a comment", 2, "middle of a line"},
                      malformed{"BlanksOnly", "foo 1\n   ", 2, "middle of a line"}, malformed{"SingleLineNoFeed", "foo 1", 1, "middle of a line"}),
    [](const ::testing::TestParamInfo<malformed> &info) { return std::string(info.param.name); });

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

TEST(OpenmetricsParser, FailingSampleIsNotAddedToItsFamily) {
  // The label set is read before the duplicate is found; none of it may leak
  // into the family the sample would have joined.
  const om::result parsed = om::parse(
      "# TYPE foo gauge\n"
      "foo{a=\"1\"} 1\n"
      "foo{a=\"2\",a=\"3\"} 2\n");
  EXPECT_FALSE(parsed.ok());
  ASSERT_EQ(parsed.families.size(), 1u);
  ASSERT_EQ(parsed.families[0].samples.size(), 1u);
  EXPECT_EQ(parsed.sample_count, 1u);
}

TEST(OpenmetricsParser, ErrorLineCountsBlankCommentAndCrlfLines) {
  const om::result parsed = om::parse("# hello\r\n\r\n\nfoo 1\r\n   \nbar x\r\n");
  EXPECT_FALSE(parsed.ok());
  EXPECT_EQ(parsed.error_line, 6u) << parsed.error;
}

TEST(OpenmetricsParser, OnlyTheFirstErrorIsReported) {
  const om::result parsed = om::parse("a 1\nb x\nc{ 3\n");
  EXPECT_EQ(parsed.error_line, 2u);
  EXPECT_NE(parsed.error.find("invalid value 'x'"), std::string::npos) << parsed.error;
}

TEST(OpenmetricsParser, NothingMayFollowEofButBlankLines) {
  const om::result trailing = om::parse("foo 1\n# EOF\n\n  \n");
  EXPECT_TRUE(trailing.ok()) << trailing.error;
  EXPECT_TRUE(trailing.saw_eof);
}

TEST(OpenmetricsParser, EmptyBodyIsAnEmptyScrape) {
  const om::result parsed = om::parse("");
  EXPECT_TRUE(parsed.ok());
  EXPECT_TRUE(parsed.families.empty());
  EXPECT_FALSE(parsed.saw_eof);
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
  std::size_t samples = 0;
  for (const om::family &f : parsed.families) {
    // A family is only created to hold a sample or metadata, and every sample
    // carries the family's name as its prefix.
    EXPECT_FALSE(f.name.empty());
    for (const om::sample &s : f.samples) {
      EXPECT_EQ(s.name.compare(0, f.name.size(), f.name), 0) << s.name << " in " << f.name;
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

}  // namespace

TEST(OpenmetricsParser, EveryTruncationIsRefusedOrEndsOnALine) {
  // Cut a valid exposition at every byte. A prefix that ends on a line feed is
  // a shorter valid exposition; any other prefix was cut mid-line and must be
  // refused - never read as a sample that happens to parse.
  const std::string body = exposition;
  ASSERT_TRUE(om::parse(body).ok());
  for (std::size_t length = 0; length < body.size(); ++length) {
    const std::string prefix = body.substr(0, length);
    const om::result parsed = om::parse(prefix);
    expect_consistent(parsed, prefix);
    const bool on_a_line = prefix.empty() || prefix[prefix.size() - 1] == '\n' || (prefix.size() >= 5 && prefix.compare(prefix.size() - 5, 5, "# EOF") == 0);
    if (on_a_line) {
      EXPECT_TRUE(parsed.ok()) << "cut at " << length << ": " << parsed.error;
    } else {
      EXPECT_FALSE(parsed.ok()) << "cut at " << length << " was read";
      EXPECT_NE(parsed.error.find("middle of a line"), std::string::npos) << "cut at " << length << ": " << parsed.error;
    }
    EXPECT_FALSE(parsed.saw_eof && length < body.size() - 6);
  }
}

TEST(OpenmetricsParser, EverySingleByteCorruptionIsHandled) {
  // Overwrite every byte with each character the grammar gives a meaning to,
  // and a few it does not. Most results are errors; all must be consistent.
  const std::string body = exposition;
  const char replacements[] = {'"', '\\', '{', '}', '=', ',', ' ', '\t', '#', '\n', '\r', '\0', '\x7f', '\xff', 'x', '1', '.', 'e', '-', '+'};
  std::size_t failures = 0;
  for (std::size_t at = 0; at < body.size(); ++at) {
    for (const char replacement : replacements) {
      std::string corrupt = body;
      corrupt[at] = replacement;
      const om::result parsed = om::parse(corrupt);
      expect_consistent(parsed, corrupt);
      if (!parsed.ok()) ++failures;
    }
  }
  // Not a precise number - only proof that the corruption is being noticed
  // rather than read through.
  EXPECT_GT(failures, body.size());
}

TEST(OpenmetricsParser, RandomBytesAreHandled) {
  // Short random documents drawn mostly from the grammar's own characters, so
  // they get past the first token often enough to reach the deeper states.
  // Fixed seed: a failure here reproduces.
  const std::string alphabet = "abc_:{}=,\"\\ \t#\n\r.0123456789eE+-InfNa\xff";
  std::mt19937 random(1499);
  std::uniform_int_distribution<std::size_t> pick(0, alphabet.size() - 1);
  std::uniform_int_distribution<std::size_t> length(0, 120);
  for (int round = 0; round < 20000; ++round) {
    std::string body;
    const std::size_t size = length(random);
    for (std::size_t i = 0; i < size; ++i) body += alphabet[pick(random)];
    const om::result parsed = om::parse(body);
    expect_consistent(parsed, body);
    if (::testing::Test::HasFailure()) {
      ADD_FAILURE() << "round " << round << " body: " << body;
      return;
    }
  }
}

TEST(OpenmetricsParser, RandomLinesSplicedIntoAValidBodyAreHandled) {
  // A valid body with one of its lines swapped for a random one: the families
  // before the splice must survive whatever the spliced line does.
  const std::string alphabet = "abc_:{}=,\"\\ \t#.0123456789eE+-";
  std::mt19937 random(1623);
  std::uniform_int_distribution<std::size_t> pick(0, alphabet.size() - 1);
  std::uniform_int_distribution<std::size_t> length(1, 40);
  for (int round = 0; round < 5000; ++round) {
    std::string line;
    const std::size_t size = length(random);
    for (std::size_t i = 0; i < size; ++i) line += alphabet[pick(random)];
    const std::string body = std::string("# TYPE kept gauge\nkept 1\n") + line + "\n";
    const om::result parsed = om::parse(body);
    expect_consistent(parsed, body);
    ASSERT_FALSE(parsed.families.empty()) << line;
    EXPECT_EQ(parsed.families[0].name, "kept") << line;
    EXPECT_GE(parsed.families[0].samples.size(), 1u) << line;
    if (!parsed.ok()) {
      EXPECT_EQ(parsed.error_line, 3u) << line << " -> " << parsed.error;
    }
  }
}

// --- bounded input ----------------------------------------------------------

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
