// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <metrics/openmetrics_parser.hpp>
#include <string>
#include <string_view>

// The pure helpers `openmetrics_parser.cpp` is built from. Not part of the
// parser's interface: they are declared here so that a test can drive each one
// directly - with the inputs the parser never hands it (an empty token, a
// token the grammar check let through) as well as the ones it does - and so
// that every path through them is exercised, not only those a whole body can
// reach.
namespace metrics {
namespace openmetrics {
namespace detail {

// The character classes of the grammar.
bool is_blank(char c);
bool is_alpha(char c);
bool is_digit(char c);
bool is_name_start(char c);
bool is_name_char(char c);
bool is_label_start(char c);
bool is_label_char(char c);
// The byte at `i`, or `\0` past the end.
char char_at(std::string_view text, std::size_t i);
// ASCII-only case folding.
char ascii_lower(char c);

// Whether `value` is longer than `suffix` and ends with it.
bool ends_with(std::string_view value, std::string_view suffix);
// `text` without the blanks around it.
std::string_view trim_blanks(std::string_view text);
// A line without one trailing carriage return and the blanks around it.
std::string_view normalise(std::string_view line);
// `# EOF`, with any blanks the reader would skip.
bool is_eof_marker(std::string_view line);
// Case-insensitive comparison against a lower-case ASCII word.
bool equals_word(std::string_view raw, const char *word);

// `[+-]? (digits [. digits?] | . digits) ([eE] [+-]? digits)?`
bool is_decimal(std::string_view raw);
// `[+-]? digits`
bool is_integer(std::string_view raw);
// `strtod` in the C locale, whatever the process locale is.
double c_strtod(const char *text, char **end);
// The number `raw` spells, through a C-locale `strtod`; false when it does not
// spell all of it, or overflows.
bool to_double(std::string_view raw, double &out);
// A sample value, the non-finite spellings included.
bool read_value(std::string_view raw, double &out);
// A timestamp in the format's unit: decimal seconds, or integer milliseconds.
bool read_timestamp(std::string_view raw, format f, double &out);

// The type a `# TYPE` line names, in the words of the format.
bool parse_type(std::string_view word, format f, family_type &out);
// Whether a family of `type` owns the sample `<family name><suffix>`.
bool owns_suffix(format f, family_type type, std::string_view suffix);
// `# HELP` text with its escapes undone.
std::string unescape_help(std::string_view raw);

}  // namespace detail
}  // namespace openmetrics
}  // namespace metrics
