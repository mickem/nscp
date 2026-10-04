// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <locale.h>

#include <algorithm>
#include <array>
#include <clocale>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#include <map>
#include <metrics/openmetrics_parser.hpp>
#include <metrics/openmetrics_parser_detail.hpp>
#include <string_view>
#include <vector>
#if defined(__APPLE__) || defined(__FreeBSD__)
#include <xlocale.h>
#endif

namespace metrics {
namespace openmetrics {

namespace {
const std::size_t npos = static_cast<std::size_t>(-1);
}  // namespace

// The pure helpers the parser is built from, declared in
// `openmetrics_parser_detail.hpp` so that each can be tested on its own -
// including the inputs the parser never hands them, which is what keeps them
// safe if a later change does.
namespace detail {

bool is_blank(const char c) { return c == ' ' || c == '\t'; }
bool is_alpha(const char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
bool is_digit(const char c) { return c >= '0' && c <= '9'; }

// `[a-zA-Z_:][a-zA-Z0-9_:]*`. Colons are reserved for recording rules, but an
// exporter that federates a Prometheus serves them, and both formats allow
// them.
bool is_name_start(const char c) { return is_alpha(c) || c == '_' || c == ':'; }
bool is_name_char(const char c) { return is_name_start(c) || is_digit(c); }

// `[a-zA-Z_][a-zA-Z0-9_]*`: a label name has no colons.
bool is_label_start(const char c) { return is_alpha(c) || c == '_'; }
bool is_label_char(const char c) { return is_label_start(c) || is_digit(c); }

bool ends_with(const std::string_view value, const std::string_view suffix) {
  return value.size() > suffix.size() && value.substr(value.size() - suffix.size()) == suffix;
}

std::string_view trim_blanks(std::string_view text) {
  while (!text.empty() && is_blank(text.front())) text.remove_prefix(1);
  while (!text.empty() && is_blank(text.back())) text.remove_suffix(1);
  return text;
}

// A line as it is read: one trailing carriage return (a CRLF body) dropped and
// the blanks around it trimmed. Every decision `parser::read_line` makes about
// what a line says - whether it may end the body without a line feed, what
// kind of line it is, the parse - goes through this, so no two of them can see
// a different line. The one exception is deliberate: `max_line_bytes` is a
// limit on the line as served, so it reads the raw line, carriage return and
// blanks included.
std::string_view normalise(std::string_view line) {
  if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
  return trim_blanks(line);
}

// `# EOF`, with any blanks the reader would skip anyway.
bool is_eof_marker(const std::string_view line) {
  if (line.empty() || line.front() != '#') return false;
  return trim_blanks(line.substr(1)) == "EOF";
}

// Case-insensitive comparison against a lower-case ASCII word, without
// allocating.
bool equals_word(const std::string_view raw, const char *word) {
  const std::size_t length = std::strlen(word);
  if (raw.size() != length) return false;
  for (std::size_t i = 0; i < length; ++i) {
    char c = raw[i];
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    if (c != word[i]) return false;
  }
  return true;
}

// One line of the body, read left to right. A view rather than a copy: a
// label value can be as long as the whole line, and the line as long as
// `max_line_bytes` allows.
struct cursor {
  std::string_view line;
  std::size_t at = 0;

  bool done() const { return at >= line.size(); }
  // `\0` at the end of the line, which no rule of either grammar accepts, so
  // a check of the next character is safe without a `done()` before it.
  char peek() const { return done() ? '\0' : line[at]; }
  void skip_blanks() {
    while (is_blank(peek())) ++at;
  }
  // Up to the next blank or the end of the line.
  std::string_view token() {
    const std::size_t start = at;
    while (!done() && !is_blank(peek())) ++at;
    return line.substr(start, at - start);
  }
  std::string_view rest() const { return line.substr(at); }
};

// --- numbers ------------------------------------------------------------------

// `[+-]? (digits [. digits?] | . digits) ([eE] [+-]? digits)?`
bool is_decimal(const std::string_view raw) {
  std::size_t i = 0;
  if (i < raw.size() && (raw[i] == '+' || raw[i] == '-')) ++i;
  std::size_t digits = 0;
  while (i < raw.size() && is_digit(raw[i])) {
    ++i;
    ++digits;
  }
  if (i < raw.size() && raw[i] == '.') {
    ++i;
    while (i < raw.size() && is_digit(raw[i])) {
      ++i;
      ++digits;
    }
  }
  if (digits == 0) return false;
  if (i < raw.size() && (raw[i] == 'e' || raw[i] == 'E')) {
    ++i;
    if (i < raw.size() && (raw[i] == '+' || raw[i] == '-')) ++i;
    std::size_t exponent = 0;
    while (i < raw.size() && is_digit(raw[i])) {
      ++i;
      ++exponent;
    }
    if (exponent == 0) return false;
  }
  return i == raw.size();
}

// `[+-]? digits`
bool is_integer(const std::string_view raw) {
  std::size_t i = 0;
  if (i < raw.size() && (raw[i] == '+' || raw[i] == '-')) ++i;
  if (i == raw.size()) return false;
  for (; i < raw.size(); ++i) {
    if (!is_digit(raw[i])) return false;
  }
  return true;
}

// `strtod` against the "C" locale, whatever the process locale is: the token
// was held to the grammar already, and the decimal point is the one character
// in it a locale could change the meaning of. The locale object is made once;
// every C library this builds on has the `_l` variant.
double c_strtod(const char *text, char **end) {
#ifdef _WIN32
  static const _locale_t c_locale = _create_locale(LC_NUMERIC, "C");
  return _strtod_l(text, end, c_locale);
#else
  static const locale_t c_locale = newlocale(LC_NUMERIC_MASK, "C", static_cast<locale_t>(0));
  return strtod_l(text, end, c_locale);
#endif
}

// Converts a token already held to `is_decimal` or `is_integer`.
//
// `strtod` rather than a stream: it is one call on a stack buffer instead of
// three copies per value, and every C library rounds the same way at both ends
// of the range - an underflow becomes the nearest subnormal or zero, an
// overflow `HUGE_VAL`. Underflow is read as that value, on every platform; a
// stream would refuse it on some and read it on others. Overflow is refused:
// the non-finite values have their own spellings, and `1e400` is not one.
bool to_double(const std::string_view raw, double &out) {
  // `strtod` reads an empty token as a zero it consumed all of.
  if (raw.empty()) return false;
  // `at` rather than `[]` for the terminator: a bound off by one here throws
  // in every build, instead of writing past the buffer unseen.
  std::array<char, 64> buffer;
  std::string spill;
  char *text = buffer.data();
  if (raw.size() < buffer.size()) {
    std::memcpy(buffer.data(), raw.data(), raw.size());
    buffer.at(raw.size()) = '\0';
  } else {
    spill.assign(raw);
    text = &spill[0];
  }
  char *end = nullptr;
  const double value = c_strtod(text, &end);
  if (end != text + raw.size() || !std::isfinite(value)) return false;
  out = value;
  return true;
}

// A sample value. Both formats spell the non-finite values in words, and Go's
// `ParseFloat` - which the reference parser and most exporters use - accepts
// them in any case, so this does too: infinity with or without a sign, NaN
// only without one. OpenMetrics allows no signed NaN either.
//
// The one body known to write a signed NaN is NSClient++'s own deprecated
// `openmetrics format = legacy`, which streams a value as `-nan` (glibc) or
// `-nan(ind)` (MSVC). Accepting `-nan` would not make that body readable: it
// pastes its keys in verbatim, dots and spaces included, so it is not an
// exposition Prometheus or this parser can read from its first line on.
bool read_value(const std::string_view raw, double &out) {
  if (equals_word(raw, "nan")) {
    out = std::numeric_limits<double>::quiet_NaN();
    return true;
  }
  if (equals_word(raw, "inf") || equals_word(raw, "+inf") || equals_word(raw, "infinity") || equals_word(raw, "+infinity")) {
    out = std::numeric_limits<double>::infinity();
    return true;
  }
  if (equals_word(raw, "-inf") || equals_word(raw, "-infinity")) {
    out = -std::numeric_limits<double>::infinity();
    return true;
  }
  return is_decimal(raw) && to_double(raw, out);
}

// A timestamp is a point in time, so neither format lets it be non-finite:
// OpenMetrics writes it as decimal seconds, the Prometheus text format as
// integer milliseconds.
bool read_timestamp(const std::string_view raw, const format f, double &out) {
  if (f == format::prometheus_text_0_0_4) return is_integer(raw) && to_double(raw, out);
  return is_decimal(raw) && to_double(raw, out);
}

// --- families -----------------------------------------------------------------

bool parse_type(const std::string_view word, const format f, family_type &out) {
  if (word == "counter") {
    out = family_type::counter;
  } else if (word == "gauge") {
    out = family_type::gauge;
  } else if (word == "histogram") {
    out = family_type::histogram;
  } else if (word == "summary") {
    out = family_type::summary;
  } else if (f == format::prometheus_text_0_0_4) {
    // The older format has five types, and its word for "not known" is not
    // OpenMetrics' word.
    if (word != "untyped") return false;
    out = family_type::unknown;
  } else if (word == "gaugehistogram") {
    out = family_type::gaugehistogram;
  } else if (word == "info") {
    out = family_type::info;
  } else if (word == "stateset") {
    out = family_type::stateset;
  } else if (word == "unknown") {
    out = family_type::unknown;
  } else {
    return false;
  }
  return true;
}

// Which sample names a family of this type owns, by the suffix on the family
// name.
bool owns_suffix(const format f, const family_type type, const std::string_view suffix) {
  if (f == format::prometheus_text_0_0_4) {
    // The metadata lines name the sample, so a counter's sample is the family
    // name itself; only the two multi-sample types spread over suffixes.
    if (type == family_type::histogram) return suffix == "_bucket" || suffix == "_sum" || suffix == "_count";
    if (type == family_type::summary) return suffix.empty() || suffix == "_sum" || suffix == "_count";
    return suffix.empty();
  }
  switch (type) {
    case family_type::counter:
      return suffix == "_total" || suffix == "_created";
    case family_type::histogram:
      return suffix == "_bucket" || suffix == "_sum" || suffix == "_count" || suffix == "_created";
    case family_type::gaugehistogram:
      return suffix == "_bucket" || suffix == "_gsum" || suffix == "_gcount";
    case family_type::summary:
      return suffix.empty() || suffix == "_sum" || suffix == "_count" || suffix == "_created";
    case family_type::info:
      return suffix == "_info";
    case family_type::gauge:
    case family_type::stateset:
    case family_type::unknown:
      break;
  }
  return suffix.empty();
}

const char *const sample_suffixes[] = {"", "_total", "_created", "_bucket", "_sum", "_count", "_gsum", "_gcount", "_info"};

// `# HELP` text. Both formats escape a backslash and a line feed; OpenMetrics
// also escapes a double quote. Any other backslash is kept as served: help is
// prose for a person, and refusing a whole scrape over one would cost far more
// than it protects.
std::string unescape_help(const std::string_view raw) {
  std::string ret;
  ret.reserve(raw.size());
  for (std::size_t i = 0; i < raw.size(); ++i) {
    const char c = raw[i];
    if (c == '\\' && i + 1 < raw.size()) {
      const char next = raw[i + 1];
      if (next == '\\' || next == '"') {
        ret += next;
        ++i;
        continue;
      }
      if (next == 'n') {
        ret += '\n';
        ++i;
        continue;
      }
    }
    ret += c;
  }
  return ret;
}

}  // namespace detail

using namespace detail;

namespace {

// What the metadata lines of a family have said, kept beside the family
// rather than in it because none of it is part of what the caller reads.
struct family_state {
  bool help = false;
  bool type = false;
  bool unit = false;
  // The block a repeated OpenMetrics name opened, until its first sample says
  // whether it is the second family of a same-name pair (see
  // `metadata_line`).
  bool tentative = false;
};

// The families declared or sampled under one name: one, or the two of an
// OpenMetrics same-name pair.
struct name_entry {
  std::size_t first = npos;
  std::size_t second = npos;
};
typedef std::map<std::string, name_entry, std::less<> > name_index;

// The family that could own a sample name, by the name it was declared under
// and the suffix the sample carries; and the family declared under the sample
// name itself, whether or not it owns it.
struct match {
  std::size_t owner = npos;
  std::size_t same_name = npos;
};

enum class line_kind { blank, eof, eof_with_text, comment, metadata, sample };

// What a line is, from its first characters (see `parser::shape_of`).
struct line_shape {
  line_kind what = line_kind::blank;
  // `HELP`, `TYPE` or `UNIT` for metadata, the first word for a comment.
  std::string_view keyword;
  // The metric name the line starts with, after the keyword for metadata;
  // empty when there is none.
  std::string_view name;
  // Where the line goes on after the name.
  std::size_t after_name = 0;
  // Metadata only: the name is followed by a blank or the end of the line.
  bool whole_name = false;
};

// Whether two families of one name are the pair client_golang writes: exactly
// one of them a counter.
bool pair_types(const family_type a, const family_type b) { return (a == family_type::counter) != (b == family_type::counter); }

class parser {
 public:
  parser(result &out, const format f, const limits &bounds) : out_(out), format_(f), bounds_(bounds) {}

  void begin_line(const std::size_t number) { line_number_ = number; }

  // How a parse stops: the reason and the line at fault are set together, so
  // no error can be reported without its line. The line is the one being
  // read, except for a repeated name, whose line is only found to be at fault
  // by a later one (see `refuse_block`).
  bool fail(const std::string &why) { return fail_on(why, line_number_); }

  // Reads one line, without its line feed. Everything about a line is decided
  // here, in one order, from one reading of what the line is (`shape_of`):
  //
  //   1. A line the body ends in the middle of is refused, unread: it cannot
  //      be trusted even for its name. Only an OpenMetrics `# EOF`, or a blank
  //      line after one, may end the body without its line feed.
  //   2. While a repeated name's block is tentative, a line that is not one of
  //      the block's own makes the repeated name the first line at fault, and
  //      that is reported before anything else wrong with this line. Its name
  //      is all that is read of it, so an oversized line costs no more.
  //   3. A line over `max_line_bytes` is refused, unparsed.
  //   4. The line is parsed.
  //
  // Steps 3 and 4 fail a line alike: either way, a metadata line of the family
  // being read, before `# EOF`, takes that family out with it.
  bool read_line(const std::string_view raw, const bool terminated) {
    const std::string_view text = normalise(raw);
    if (!terminated && !may_end_unterminated(text)) return fail("the body ends in the middle of a line");
    const line_shape shape = shape_of(text);
    if (!block_admits(shape)) return refuse_block();
    // A metadata line of the family being read that fails - over-long, or
    // anything `metadata_line` refuses - leaves that family incomplete, and
    // `finish()` takes it out. After `# EOF` the document is complete, and
    // nothing appended to it can take back what it declared. Only a metadata
    // line has a whole name.
    const bool own_metadata = !out_.saw_eof && shape.whole_name && in_own_block(shape.name);
    if (read_shaped(raw, text, shape)) return true;
    if (own_metadata) current_block_failed_ = true;
    return false;
  }

  // Steps 3 and 4 of `read_line`.
  bool read_shaped(const std::string_view raw, const std::string_view text, const line_shape &shape) {
    // The limit is on the line as served, carriage return and blanks included.
    if (bounds_.max_line_bytes != 0 && raw.size() > bounds_.max_line_bytes) {
      return fail("line longer than " + std::to_string(bounds_.max_line_bytes) + " bytes");
    }
    // Blank lines are not part of OpenMetrics, but the Prometheus text format
    // allows them, some exporters separate families with one, and some end the
    // body with one after `# EOF`.
    if (shape.what == line_kind::blank) return true;
    // OpenMetrics ends the document at `# EOF`. Anything after it is either a
    // second document glued on or a proxy appending to the body, and neither
    // belongs in this scrape.
    if (out_.saw_eof) return fail("text after '# EOF'");
    switch (shape.what) {
      case line_kind::eof:
        out_.saw_eof = true;
        return true;
      case line_kind::eof_with_text:
        return fail("unexpected text after '# EOF'");
      case line_kind::metadata:
        return metadata_line(text, shape);
      case line_kind::sample:
        return sample_line(text, shape);
      case line_kind::blank:
      case line_kind::comment:
        break;
    }
    return true;
  }

  // Whether the body may end on this line without its line feed: only an
  // OpenMetrics `# EOF`, or a blank line after one. Anything else cut there
  // may be missing the rest of the line.
  bool may_end_unterminated(const std::string_view text) const {
    if (format_ != format::openmetrics_1_0) return false;
    if (is_eof_marker(text)) return true;
    return out_.saw_eof && text.empty();
  }

  // After the last line read, whether the body ended there or a line failed.
  // A family read only as far as its metadata is kept like any other, except
  // when it is not whole: a line of its own metadata failed, or it is the
  // block of a repeated name that never said its type - which cannot be told
  // apart from late lines for the earlier family, and is what a body cut
  // there looks like.
  void finish() {
    // `read_line` refuses a tentative block at `# EOF`, so none is still open
    // when the body said it was complete.
    if (current_ != npos && state_[current_].tentative && out_.saw_eof && out_.ok()) {
      invariant_broken("a repeated name's block was still open at '# EOF'");
      current_block_failed_ = true;
    }
    if (current_ == npos || current_ + 1 != out_.families.size() || !out_.families[current_].samples.empty()) return;
    family_state &seen = state_[current_];
    if (current_block_failed_ || (seen.tentative && !seen.type)) {
      drop_current();
      return;
    }
    // A repeated name that declared a type its earlier family can pair with,
    // with the body ending before its sample: read as declared.
    seen.tentative = false;
  }

 private:
  bool fail_on(const std::string &why, const std::size_t line) {
    out_.error = why;
    out_.error_line = line;
    return false;
  }

  // An invariant the reading order is meant to keep did not hold. A body off
  // the network must never take the agent down - not even a debug build - nor
  // be read on regardless: the parse stops here, in every build, with an error
  // that names what broke.
  bool invariant_broken(const std::string &what) { return fail("internal error: " + what); }

  static std::string declared_again(const std::string_view name) {
    return "metadata for '" + std::string(name) + "' after that family was already declared or sampled";
  }

  // The block of a repeated name turned out not to be the second family of a
  // pair: its first line was a late line for the earlier family, and that is
  // the line at fault - worded as the text format words it.
  bool refuse_block() {
    current_block_failed_ = true;
    return fail_on(declared_again(out_.families[current_].name), block_opened_on_);
  }

  // Whether a metadata line naming `name` belongs to the family being read: one
  // of that name with no samples yet, whose metadata is still being declared.
  bool in_own_block(const std::string_view name) const {
    return current_ != npos && out_.families[current_].name == name && out_.families[current_].samples.empty();
  }

  // Whether a line can be part of the tentative block of a repeated name: a
  // blank line, a comment, metadata naming the block, or - once the block has
  // declared its type - one of its own samples. A `# EOF` cannot.
  bool block_admits(const line_shape &shape) const {
    if (current_ == npos || !state_[current_].tentative) return true;
    switch (shape.what) {
      case line_kind::blank:
      case line_kind::comment:
        return true;
      case line_kind::eof:
      case line_kind::eof_with_text:
        return false;
      case line_kind::metadata:
        return shape.whole_name && shape.name == out_.families[current_].name;
      case line_kind::sample:
        return state_[current_].type && !shape.name.empty() && owns(current_, shape.name);
    }
    return false;
  }

  // What a line is - its kind, and the keyword and name it starts with - read
  // once, for the block rule and for the parse alike, so that the two can never
  // disagree about a line.
  line_shape shape_of(const std::string_view text) const {
    line_shape shape;
    if (text.empty()) return shape;
    cursor c;
    c.line = text;
    if (text.front() != '#') {
      shape.what = line_kind::sample;
      shape.name = metric_name(c);
      shape.after_name = c.at;
      return shape;
    }
    if (format_ == format::openmetrics_1_0 && is_eof_marker(text)) {
      shape.what = line_kind::eof;
      return shape;
    }
    c.at = 1;
    c.skip_blanks();
    shape.keyword = c.token();
    // In OpenMetrics `# EOF` is the terminator and nothing else; in the
    // Prometheus text format it is a comment like any other.
    if (format_ == format::openmetrics_1_0 && shape.keyword == "EOF") {
      shape.what = line_kind::eof_with_text;
      return shape;
    }
    if (shape.keyword != "HELP" && shape.keyword != "TYPE" && shape.keyword != "UNIT") {
      shape.what = line_kind::comment;
      return shape;
    }
    shape.what = line_kind::metadata;
    if (!is_blank(c.peek())) return shape;
    c.skip_blanks();
    shape.name = metric_name(c);
    shape.after_name = c.at;
    // `foo.bar` is not `foo` with something after it: the line names no
    // family at all.
    shape.whole_name = !shape.name.empty() && (c.done() || is_blank(c.peek()));
    return shape;
  }

  bool metadata_line(const std::string_view text, const line_shape &shape) {
    const std::string_view keyword = shape.keyword;
    const bool help = keyword == "HELP";
    const bool type = keyword == "TYPE";
    const bool unit = keyword == "UNIT";
    const std::string_view name = shape.name;
    if (name.empty()) return fail("expected a metric name after '# " + std::string(keyword) + "'");
    cursor c;
    c.line = text;
    c.at = shape.after_name;
    if (!shape.whole_name) return fail("invalid character in metric name '" + std::string(name) + std::string(c.token()) + "'");
    c.skip_blanks();

    // A line that fails from here on, naming the family being read, fails
    // that family (see `read_line`).
    const bool own_block = in_own_block(name);

    // Read the whole line before touching any family: a line that fails here
    // must create nothing.
    std::string_view payload;
    family_type declared_type = family_type::unknown;
    if (help) {
      payload = c.rest();
    } else {
      payload = c.token();
      c.skip_blanks();
      if (!c.done()) return fail(std::string("unexpected text after the ") + (unit ? "unit" : "type") + " of '" + std::string(name) + "'");
      if (type && !parse_type(payload, format_, declared_type)) {
        return fail("unknown type '" + std::string(payload) + "' for '" + std::string(name) + "'");
      }
    }

    // The family a metadata line describes is the one the previous metadata
    // line opened, while it has no samples yet, or a new one. Both formats keep
    // a family's lines together and its metadata first, so a name already
    // declared or sampled is a second declaration - with one exception.
    std::size_t at = own_block ? current_ : npos;
    bool repeated = false;
    if (at == npos) {
      const match m = find(name);
      if (m.owner != npos && out_.families[m.owner].name != name) {
        return fail("'" + std::string(name) + "' is a sample of the family '" + out_.families[m.owner].name + "'");
      }
      const name_index::const_iterator taken = families_by_name_.find(name);
      if (taken != families_by_name_.end()) {
        // client_golang writes a counter `X_total` in OpenMetrics as the family
        // `X` (it strips the suffix from the metadata lines), so a Go exporter
        // serves the counter `X` beside any gauge, histogram or summary of the
        // same name. Their full names differ, so the registry allows it, and
        // the reference parser takes it. That pair, and only that pair, may
        // share a name: the earlier family must have declared its type, and
        // the later block must declare the other one and carry a sample of its
        // own. Until that sample, the block is read like any other - every
        // line held to the same rules - but tentatively: anything else first
        // makes this line a late line for the earlier family.
        const std::size_t earlier = taken->second.first;
        if (format_ != format::openmetrics_1_0 || taken->second.second != npos || !state_[earlier].type) return fail(declared_again(name));
        if (type && !pair_types(out_.families[earlier].type, declared_type)) return fail(declared_again(name));
        repeated = true;
      }
      if (!room_for_family()) return false;
    } else {
      const family_state &seen = state_[at];
      const bool declared = help ? seen.help : type ? seen.type : seen.unit;
      if (declared) {
        return fail("second '# " + std::string(keyword) + "' line for '" + std::string(name) + "'");
      }
    }
    if (type) {
      // The type of a repeated name's block decides whether it can pair at
      // all, which comes before anything the type would then claim.
      if (at != npos && state_[at].tentative && !pair_types(out_.families[families_by_name_.find(name)->second.first].type, declared_type)) {
        return refuse_block();
      }
      // Samples that arrived before this line under a name the type now claims
      // (`lat_bucket` before `# TYPE lat histogram`) would be split off into a
      // family of their own.
      for (const char *suffix : sample_suffixes) {
        if (*suffix == '\0' || !owns_suffix(format_, declared_type, suffix)) continue;
        scratch_.assign(name.data(), name.size());
        scratch_ += suffix;
        if (families_by_name_.find(scratch_) != families_by_name_.end()) {
          return fail("'" + scratch_ + "' came before the '# TYPE' line of '" + std::string(name) + "'");
        }
      }
    }

    if (at == npos) at = add_family(name, repeated);
    family &target = out_.families[at];
    family_state &seen = state_[at];
    if (help) {
      seen.help = true;
      target.help = unescape_help(payload);
    } else if (unit) {
      seen.unit = true;
      target.unit = std::string(payload);
    } else {
      seen.type = true;
      target.type = declared_type;
    }
    return true;
  }

  // The families that could own `name` as a sample, by every suffix the name
  // could carry.
  match find(const std::string_view name) const {
    match ret;
    for (const char *suffix : sample_suffixes) {
      const std::string_view tail(suffix);
      if (!tail.empty() && !ends_with(name, tail)) continue;
      const name_index::const_iterator it = families_by_name_.find(name.substr(0, name.size() - tail.size()));
      if (it == families_by_name_.end()) continue;
      for (const std::size_t at : {it->second.first, it->second.second}) {
        if (at != npos && owns(at, name)) {
          ret.owner = at;
          return ret;
        }
      }
      if (tail.empty()) ret.same_name = it->second.first;
    }
    return ret;
  }

  bool room_for_family() {
    if (bounds_.max_families != 0 && out_.families.size() >= bounds_.max_families) {
      return fail("more than " + std::to_string(bounds_.max_families) + " families");
    }
    return true;
  }

  void switch_to(const std::size_t at) {
    current_ = at;
    current_block_failed_ = false;
  }

  std::size_t add_family(const std::string_view name, const bool repeated) {
    family added;
    added.name = std::string(name);
    out_.families.push_back(std::move(added));
    family_state seen;
    seen.tentative = repeated;
    if (repeated) block_opened_on_ = line_number_;
    state_.push_back(seen);
    const std::size_t at = out_.families.size() - 1;
    name_entry &entry = families_by_name_[out_.families[at].name];
    (repeated ? entry.second : entry.first) = at;
    switch_to(at);
    return at;
  }

  // Only ever the last family: one without samples, which nothing switches
  // back to.
  void drop_current() {
    const name_index::iterator it = families_by_name_.find(out_.families[current_].name);
    if (it->second.second == current_) {
      it->second.second = npos;
    } else {
      families_by_name_.erase(it);
    }
    out_.families.pop_back();
    state_.pop_back();
    current_ = npos;
  }

  bool owns(const std::size_t at, const std::string_view name) const {
    const family &target = out_.families[at];
    if (name.size() < target.name.size() || name.substr(0, target.name.size()) != target.name) return false;
    return owns_suffix(format_, target.type, name.substr(target.name.size()));
  }

  // The family a sample of this name belongs to: the family being read, or a
  // new one - of unknown type, named after the sample, which is what a sample
  // without any metadata is in both formats. A bare `h` inside the histogram
  // `h` is not one of its samples, and a second family `h` would make the
  // name ambiguous.
  //
  // A sample some earlier family owns arrived after that family gave way to
  // another. OpenMetrics forbids that outright, and it is where a same-name
  // pair would make the sample ambiguous, so it is an error there. The
  // Prometheus text format asks for the same grouping but has no pairs, and
  // its reference parser regroups such a sample into its family, as some
  // exporters rely on; so does this one.
  std::size_t family_for(const std::string_view name) {
    if (current_ != npos && state_[current_].tentative) {
      // The first sample after a repeated name decides: one of the block's own
      // makes it the second family of a pair. `read_line` refuses any other
      // sample before it gets here.
      if (!state_[current_].type || !owns(current_, name)) {
        invariant_broken("'" + std::string(name) + "' reached the block of a repeated name it does not belong to");
        return npos;
      }
      state_[current_].tentative = false;
      return current_;
    }
    if (current_ != npos && owns(current_, name)) return current_;
    const match m = find(name);
    if (m.owner != npos) {
      if (format_ == format::prometheus_text_0_0_4) {
        switch_to(m.owner);
        return m.owner;
      }
      fail("'" + std::string(name) + "' comes after another family, apart from the rest of the family '" + out_.families[m.owner].name + "'");
      return npos;
    }
    if (m.same_name != npos) {
      fail("'" + std::string(name) + "' is not a sample of the " + type_name(out_.families[m.same_name].type) + " family '" + std::string(name) + "'");
      return npos;
    }
    if (!room_for_family()) return npos;
    return add_family(name, false);
  }

  static std::string_view metric_name(cursor &c) {
    const std::size_t start = c.at;
    if (!is_name_start(c.peek())) return std::string_view();
    while (is_name_char(c.peek())) ++c.at;
    return c.line.substr(start, c.at - start);
  }

  bool sample_line(const std::string_view text, const line_shape &shape) {
    cursor c;
    c.line = text;
    c.at = shape.after_name;
    sample parsed;
    const std::string_view name = shape.name;
    if (name.empty()) return fail("expected a metric name at the start of the line");
    parsed.name = std::string(name);
    bool separated = false;
    if (is_blank(c.peek())) {
      c.skip_blanks();
      separated = true;
    }
    if (c.peek() == '{') {
      if (!labels(c, parsed)) return false;
      separated = false;
      if (is_blank(c.peek())) {
        c.skip_blanks();
        separated = true;
      }
    }
    if (c.done()) return fail("missing value for '" + parsed.name + "'");
    if (!separated) return fail("invalid character after '" + parsed.name + "'");

    const std::string_view value = c.token();
    if (!read_value(value, parsed.value)) return fail("invalid value '" + std::string(value) + "' for '" + parsed.name + "'");
    c.skip_blanks();
    if (!c.done() && c.peek() != '#') {
      const std::string_view stamp = c.token();
      double timestamp = 0;
      if (!read_timestamp(stamp, format_, timestamp)) return fail("invalid timestamp '" + std::string(stamp) + "' for '" + parsed.name + "'");
      parsed.timestamp = timestamp;
      c.skip_blanks();
    }
    // An exemplar (`# {trace_id="..."} 1`) is the only thing OpenMetrics allows
    // after the value or timestamp. It describes one traced request rather
    // than the series, so it is not kept. The Prometheus text format has none.
    if (!c.done() && (format_ == format::prometheus_text_0_0_4 || c.peek() != '#')) {
      return fail("unexpected text after the value of '" + parsed.name + "'");
    }

    if (bounds_.max_series != 0 && out_.sample_count >= bounds_.max_series) {
      return fail("more than " + std::to_string(bounds_.max_series) + " series");
    }
    const std::size_t at = family_for(name);
    if (at == npos) return false;
    label_count_ += parsed.labels.size();
    out_.families[at].samples.push_back(std::move(parsed));
    ++out_.sample_count;
    return true;
  }

  // `{name="value",...}`, with an optional trailing comma (the Prometheus text
  // format allows one) and blanks around the separators. Read into a scratch
  // list that keeps its capacity across samples, then moved into a list of
  // exactly the right size: a label list grown by doubling would hold up to
  // twice the memory its labels need, for as long as the result lives.
  bool labels(cursor &c, sample &parsed) {
    names_seen_.clear();
    scratch_labels_.clear();
    ++c.at;
    while (true) {
      c.skip_blanks();
      if (c.done()) return fail("unterminated label set on '" + parsed.name + "'");
      if (c.peek() == '}') {
        ++c.at;
        if (!unique_labels(parsed)) return false;
        parsed.labels.reserve(scratch_labels_.size());
        for (std::pair<std::string, std::string> &label : scratch_labels_) parsed.labels.push_back(std::move(label));
        return true;
      }
      const std::size_t start = c.at;
      if (!is_label_start(c.peek())) return fail("invalid label name on '" + parsed.name + "'");
      while (is_label_char(c.peek())) ++c.at;
      const std::string_view name = c.line.substr(start, c.at - start);
      c.skip_blanks();
      if (c.peek() != '=') return fail("expected '=' after label '" + std::string(name) + "' on '" + parsed.name + "'");
      ++c.at;
      c.skip_blanks();
      if (c.peek() != '"') return fail("expected a quoted value for label '" + std::string(name) + "' on '" + parsed.name + "'");
      ++c.at;
      std::string value;
      if (!label_value(c, value)) return fail("unterminated or badly escaped value for label '" + std::string(name) + "' on '" + parsed.name + "'");
      if (bounds_.max_labels_per_sample != 0 && scratch_labels_.size() >= bounds_.max_labels_per_sample) {
        return fail("more than " + std::to_string(bounds_.max_labels_per_sample) + " labels on '" + parsed.name + "'");
      }
      if (bounds_.max_labels != 0 && label_count_ + scratch_labels_.size() >= bounds_.max_labels) {
        return fail("more than " + std::to_string(bounds_.max_labels) + " labels");
      }
      names_seen_.push_back(name);
      scratch_labels_.emplace_back(std::string(name), std::move(value));
      c.skip_blanks();
      if (c.done()) return fail("unterminated label set on '" + parsed.name + "'");
      if (c.peek() == ',') {
        ++c.at;
        continue;
      }
      if (c.peek() != '}') return fail("expected ',' or '}' after label '" + std::string(name) + "' on '" + parsed.name + "'");
    }
  }

  // A label name twice on one sample, found by sorting the names - views into
  // the line - and comparing neighbours. That costs the sample's own labels
  // and nothing else: no state survives into the next sample, and no hash an
  // exporter could choose names to collide in.
  bool unique_labels(const sample &parsed) {
    if (names_seen_.size() < 2) return true;
    std::sort(names_seen_.begin(), names_seen_.end());
    const std::vector<std::string_view>::const_iterator repeated = std::adjacent_find(names_seen_.begin(), names_seen_.end());
    if (repeated == names_seen_.end()) return true;
    return fail("label '" + std::string(*repeated) + "' twice on '" + parsed.name + "'");
  }

  // The inside of a quoted label value, up to and past its closing quote. A
  // label value is the one place where an exporter may legitimately serve a
  // very long string, so it is copied in runs rather than a byte at a time.
  static bool label_value(cursor &c, std::string &out) {
    while (true) {
      const std::size_t stop = c.line.find_first_of("\\\"", c.at);
      if (stop == std::string_view::npos) return false;
      out.append(c.line.data() + c.at, stop - c.at);
      c.at = stop;
      if (c.peek() == '"') {
        ++c.at;
        return true;
      }
      ++c.at;
      if (c.done()) return false;
      const char escaped = c.peek();
      if (escaped == '\\' || escaped == '"') {
        out += escaped;
      } else if (escaped == 'n') {
        out += '\n';
      } else {
        // Both formats define exactly these three escapes; anything else is a
        // body this parser would have to guess at.
        return false;
      }
      ++c.at;
    }
  }

  result &out_;
  const format format_;
  const limits &bounds_;
  std::size_t line_number_ = 0;
  // The line that repeated the name of the tentative block, if one is open.
  std::size_t block_opened_on_ = 0;
  // Family name -> the families declared or sampled under it. An ordered map
  // rather than a hash table: its worst case does not depend on names an
  // exporter chooses, and it looks a `string_view` up without copying it. It
  // is consulted only when a family starts, never per sample of the family
  // being read.
  name_index families_by_name_;
  std::vector<family_state> state_;
  std::size_t current_ = npos;
  // Whether a metadata line of the family being read failed.
  bool current_block_failed_ = false;
  std::size_t label_count_ = 0;
  // Reused across lines, so that neither costs an allocation per line.
  std::string scratch_;
  std::vector<std::pair<std::string, std::string> > scratch_labels_;
  std::vector<std::string_view> names_seen_;
};

}  // namespace

format format_for_content_type(const std::string &content_type) {
  std::string lowered = content_type;
  for (char &c : lowered) {
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  }
  if (lowered.find("application/openmetrics-text") != std::string::npos) return format::openmetrics_1_0;
  return format::prometheus_text_0_0_4;
}

const char *type_name(const family_type type) {
  switch (type) {
    case family_type::counter:
      return "counter";
    case family_type::gauge:
      return "gauge";
    case family_type::histogram:
      return "histogram";
    case family_type::gaugehistogram:
      return "gaugehistogram";
    case family_type::summary:
      return "summary";
    case family_type::info:
      return "info";
    case family_type::stateset:
      return "stateset";
    case family_type::unknown:
      break;
  }
  return "unknown";
}

namespace {

// Splits the body into lines and hands each to the parser, until the body
// ends or a line fails. Both formats end every line, the last one included,
// with a line feed; whether a line without one may still be read is the
// parser's to decide (see `parser::read_line`).
void read_lines(const std::string_view text, parser &reader) {
  std::size_t start = 0;
  std::size_t number = 0;
  while (start < text.size()) {
    const std::size_t end = text.find('\n', start);
    reader.begin_line(++number);
    const bool terminated = end != std::string_view::npos;
    if (!reader.read_line(text.substr(start, (terminated ? end : text.size()) - start), terminated)) return;
    if (!terminated) return;
    start = end + 1;
  }
}

}  // namespace

result parse(const std::string &body, const format body_format, const limits &bounds) {
  result out;
  parser reader(out, body_format, bounds);
  read_lines(body, reader);
  reader.finish();
  return out;
}

std::optional<std::string> find_label(const sample &s, const std::string &name) {
  for (const std::pair<std::string, std::string> &label : s.labels) {
    if (label.first == name) return label.second;
  }
  return std::nullopt;
}

}  // namespace openmetrics
}  // namespace metrics
