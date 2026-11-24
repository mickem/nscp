// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <parsers/csv/csv_parser.hpp>
#include <string>
#include <vector>

using csv_parser::options;
using csv_parser::record;

namespace {

options with(const char delimiter, const char quote) {
  options opt;
  opt.delimiter = delimiter;
  opt.quote = quote;
  return opt;
}

// Parse with the default options and expect success.
std::vector<record> ok(const std::string &text, const options &opt = options()) {
  std::vector<record> rows;
  std::string error;
  EXPECT_TRUE(csv_parser::parse(text, opt, rows, error)) << error;
  EXPECT_EQ("", error);
  return rows;
}

std::vector<std::string> fields(const std::vector<std::string> &f) { return f; }

}  // namespace

// --- line_break_at -----------------------------------------------------------

TEST(csv_parser, line_break_at) {
  EXPECT_EQ(1u, csv_parser::line_break_at("a\nb", 1));
  EXPECT_EQ(2u, csv_parser::line_break_at("a\r\nb", 1));
  EXPECT_EQ(1u, csv_parser::line_break_at("a\rb", 1));
  EXPECT_EQ(1u, csv_parser::line_break_at("a\r", 1));  // CR as the last character
  EXPECT_EQ(0u, csv_parser::line_break_at("ab", 1));
}

// --- validate ----------------------------------------------------------------

TEST(csv_parser, validate_accepts_the_defaults_and_disabled_quoting) {
  EXPECT_EQ("", csv_parser::validate(options()));
  EXPECT_EQ("", csv_parser::validate(with(';', '\0')));
  EXPECT_EQ("", csv_parser::validate(with('\t', '\'')));
}

TEST(csv_parser, validate_refuses_unusable_options) {
  EXPECT_EQ("The delimiter must be set", csv_parser::validate(with('\0', '"')));
  EXPECT_EQ("Neither the delimiter nor the quote can be a line break", csv_parser::validate(with('\r', '"')));
  EXPECT_EQ("Neither the delimiter nor the quote can be a line break", csv_parser::validate(with('\n', '"')));
  EXPECT_EQ("Neither the delimiter nor the quote can be a line break", csv_parser::validate(with(',', '\r')));
  EXPECT_EQ("Neither the delimiter nor the quote can be a line break", csv_parser::validate(with(',', '\n')));
  EXPECT_EQ("The quote and the delimiter must differ", csv_parser::validate(with(',', ',')));
}

TEST(csv_parser, parse_refuses_unusable_options_and_clears_the_output) {
  std::vector<record> rows(3);
  std::string error;
  EXPECT_FALSE(csv_parser::parse("a,b", with(',', ','), rows, error));
  EXPECT_EQ("The quote and the delimiter must differ", error);
  EXPECT_TRUE(rows.empty());
}

// --- plain fields ------------------------------------------------------------

TEST(csv_parser, empty_text_has_no_records) { EXPECT_TRUE(ok("").empty()); }

TEST(csv_parser, plain_fields_and_line_numbers) {
  const std::vector<record> rows = ok("a,b,c\nd,e,f\n");
  ASSERT_EQ(2u, rows.size());
  EXPECT_EQ(fields({"a", "b", "c"}), rows[0].fields);
  EXPECT_EQ(1u, rows[0].line);
  EXPECT_EQ(fields({"d", "e", "f"}), rows[1].fields);
  EXPECT_EQ(2u, rows[1].line);
}

TEST(csv_parser, every_line_ending) {
  const std::vector<record> rows = ok("a\r\nb\rc\nd");
  ASSERT_EQ(4u, rows.size());
  EXPECT_EQ("a", rows[0].fields[0]);
  EXPECT_EQ("b", rows[1].fields[0]);
  EXPECT_EQ("c", rows[2].fields[0]);
  EXPECT_EQ("d", rows[3].fields[0]);  // no line break at the end
  EXPECT_EQ(4u, rows[3].line);
}

TEST(csv_parser, empty_lines_are_skipped_but_counted) {
  const std::vector<record> rows = ok("\n\r\n\ra\n\nb");
  ASSERT_EQ(2u, rows.size());
  EXPECT_EQ(4u, rows[0].line);
  EXPECT_EQ(6u, rows[1].line);
}

TEST(csv_parser, empty_fields_are_kept) {
  const std::vector<record> rows = ok("a,\n,\n,,b\n");
  ASSERT_EQ(3u, rows.size());
  EXPECT_EQ(fields({"a", ""}), rows[0].fields);
  EXPECT_EQ(fields({"", ""}), rows[1].fields);
  EXPECT_EQ(fields({"", "", "b"}), rows[2].fields);
}

TEST(csv_parser, backslash_is_an_ordinary_character) {
  const std::vector<record> rows = ok("srv1,C:\\data\\x.csv,\"\\\\server\\share\"");
  ASSERT_EQ(1u, rows.size());
  EXPECT_EQ(fields({"srv1", "C:\\data\\x.csv", "\\\\server\\share"}), rows[0].fields);
}

TEST(csv_parser, other_delimiter) {
  const std::vector<record> rows = ok("a\tb;c\n", with('\t', '"'));
  ASSERT_EQ(1u, rows.size());
  EXPECT_EQ(fields({"a", "b;c"}), rows[0].fields);
}

// --- quoted fields -----------------------------------------------------------

TEST(csv_parser, quoted_field_may_hold_the_delimiter) {
  const std::vector<record> rows = ok("a,\"b,c\",d");
  ASSERT_EQ(1u, rows.size());
  EXPECT_EQ(fields({"a", "b,c", "d"}), rows[0].fields);
}

TEST(csv_parser, doubled_quote_is_a_literal_quote) {
  const std::vector<record> rows = ok("\"say \"\"hi\"\"\",\"\"\"\"");
  ASSERT_EQ(1u, rows.size());
  EXPECT_EQ(fields({"say \"hi\"", "\""}), rows[0].fields);
}

TEST(csv_parser, empty_quoted_field_makes_a_record) {
  const std::vector<record> rows = ok("\"\"\n");
  ASSERT_EQ(1u, rows.size());
  EXPECT_EQ(fields({""}), rows[0].fields);
}

TEST(csv_parser, quoted_field_closing_at_the_end_of_the_text) {
  const std::vector<record> rows = ok("a,\"b\"");
  ASSERT_EQ(1u, rows.size());
  EXPECT_EQ(fields({"a", "b"}), rows[0].fields);
}

TEST(csv_parser, line_breaks_inside_quotes_stay_in_the_field) {
  const std::vector<record> rows = ok("a,\"1\n2\r\n3\r4\"\nb\n");
  ASSERT_EQ(2u, rows.size());
  EXPECT_EQ("1\n2\r\n3\r4", rows[0].fields[1]);
  EXPECT_EQ(1u, rows[0].line);
  EXPECT_EQ(5u, rows[1].line);  // the quoted field spanned lines 1-4
}

TEST(csv_parser, quote_inside_an_unquoted_field_is_data) {
  const std::vector<record> rows = ok("ab\"c,d");
  ASSERT_EQ(1u, rows.size());
  EXPECT_EQ(fields({"ab\"c", "d"}), rows[0].fields);
}

TEST(csv_parser, text_after_a_closing_quote_is_kept) {
  const std::vector<record> rows = ok("\"a\"b\"c,d");
  ASSERT_EQ(1u, rows.size());
  EXPECT_EQ(fields({"ab\"c", "d"}), rows[0].fields);
}

TEST(csv_parser, quoting_can_be_turned_off) {
  // With no quote character a '"' is data, and so is a NUL.
  const std::vector<record> rows = ok(std::string("\"a\",b\0c\n", 8), with(',', '\0'));
  ASSERT_EQ(1u, rows.size());
  EXPECT_EQ(fields({"\"a\"", std::string("b\0c", 3)}), rows[0].fields);
}

TEST(csv_parser, other_quote_character) {
  const std::vector<record> rows = ok("'a,b','it''s',\"c\"", with(',', '\''));
  ASSERT_EQ(1u, rows.size());
  EXPECT_EQ(fields({"a,b", "it's", "\"c\""}), rows[0].fields);
}

// --- errors ------------------------------------------------------------------

TEST(csv_parser, unterminated_quote_names_the_line_it_opened_on) {
  std::vector<record> rows;
  std::string error;
  EXPECT_FALSE(csv_parser::parse("a,b\nc,\"open\nd,e\n", options(), rows, error));
  EXPECT_EQ("Unterminated quoted field starting on line 2", error);
  // The records completed before the error are kept.
  ASSERT_EQ(1u, rows.size());
  EXPECT_EQ(fields({"a", "b"}), rows[0].fields);
}

TEST(csv_parser, a_lone_quote_is_unterminated) {
  std::vector<record> rows;
  std::string error;
  EXPECT_FALSE(csv_parser::parse("\"", options(), rows, error));
  EXPECT_EQ("Unterminated quoted field starting on line 1", error);
}

TEST(csv_parser, parse_replaces_the_output) {
  std::vector<record> rows(5);
  std::string error;
  ASSERT_TRUE(csv_parser::parse("x", options(), rows, error));
  ASSERT_EQ(1u, rows.size());
  EXPECT_EQ("x", rows[0].fields[0]);
}
