// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Every way a parse can fail, one row each: the smallest body that reaches
// that refusal, the exact message, the line it is reported on, and the
// families that must survive it. The last test reads the parser's source and
// fails when it can report a message no row here produces, so a new refusal
// cannot be added without the body that reaches it.

#include <gtest/gtest.h>

#include <fstream>
#include <functional>
#include <metrics/openmetrics_parser.hpp>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace om = metrics::openmetrics;

namespace {

const om::format openmetrics = om::format::openmetrics_1_0;
const om::format text = om::format::prometheus_text_0_0_4;

struct refusal {
  const char *case_name;
  om::format format;
  std::string body;
  // The whole message, exactly.
  std::string error;
  std::size_t line;
  // The families the parse must keep, by name, in order.
  std::vector<std::string> kept;
  // Applied to default limits.
  std::function<void(om::limits &)> limit;
};

om::limits limits_for(const refusal &r) {
  om::limits bounds;
  if (r.limit) r.limit(bounds);
  return bounds;
}

const std::string long_name = "\"" + std::string(10, 'v') + "\"";

std::vector<refusal> refusals() {
  return {
      // --- reading lines ---------------------------------------------------
      {"CutLine", text, "a 1", "the body ends in the middle of a line", 1, {}, {}},
      {"CutLineAfterAFamily", text, "a 1\nb 2", "the body ends in the middle of a line", 2, {"a"}, {}},
      {"CutLineAfterEofThatIsNotBlank", openmetrics, "# EOF\nx", "the body ends in the middle of a line", 2, {}, {}},
      {"CutTextEof", text, "a 1\n# EOF", "the body ends in the middle of a line", 2, {"a"}, {}},
      {"LineTooLong", text, "a 1\nabc 123456\n", "line longer than 8 bytes", 2, {"a"}, [](om::limits &l) { l.max_line_bytes = 8; }},
      {"LineTooLongByItsCarriageReturn", text, "a 12345\r\n", "line longer than 7 bytes", 1, {}, [](om::limits &l) { l.max_line_bytes = 7; }},
      {"TextAfterEof", openmetrics, "a 1\n# EOF\nb 1\n", "text after '# EOF'", 3, {"a"}, {}},
      {"CommentAfterEof", openmetrics, "# EOF\n# a comment\n", "text after '# EOF'", 2, {}, {}},
      {"SecondEof", openmetrics, "# EOF\n# EOF\n", "text after '# EOF'", 2, {}, {}},
      {"EofWithText", openmetrics, "a 1\n# EOF now\n", "unexpected text after '# EOF'", 2, {"a"}, {}},

      // --- metadata lines --------------------------------------------------
      {"MetadataWithoutName", text, "# HELP\n", "expected a metric name after '# HELP'", 1, {}, {}},
      {"MetadataNameStartsWithDigit", text, "# TYPE 1a gauge\n", "expected a metric name after '# TYPE'", 1, {}, {}},
      {"MetadataKeywordGluedToName", openmetrics, "# UNIT\ta s\n# UNITb s\n# UNIT\n", "expected a metric name after '# UNIT'", 3, {"a"}, {}},
      {"MetadataNameWithDot", text, "# HELP a.b x\n", "invalid character in metric name 'a.b'", 1, {}, {}},
      {"TrailingTextAfterType", text, "# TYPE a gauge extra\n", "unexpected text after the type of 'a'", 1, {}, {}},
      {"TrailingTextAfterUnit", openmetrics, "# UNIT a seconds extra\n", "unexpected text after the unit of 'a'", 1, {}, {}},
      {"UnknownType", text, "# TYPE a gauges\n", "unknown type 'gauges' for 'a'", 1, {}, {}},
      {"OpenMetricsTypeInText", text, "# TYPE a info\n", "unknown type 'info' for 'a'", 1, {}, {}},
      {"TextTypeInOpenMetrics", openmetrics, "# TYPE a untyped\n", "unknown type 'untyped' for 'a'", 1, {}, {}},
      {"MetadataForASample", openmetrics, "# TYPE a counter\n# HELP a_total x\n", "'a_total' is a sample of the family 'a'", 2, {"a"}, {}},
      {"MetadataAfterSamples", text, "a 1\n# TYPE a gauge\n", "metadata for 'a' after that family was already declared or sampled", 2, {"a"}, {}},
      {"MetadataForAnEarlierFamily",
       text,
       "# TYPE a gauge\na 1\nb 1\n# HELP a x\n",
       "metadata for 'a' after that family was already declared or sampled",
       4,
       {"a", "b"},
       {}},
      {"RepeatedNameInText",
       text,
       "# TYPE a gauge\na 1\n# TYPE a counter\n",
       "metadata for 'a' after that family was already declared or sampled",
       3,
       {"a"},
       {}},
      {"RepeatedNameAfterUntypedFamily",
       openmetrics,
       "a 1\n# TYPE a counter\n",
       "metadata for 'a' after that family was already declared or sampled",
       2,
       {"a"},
       {}},
      {"RepeatedNameThatCannotPair",
       openmetrics,
       "# TYPE a gauge\na 1\n# TYPE a summary\n",
       "metadata for 'a' after that family was already declared or sampled",
       3,
       {"a"},
       {}},
      {"NameRepeatedAThirdTime",
       openmetrics,
       "# TYPE a gauge\na 1\n# TYPE a counter\na_total 1\n# HELP a x\n",
       "metadata for 'a' after that family was already declared or sampled",
       5,
       {"a", "a"},
       {}},
      {"RepeatedNameBlockRefusedLater",
       openmetrics,
       "# TYPE a gauge\na 1\n# HELP a x\nb 1\n",
       "metadata for 'a' after that family was already declared or sampled",
       3,
       {"a"},
       {}},
      {"RepeatedNameBlockWithTypeThatCannotPair",
       openmetrics,
       "# TYPE a gauge\na 1\n# HELP a x\n# TYPE a gauge\n",
       "metadata for 'a' after that family was already declared or sampled",
       3,
       {"a"},
       {}},
      {"SecondHelp", text, "# HELP a x\n# HELP a y\n", "second '# HELP' line for 'a'", 2, {}, {}},
      {"SecondType", text, "# TYPE a gauge\n# TYPE a gauge\n", "second '# TYPE' line for 'a'", 2, {}, {}},
      {"SecondUnit", openmetrics, "# UNIT a s\n# UNIT a s\n", "second '# UNIT' line for 'a'", 2, {}, {}},
      {"SampleBeforeItsType", text, "a_bucket 1\n# TYPE a histogram\n", "'a_bucket' came before the '# TYPE' line of 'a'", 2, {"a_bucket"}, {}},
      {"TooManyFamiliesByMetadata", text, "# TYPE a gauge\n# TYPE b gauge\n", "more than 1 families", 2, {"a"}, [](om::limits &l) { l.max_families = 1; }},

      // --- sample lines ----------------------------------------------------
      {"SampleWithoutName", text, "1a 2\n", "expected a metric name at the start of the line", 1, {}, {}},
      {"SampleStartingWithLabels", text, "{a=\"1\"} 2\n", "expected a metric name at the start of the line", 1, {}, {}},
      {"SampleWithoutValue", text, "a\n", "missing value for 'a'", 1, {}, {}},
      {"SampleWithLabelsWithoutValue", text, "a{b=\"1\"}\n", "missing value for 'a'", 1, {}, {}},
      {"SampleNameWithDot", text, "a.b 1\n", "invalid character after 'a'", 1, {}, {}},
      {"ValueGluedToLabels", text, "a{b=\"1\"}2\n", "invalid character after 'a'", 1, {}, {}},
      {"InvalidValue", text, "a one\n", "invalid value 'one' for 'a'", 1, {}, {}},
      {"SignedNaN", openmetrics, "a -NaN\n", "invalid value '-NaN' for 'a'", 1, {}, {}},
      {"InvalidTimestamp", text, "a 1 soon\n", "invalid timestamp 'soon' for 'a'", 1, {}, {}},
      {"TextTimestampInSeconds", text, "a 1 1.5\n", "invalid timestamp '1.5' for 'a'", 1, {}, {}},
      {"TextTimestampThatOverflows", text, "a 1 " + std::string(400, '9') + "\n", "invalid timestamp '" + std::string(400, '9') + "' for 'a'", 1, {}, {}},
      {"OpenMetricsTimestampThatOverflows", openmetrics, "a 1 1e400\n", "invalid timestamp '1e400' for 'a'", 1, {}, {}},
      {"TextAfterTimestamp", text, "a 1 2 3\n", "unexpected text after the value of 'a'", 1, {}, {}},
      {"TextAfterOpenMetricsTimestamp", openmetrics, "a 1 2 3\n", "unexpected text after the value of 'a'", 1, {}, {}},
      {"ExemplarInText", text, "a 1 # {t=\"x\"} 1\n", "unexpected text after the value of 'a'", 1, {}, {}},
      {"TooManySeries", text, "a 1\na 2\n", "more than 1 series", 2, {"a"}, [](om::limits &l) { l.max_series = 1; }},
      {"TooManyFamiliesBySample", text, "a 1\nb 1\n", "more than 1 families", 2, {"a"}, [](om::limits &l) { l.max_families = 1; }},
      {"OpenMetricsFamilyResumed", openmetrics, "a 1\nb 1\na 2\n", "'a' comes after another family, apart from the rest of the family 'a'", 3, {"a", "b"}, {}},
      {"BareNameInHistogram", openmetrics, "# TYPE h histogram\nh 1\n", "'h' is not a sample of the histogram family 'h'", 2, {"h"}, {}},
      {"BareNameInCounter", openmetrics, "# TYPE c counter\nc 1\n", "'c' is not a sample of the counter family 'c'", 2, {"c"}, {}},

      // --- labels ----------------------------------------------------------
      {"LabelSetCutAtBrace", text, "a{\n", "unterminated label set on 'a'", 1, {}, {}},
      {"LabelSetCutAfterValue", text, "a{b=\"1\"\n", "unterminated label set on 'a'", 1, {}, {}},
      {"LabelSetCutAfterComma", text, "a{b=\"1\",\n", "unterminated label set on 'a'", 1, {}, {}},
      {"LabelNameStartsWithDigit", text, "a{1b=\"x\"} 1\n", "invalid label name on 'a'", 1, {}, {}},
      {"LabelNameWithColon", text, "a{b:c=\"x\"} 1\n", "expected '=' after label 'b' on 'a'", 1, {}, {}},
      {"LabelWithoutEquals", text, "a{b \"x\"} 1\n", "expected '=' after label 'b' on 'a'", 1, {}, {}},
      {"LabelValueUnquoted", text, "a{b=x} 1\n", "expected a quoted value for label 'b' on 'a'", 1, {}, {}},
      {"LabelValueCutAtEquals", text, "a{b=\n", "expected a quoted value for label 'b' on 'a'", 1, {}, {}},
      {"LabelValueUnclosed", text, "a{b=\"x} 1\n", "unterminated or badly escaped value for label 'b' on 'a'", 1, {}, {}},
      {"LabelValueBadEscape", text, "a{b=\"\\t\"} 1\n", "unterminated or badly escaped value for label 'b' on 'a'", 1, {}, {}},
      {"LabelValueEndsInBackslash", text, "a{b=\"x\\\n", "unterminated or badly escaped value for label 'b' on 'a'", 1, {}, {}},
      {"TooManyLabelsOnASample", text, "a{b=\"1\",c=\"2\"} 1\n", "more than 1 labels on 'a'", 1, {}, [](om::limits &l) { l.max_labels_per_sample = 1; }},
      {"TooManyLabelsInTheBody", text, "a{b=\"1\"} 1\na{b=\"2\"} 1\n", "more than 1 labels", 2, {"a"}, [](om::limits &l) { l.max_labels = 1; }},
      {"LabelsWithoutComma", text, "a{b=\"1\" c=\"2\"} 1\n", "expected ',' or '}' after label 'b' on 'a'", 1, {}, {}},
      {"LabelTwice", text, "a{b=\"1\",c=\"2\",b=\"3\"} 1\n", "label 'b' twice on 'a'", 1, {}, {}},
  };
}

class OpenmetricsParserRefusal : public ::testing::TestWithParam<refusal> {};

}  // namespace

TEST_P(OpenmetricsParserRefusal, IsReportedExactly) {
  const refusal &r = GetParam();
  const om::result parsed = om::parse(r.body, r.format, limits_for(r));
  EXPECT_FALSE(parsed.ok());
  EXPECT_EQ(parsed.error, r.error);
  EXPECT_EQ(parsed.error_line, r.line) << parsed.error;
  EXPECT_FALSE(parsed.saw_eof && r.format == text);
  std::vector<std::string> kept;
  for (const om::family &f : parsed.families) kept.push_back(f.name);
  EXPECT_EQ(kept, r.kept) << parsed.error;
  // Everything kept arrived before the line at fault: the body cut just
  // before that line keeps at least as many samples, family by family.
  std::size_t start = 0;
  for (std::size_t line = 1; line < r.line; ++line) start = r.body.find('\n', start) + 1;
  const om::result before = om::parse(r.body.substr(0, start), r.format, limits_for(r));
  ASSERT_LE(parsed.families.size(), before.families.size() + (r.line == 0 ? 0 : 1));
  std::size_t samples = 0;
  for (const om::family &f : parsed.families) samples += f.samples.size();
  EXPECT_EQ(samples, parsed.sample_count);
  EXPECT_LE(parsed.sample_count, before.sample_count);
}

INSTANTIATE_TEST_SUITE_P(EveryRefusal, OpenmetricsParserRefusal, ::testing::ValuesIn(refusals()),
                         [](const ::testing::TestParamInfo<refusal> &info) { return std::string(info.param.case_name); });

TEST(OpenmetricsParserRefusals, EveryMessageTheParserCanReportHasARow) {
  // The words of every message the parser source can fail with - each string
  // literal inside a `fail(...)` call, and in `declared_again` - must appear in
  // some row's message above. An internal error is the exception: it reports
  // a broken invariant, which no body can reach, and every other test fails
  // if one is ever reported (`parse_checked`).
  const std::string here = __FILE__;
  const std::string source_path = here.substr(0, here.find_last_of("/\\") + 1) + "openmetrics_parser.cpp";
  std::ifstream file(source_path);
  if (!file) GTEST_SKIP() << "parser source not found beside this test: " << source_path;
  std::stringstream buffer;
  buffer << file.rdbuf();
  const std::string source = buffer.str();

  std::string messages;
  for (const refusal &r : refusals()) messages += r.error + "\n";

  const std::regex call(R"((?:fail|fail_on)\(([^;]*)\);|return ("metadata for[^;]*);)");
  const std::regex literal(R"re("((?:[^"\\]|\\.)*)")re");
  std::set<std::string> checked;
  for (std::sregex_iterator it(source.begin(), source.end(), call), end; it != end; ++it) {
    const std::string arguments = (*it)[1].matched ? (*it)[1].str() : (*it)[2].str();
    for (std::sregex_iterator lit(arguments.begin(), arguments.end(), literal); lit != end; ++lit) {
      std::string words = (*lit)[1].str();
      // Undo the C++ escapes the literal itself needed.
      std::string unescaped;
      for (std::size_t i = 0; i < words.size(); ++i) {
        if (words[i] == '\\' && i + 1 < words.size()) ++i;
        unescaped += words[i];
      }
      if (unescaped.size() < 3 || unescaped.rfind("internal error", 0) == 0) continue;
      checked.insert(unescaped);
      EXPECT_NE(messages.find(unescaped), std::string::npos) << "no refusal row reports a message containing \"" << unescaped << "\"";
    }
  }
  // The sweep must have found the parser's messages at all.
  EXPECT_GT(checked.size(), 30u);
}
