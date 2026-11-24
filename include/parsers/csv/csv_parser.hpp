// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <cstddef>
#include <string>
#include <vector>

// An RFC 4180 CSV parser. Header-only, no dependencies.
//
//  * `delimiter` separates fields; a field that starts with `quote` runs to the
//    matching closing quote and may contain the delimiter, line breaks and the
//    quote itself written twice (`"say ""hi"""` is `say "hi"`).
//  * CRLF, LF and a lone CR all end a record. Empty lines are skipped.
//  * A backslash is an ordinary character, so `C:\data\x.csv` survives as
//    written (boost::escaped_list_separator throws on it).
//  * Text after a closing quote (`"a"b`) is kept, as most readers do.
//  * quote == '\0' turns quoting off: quotes are then ordinary characters.
//
// Input is text, not bytes: decode it first (see utf8::from_utf16).
namespace csv_parser {

struct options {
  char delimiter = ',';
  char quote = '"';
};

struct record {
  std::vector<std::string> fields;
  // The (1-based) line of the text the record starts on. A quoted field with
  // line breaks makes a record span several lines.
  std::size_t line = 0;
};

// Length of the line break starting at text[i]: 2 for CRLF, 1 for a lone CR
// or LF, 0 when text[i] does not start one.
inline std::size_t line_break_at(const std::string &text, const std::size_t i) {
  if (text[i] == '\n') return 1;
  if (text[i] != '\r') return 0;
  return i + 1 < text.size() && text[i + 1] == '\n' ? 2 : 1;
}

// Why the options cannot be used, or "" when they can.
inline std::string validate(const options &opt) {
  if (opt.delimiter == '\0') return "The delimiter must be set";
  if (opt.delimiter == '\r' || opt.delimiter == '\n' || opt.quote == '\r' || opt.quote == '\n') {
    return "Neither the delimiter nor the quote can be a line break";
  }
  if (opt.delimiter == opt.quote) return "The quote and the delimiter must differ";
  return "";
}

namespace detail {

// The parse state. Each step consumes what starts at text[i] and returns how
// many characters it took, so the loop in parse() stays a flat dispatch.
class parser {
 public:
  parser(const std::string &text, const options &opt, std::vector<record> &out) : text_(text), opt_(opt), out_(out) { current_.line = line_; }

  bool run(std::string &error) {
    for (std::size_t i = 0; i < text_.size();) i += in_quotes_ ? step_quoted(i) : step_plain(i);
    if (in_quotes_) {
      error = "Unterminated quoted field starting on line " + std::to_string(quote_line_);
      return false;
    }
    end_record();
    return true;
  }

 private:
  // Inside quotes everything is data except the quote itself.
  std::size_t step_quoted(const std::size_t i) {
    if (text_[i] == opt_.quote) return close_or_escape(i);
    const std::size_t brk = line_break_at(text_, i);
    if (brk > 0) {
      field_.append(text_, i, brk);
      ++line_;
      return brk;
    }
    field_ += text_[i];
    return 1;
  }

  // A quote inside quotes: doubled it is a literal quote, alone it closes.
  std::size_t close_or_escape(const std::size_t i) {
    if (i + 1 < text_.size() && text_[i + 1] == opt_.quote) {
      field_ += opt_.quote;
      return 2;
    }
    in_quotes_ = false;
    return 1;
  }

  std::size_t step_plain(const std::size_t i) {
    const char c = text_[i];
    if (opens_quote(c)) {
      in_quotes_ = has_data_ = true;
      quote_line_ = line_;
      return 1;
    }
    if (c == opt_.delimiter) {
      has_data_ = true;
      end_field();
      return 1;
    }
    const std::size_t brk = line_break_at(text_, i);
    if (brk > 0) {
      end_record();
      current_.line = ++line_;
      return brk;
    }
    field_ += c;
    has_data_ = true;
    return 1;
  }

  // Only a quote at the very start of a field opens a quoted field; anywhere
  // else (`ab"c`, or after a closing quote: `"a"b"`) it is data. An empty
  // field after a closing quote cannot be followed by a quote: `""` followed
  // by `"` is an escaped quote, not a close.
  bool opens_quote(const char c) const { return opt_.quote != '\0' && c == opt_.quote && field_.empty(); }

  void end_field() {
    current_.fields.push_back(field_);
    field_.clear();
  }

  // An empty line has no data and makes no record; ",", "\"\"" and "a" do.
  void end_record() {
    if (has_data_) {
      end_field();
      out_.push_back(current_);
    }
    current_.fields.clear();
    field_.clear();
    has_data_ = false;
  }

  const std::string &text_;
  const options &opt_;
  std::vector<record> &out_;
  record current_;
  std::string field_;
  std::size_t line_ = 1;
  std::size_t quote_line_ = 0;
  bool in_quotes_ = false;
  bool has_data_ = false;  // anything (even an empty quoted field) on this record
};

}  // namespace detail

// Parse `text` into `out` (replacing its contents). Returns false with `error`
// set on unusable options or an unterminated quoted field; `out` then holds
// the records completed before the error.
inline bool parse(const std::string &text, const options &opt, std::vector<record> &out, std::string &error) {
  out.clear();
  error = validate(opt);
  if (!error.empty()) return false;
  return detail::parser(text, opt, out).run(error);
}

}  // namespace csv_parser
