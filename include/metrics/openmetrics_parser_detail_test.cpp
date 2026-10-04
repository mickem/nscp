// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The helpers the parser is built from, driven directly: every character of
// each character class, every edge of the number grammar, every type word in
// both formats, every suffix against every type. A whole body only reaches the
// inputs the parser hands these helpers today; these tests pin what each one
// does with any input, so that a later change to the parser cannot hand one a
// token it was never checked against.

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <metrics/openmetrics_parser_detail.hpp>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace om = metrics::openmetrics;
namespace omd = metrics::openmetrics::detail;

namespace {

const om::format openmetrics = om::format::openmetrics_1_0;
const om::format text = om::format::prometheus_text_0_0_4;

// Every byte value, against the set the grammar names for the class.
void expect_class(bool (*predicate)(char), const std::string &members, const char *name) {
  for (int i = 0; i < 256; ++i) {
    const char c = static_cast<char>(i);
    const bool expected = members.find(c) != std::string::npos;
    EXPECT_EQ(predicate(c), expected) << name << " of byte " << i;
  }
}

const std::string lower = "abcdefghijklmnopqrstuvwxyz";
const std::string upper = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
const std::string digits = "0123456789";

}  // namespace

TEST(OpenmetricsParserDetail, CharacterClassesHoldExactlyTheirMembers) {
  expect_class(omd::is_blank, " \t", "is_blank");
  expect_class(omd::is_alpha, lower + upper, "is_alpha");
  expect_class(omd::is_digit, digits, "is_digit");
  expect_class(omd::is_name_start, lower + upper + "_:", "is_name_start");
  expect_class(omd::is_name_char, lower + upper + digits + "_:", "is_name_char");
  expect_class(omd::is_label_start, lower + upper + "_", "is_label_start");
  expect_class(omd::is_label_char, lower + upper + digits + "_", "is_label_char");
}

TEST(OpenmetricsParserDetail, NulIsInNoClass) {
  // `cursor::peek()` answers `\0` at the end of a line, so the parser relies
  // on no class taking it - and the byte-by-byte sweep above includes it, but
  // this is the reason it matters.
  for (bool (*predicate)(char) :
       {omd::is_blank, omd::is_alpha, omd::is_digit, omd::is_name_start, omd::is_name_char, omd::is_label_start, omd::is_label_char}) {
    EXPECT_FALSE(predicate('\0'));
  }
}

TEST(OpenmetricsParserDetail, CharAtIsNulPastTheEnd) {
  const std::string_view abc("abc");
  EXPECT_EQ(omd::char_at(abc, 0), 'a');
  EXPECT_EQ(omd::char_at(abc, 2), 'c');
  EXPECT_EQ(omd::char_at(abc, 3), '\0');
  EXPECT_EQ(omd::char_at(abc, 4), '\0');
  EXPECT_EQ(omd::char_at(std::string_view(), 0), '\0');
  // A view into a longer string ends where the view does, not the string.
  const std::string backing = "abcdef";
  EXPECT_EQ(omd::char_at(std::string_view(backing).substr(0, 3), 3), '\0');
}

TEST(OpenmetricsParserDetail, AsciiLowerFoldsOnlyTheUpperCaseLetters) {
  for (int i = 0; i < 256; ++i) {
    const char c = static_cast<char>(i);
    const std::size_t at = upper.find(c);
    const char expected = at == std::string::npos ? c : lower.at(at);
    EXPECT_EQ(omd::ascii_lower(c), expected) << "byte " << i;
  }
}

TEST(OpenmetricsParserDetail, EndsWithNeedsSomethingBeforeTheSuffix) {
  EXPECT_TRUE(omd::ends_with("abc", "bc"));
  EXPECT_TRUE(omd::ends_with("abc", ""));
  EXPECT_FALSE(omd::ends_with("bc", "bc"));
  EXPECT_FALSE(omd::ends_with("c", "bc"));
  EXPECT_FALSE(omd::ends_with("abc", "xc"));
  EXPECT_FALSE(omd::ends_with("abc", "ab"));
  EXPECT_FALSE(omd::ends_with("", ""));
}

TEST(OpenmetricsParserDetail, TrimBlanksTakesSpacesAndTabsOnly) {
  EXPECT_EQ(omd::trim_blanks(""), "");
  EXPECT_EQ(omd::trim_blanks(" \t "), "");
  EXPECT_EQ(omd::trim_blanks(" \ta b\t "), "a b");
  EXPECT_EQ(omd::trim_blanks("a"), "a");
  EXPECT_EQ(omd::trim_blanks("\ra\r"), "\ra\r");
  EXPECT_EQ(omd::trim_blanks("\na"), "\na");
}

TEST(OpenmetricsParserDetail, NormaliseDropsOneCarriageReturnThenTheBlanks) {
  EXPECT_EQ(omd::normalise(""), "");
  EXPECT_EQ(omd::normalise("\r"), "");
  EXPECT_EQ(omd::normalise("a\r"), "a");
  EXPECT_EQ(omd::normalise(" a \r"), "a");
  EXPECT_EQ(omd::normalise("a\r\r"), "a\r");
  EXPECT_EQ(omd::normalise("a\r "), "a\r");
  EXPECT_EQ(omd::normalise("\ra"), "\ra");
  EXPECT_EQ(omd::normalise(" \t a b \t "), "a b");
}

TEST(OpenmetricsParserDetail, EofMarkerIsTheCommentEofAlone) {
  for (const char *yes : {"# EOF", "#EOF", "#   EOF", "#\tEOF\t", "# EOF  "}) EXPECT_TRUE(omd::is_eof_marker(yes)) << yes;
  for (const char *no : {"", "#", "EOF", "# eof", "# EOF x", "# EOFx", "## EOF", " # EOF", "# E OF"}) EXPECT_FALSE(omd::is_eof_marker(no)) << no;
}

TEST(OpenmetricsParserDetail, EqualsWordFoldsAsciiCaseOnly) {
  EXPECT_TRUE(omd::equals_word("", ""));
  EXPECT_TRUE(omd::equals_word("nan", "nan"));
  EXPECT_TRUE(omd::equals_word("NaN", "nan"));
  EXPECT_TRUE(omd::equals_word("+INF", "+inf"));
  EXPECT_TRUE(omd::equals_word("AZ", "az"));
  EXPECT_FALSE(omd::equals_word("na", "nan"));
  EXPECT_FALSE(omd::equals_word("nanx", "nan"));
  EXPECT_FALSE(omd::equals_word("nbn", "nan"));
  EXPECT_FALSE(omd::equals_word("", "nan"));
  // The bytes just outside `A`-`Z` are not letters, so they are not folded
  // onto the bytes 32 above them.
  EXPECT_FALSE(omd::equals_word("@", "`"));
  EXPECT_FALSE(omd::equals_word("[", "{"));
  // And a lower-case input is not folded down.
  EXPECT_FALSE(omd::equals_word("a", "A"));
}

TEST(OpenmetricsParserDetail, DecimalGrammarAcceptsEachForm) {
  for (const char *yes : {"0",   "1",   "+1",  "-1",   "007",  "1.",    "1.5",  ".5",   "+.5",     "-.5",
                          "-1.", "1e5", "1E5", "1e+5", "1e-5", "1E-05", ".5e1", "1.e1", "+1.5e-3", "123456789012345678901234567890"}) {
    EXPECT_TRUE(omd::is_decimal(yes)) << yes;
  }
  for (const char *no : {"",      "+",  "-",  ".",    "+.",  "-.",  "e1",  ".e1", "1e",   "1e+", "1e-",  "1.5.5", "1x", "x1", "--1", "+-1", "1e5.5",
                         "1e5e5", " 1", "1 ", "0x10", "inf", "nan", "1_0", "1,5", "1e 5", "+e1", "1ee5", "1..5",  "e",  "E",  "1E",  "..5"}) {
    EXPECT_FALSE(omd::is_decimal(no)) << no;
  }
}

TEST(OpenmetricsParserDetail, IntegerGrammarAcceptsSignedDigitsOnly) {
  for (const char *yes : {"0", "+0", "-0", "12", "-12", "+12", "007", "99999999999999999999999"}) EXPECT_TRUE(omd::is_integer(yes)) << yes;
  for (const char *no : {"", "+", "-", "1.0", "1.", ".1", "1e3", " 1", "1 ", "--1", "+-1", "a", "1a", "0x1"}) EXPECT_FALSE(omd::is_integer(no)) << no;
}

TEST(OpenmetricsParserDetail, ToDoubleReadsAllOfAFiniteNumber) {
  double out = -1;
  EXPECT_TRUE(omd::to_double("1.5", out));
  EXPECT_DOUBLE_EQ(out, 1.5);
  EXPECT_TRUE(omd::to_double("-0", out));
  EXPECT_EQ(out, 0);
  EXPECT_TRUE(std::signbit(out));
  EXPECT_TRUE(omd::to_double("1e-400", out)) << "underflow is read as the nearest value";
  EXPECT_EQ(out, 0);
  EXPECT_TRUE(omd::to_double("4.9e-324", out));
  EXPECT_GT(out, 0);
  // Not all of the token is a number, or none of it is.
  out = 7;
  EXPECT_FALSE(omd::to_double("1x", out));
  EXPECT_FALSE(omd::to_double("1 ", out));
  EXPECT_FALSE(omd::to_double("x", out));
  EXPECT_FALSE(omd::to_double("", out));
  // Overflow: the non-finite values have their own spellings.
  EXPECT_FALSE(omd::to_double("1e400", out));
  EXPECT_FALSE(omd::to_double("-1e400", out));
  EXPECT_FALSE(omd::to_double("inf", out));
  EXPECT_FALSE(omd::to_double("nan", out));
  EXPECT_EQ(out, 7) << "a refused token leaves the output alone";
}

TEST(OpenmetricsParserDetail, ToDoubleBufferBoundary) {
  // The stack buffer holds 63 characters and a terminator; from 64 on the
  // token is copied onto the heap. Each side converts the same, and a token
  // that is not all number is refused on each side.
  for (std::size_t length = 60; length <= 68; ++length) {
    const std::string value = "1." + std::string(length - 2, '0');
    double out = 0;
    EXPECT_TRUE(omd::to_double(value, out)) << length;
    EXPECT_EQ(out, 1) << length;
    EXPECT_FALSE(omd::to_double(value + "x", out)) << length;
  }
}

TEST(OpenmetricsParserDetail, ValueReadsTheNonFiniteSpellingsAndDecimals) {
  double out = 0;
  for (const char *nan : {"nan", "NaN", "NAN", "nAn"}) {
    EXPECT_TRUE(omd::read_value(nan, out)) << nan;
    EXPECT_TRUE(std::isnan(out)) << nan;
  }
  for (const char *inf : {"inf", "Inf", "+inf", "+Inf", "infinity", "+Infinity", "INFINITY"}) {
    EXPECT_TRUE(omd::read_value(inf, out)) << inf;
    EXPECT_TRUE(std::isinf(out) && out > 0) << inf;
  }
  for (const char *inf : {"-inf", "-Inf", "-infinity", "-INFINITY"}) {
    EXPECT_TRUE(omd::read_value(inf, out)) << inf;
    EXPECT_TRUE(std::isinf(out) && out < 0) << inf;
  }
  EXPECT_TRUE(omd::read_value("-2.5e3", out));
  EXPECT_DOUBLE_EQ(out, -2500);
  for (const char *no : {"", "+nan", "-nan", "-NaN", "na", "infinit", "infinityx", "+-inf", "--inf", "1e400", "0x1", "1,5", "x"}) {
    EXPECT_FALSE(omd::read_value(no, out)) << no;
  }
}

TEST(OpenmetricsParserDetail, TimestampIsSecondsOrMillisecondsAndAlwaysFinite) {
  double out = 0;
  EXPECT_TRUE(omd::read_timestamp("1700000000.5", openmetrics, out));
  EXPECT_DOUBLE_EQ(out, 1700000000.5);
  EXPECT_TRUE(omd::read_timestamp("1.7e9", openmetrics, out));
  EXPECT_TRUE(omd::read_timestamp("-5", openmetrics, out));
  EXPECT_TRUE(omd::read_timestamp("1700000000500", text, out));
  EXPECT_DOUBLE_EQ(out, 1700000000500.0);
  EXPECT_TRUE(omd::read_timestamp("-5", text, out));
  const std::string huge(400, '9');
  for (const om::format f : {openmetrics, text}) {
    for (const std::string &no : {std::string(), std::string("x"), std::string("inf"), std::string("nan"), std::string("+Inf"), huge, std::string("1e400")}) {
      EXPECT_FALSE(omd::read_timestamp(no, f, out)) << no.substr(0, 20);
    }
  }
  // Each format's own unit only.
  EXPECT_FALSE(omd::read_timestamp("1.5", text, out));
  EXPECT_FALSE(omd::read_timestamp("1e3", text, out));
}

TEST(OpenmetricsParserDetail, TypeWordsAreThoseOfTheFormat) {
  const std::vector<std::pair<const char *, om::family_type> > shared = {{"counter", om::family_type::counter},
                                                                         {"gauge", om::family_type::gauge},
                                                                         {"histogram", om::family_type::histogram},
                                                                         {"summary", om::family_type::summary}};
  const std::vector<std::pair<const char *, om::family_type> > openmetrics_only = {{"gaugehistogram", om::family_type::gaugehistogram},
                                                                                   {"info", om::family_type::info},
                                                                                   {"stateset", om::family_type::stateset},
                                                                                   {"unknown", om::family_type::unknown}};
  for (const om::format f : {openmetrics, text}) {
    for (const auto &word : shared) {
      om::family_type out = om::family_type::stateset;
      EXPECT_TRUE(omd::parse_type(word.first, f, out)) << word.first;
      EXPECT_EQ(out, word.second) << word.first;
    }
  }
  for (const auto &word : openmetrics_only) {
    om::family_type out = om::family_type::counter;
    EXPECT_TRUE(omd::parse_type(word.first, openmetrics, out)) << word.first;
    EXPECT_EQ(out, word.second) << word.first;
    EXPECT_FALSE(omd::parse_type(word.first, text, out)) << word.first;
  }
  om::family_type out = om::family_type::counter;
  EXPECT_TRUE(omd::parse_type("untyped", text, out));
  EXPECT_EQ(out, om::family_type::unknown);
  EXPECT_FALSE(omd::parse_type("untyped", openmetrics, out));
  for (const om::format f : {openmetrics, text}) {
    for (const char *no : {"", "Counter", "GAUGE", "counters", "count", "histogram ", "x"}) {
      out = om::family_type::info;
      EXPECT_FALSE(omd::parse_type(no, f, out)) << no;
      EXPECT_EQ(out, om::family_type::info) << "a refused word leaves the output alone: " << no;
    }
  }
}

TEST(OpenmetricsParserDetail, EverySuffixAgainstEveryType) {
  // The ownership table, written out again from the two specifications rather
  // than derived from the parser, against every suffix the parser looks up and
  // one it never does.
  const std::vector<std::string> suffixes = {"", "_total", "_created", "_bucket", "_sum", "_count", "_gsum", "_gcount", "_info", "_x", "total"};
  const std::vector<std::pair<om::family_type, std::set<std::string> > > openmetrics_owns = {
      {om::family_type::counter, {"_total", "_created"}},
      {om::family_type::gauge, {""}},
      {om::family_type::histogram, {"_bucket", "_sum", "_count", "_created"}},
      {om::family_type::gaugehistogram, {"_bucket", "_gsum", "_gcount"}},
      {om::family_type::summary, {"", "_sum", "_count", "_created"}},
      {om::family_type::info, {"_info"}},
      {om::family_type::stateset, {""}},
      {om::family_type::unknown, {""}},
  };
  const std::vector<std::pair<om::family_type, std::set<std::string> > > text_owns = {
      {om::family_type::counter, {""}},
      {om::family_type::gauge, {""}},
      {om::family_type::histogram, {"_bucket", "_sum", "_count"}},
      {om::family_type::summary, {"", "_sum", "_count"}},
      {om::family_type::unknown, {""}},
      // Types the text format cannot declare own what an untyped family does.
      {om::family_type::gaugehistogram, {""}},
      {om::family_type::info, {""}},
      {om::family_type::stateset, {""}},
  };
  for (const auto &entry : openmetrics_owns) {
    for (const std::string &suffix : suffixes) {
      EXPECT_EQ(omd::owns_suffix(openmetrics, entry.first, suffix), entry.second.count(suffix) != 0) << om::type_name(entry.first) << " '" << suffix << "'";
    }
  }
  for (const auto &entry : text_owns) {
    for (const std::string &suffix : suffixes) {
      EXPECT_EQ(omd::owns_suffix(text, entry.first, suffix), entry.second.count(suffix) != 0) << om::type_name(entry.first) << " '" << suffix << "'";
    }
  }
}

TEST(OpenmetricsParserDetail, HelpEscapesAreUndoneAndNothingElse) {
  EXPECT_EQ(omd::unescape_help(""), "");
  EXPECT_EQ(omd::unescape_help("plain"), "plain");
  EXPECT_EQ(omd::unescape_help("a\\\\b"), "a\\b");
  EXPECT_EQ(omd::unescape_help("a\\\"b"), "a\"b");
  EXPECT_EQ(omd::unescape_help("a\\nb"), "a\nb");
  // An escaped backslash before `n` is a backslash and an `n`.
  EXPECT_EQ(omd::unescape_help("\\\\n"), "\\n");
  // Any other backslash, and one that ends the text, is kept as served.
  EXPECT_EQ(omd::unescape_help("a\\tb"), "a\\tb");
  EXPECT_EQ(omd::unescape_help("end\\"), "end\\");
  EXPECT_EQ(omd::unescape_help("\\"), "\\");
  EXPECT_EQ(omd::unescape_help("\\\\\\"), "\\\\");
}
