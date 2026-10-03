// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <locale.h>

#include <algorithm>
#include <clocale>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#include <map>
#include <metrics/openmetrics_parser.hpp>
#include <string_view>
#include <vector>
#if defined(__APPLE__) || defined(__FreeBSD__)
#include <xlocale.h>
#endif

namespace metrics {
namespace openmetrics {

namespace {

const std::size_t npos = static_cast<std::size_t>(-1);

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

// A line as every check sees it: one trailing carriage return (a CRLF body)
// dropped and the blanks around it trimmed. The reader and the truncation
// check in `parse()` both go through this, so they cannot disagree about what a
// line says.
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
  char peek() const { return line[at]; }
  void skip_blanks() {
    while (!done() && is_blank(peek())) ++at;
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
  char buffer[64];
  std::string spill;
  char *text = buffer;
  if (raw.size() < sizeof(buffer)) {
    std::memcpy(buffer, raw.data(), raw.size());
    buffer[raw.size()] = '\0';
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
// them in any case, so this does too.
bool read_value(const std::string_view raw, double &out) {
  if (equals_word(raw, "nan") || equals_word(raw, "+nan") || equals_word(raw, "-nan")) {
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

// What the metadata lines of a family have said, kept beside the family
// rather than in it because none of it is part of what the caller reads.
struct family_state {
  bool help = false;
  bool type = false;
  bool unit = false;
  // The other family of an OpenMetrics same-name pair (see `comment`), if this
  // is one of them.
  std::size_t partner = npos;
  // Whether the family took its `_created` sample: a counter and a histogram
  // or summary of one name both own `X_created`, and only one of them may
  // have it.
  bool took_created = false;
};

// The families declared or sampled under one name: one, or the two of an
// OpenMetrics same-name pair.
struct name_entry {
  std::size_t first = npos;
  std::size_t second = npos;
};
typedef std::map<std::string, name_entry, std::less<> > name_index;

// What reading ahead through the block a repeated OpenMetrics name opens
// found (see `pair_ahead`).
enum class verdict { pair, not_a_pair, undecided };

// The family that could own a sample name, by the name it was declared under
// and the suffix the sample carries; and the family declared under the sample
// name itself, whether or not it owns it.
struct match {
  std::size_t owner = npos;
  std::size_t same_name = npos;
};

class parser {
 public:
  parser(result &out, const std::string_view body, const format f, const limits &bounds) : out_(out), body_(body), format_(f), bounds_(bounds) {}

  // `next` is where the line after this one starts, for reading ahead.
  void begin_line(const std::size_t number, const std::size_t next) {
    line_number_ = number;
    next_line_ = next;
  }

  // Whether the body was found to end inside a block that cannot be read
  // without its rest (see `comment`): what follows is not read, and the result
  // is what a body truncated there gives.
  bool stopped() const { return stopped_; }

  // The one way a parse stops: the reason and the line it stopped on are set
  // together, so no error can be reported without its line, or with another.
  bool fail(const std::string &why) {
    out_.error = why;
    out_.error_line = line_number_;
    return false;
  }

  // Reads one line, without its line feed.
  bool line(const std::string_view raw) {
    // Blank lines are not part of OpenMetrics, but the Prometheus text format
    // allows them, some exporters separate families with one, and some end the
    // body with one after `# EOF`.
    const std::string_view text = normalise(raw);
    if (text.empty()) return true;
    // OpenMetrics ends the document at `# EOF`. Anything after it is either a
    // second document glued on or a proxy appending to the body, and neither
    // belongs in this scrape.
    if (out_.saw_eof) return fail("text after '# EOF'");
    if (text.front() == '#') return comment(text);
    return sample_line(text);
  }

  // After the last line read, whether the body ended there or a line failed.
  // The family being read is taken out of the result when it has no samples
  // and is not whole: a line of its own metadata failed, or it is the second
  // of a same-name pair, which exists only for the samples it was read ahead
  // to have. Everything else read is kept.
  void finish() {
    if (current_ == npos || !out_.families[current_].samples.empty()) return;
    const family_state &seen = state_[current_];
    if (current_block_failed_ || (seen.partner != npos && seen.partner < current_)) drop_current();
  }

 private:
  static std::string declared_again(const std::string_view name) {
    return "metadata for '" + std::string(name) + "' after that family was already declared or sampled";
  }

  bool comment(const std::string_view text) {
    if (format_ == format::openmetrics_1_0 && is_eof_marker(text)) {
      out_.saw_eof = true;
      return true;
    }
    cursor c;
    c.line = text;
    c.at = 1;
    c.skip_blanks();
    const std::string_view keyword = c.token();
    // In OpenMetrics `# EOF` is the terminator and nothing else; in the
    // Prometheus text format it is a comment like any other.
    if (format_ == format::openmetrics_1_0 && keyword == "EOF") return fail("unexpected text after '# EOF'");
    const bool help = keyword == "HELP";
    const bool type = keyword == "TYPE";
    const bool unit = keyword == "UNIT";
    // Anything else is a comment.
    if (!help && !type && !unit) return true;
    if (c.done() || !is_blank(c.peek())) return fail("expected a metric name after '# " + std::string(keyword) + "'");
    c.skip_blanks();
    const std::string_view name = metric_name(c);
    if (name.empty()) return fail("expected a metric name after '# " + std::string(keyword) + "'");
    // `foo.bar` is not `foo` with something after it: the line names no
    // family at all, so it says nothing about the one being read.
    if (!c.done() && !is_blank(c.peek())) return fail("invalid character in metric name '" + std::string(name) + std::string(c.token()) + "'");
    c.skip_blanks();

    // A line that fails from here on belongs to the family it names. When that
    // is the family being read and it has no samples yet, the failure leaves
    // it incomplete, and `finish()` takes it out.
    const bool own_block = current_ != npos && out_.families[current_].name == name && out_.families[current_].samples.empty();
    const auto fail_block = [&](const std::string &why) {
      if (own_block) current_block_failed_ = true;
      return fail(why);
    };

    // Read the whole line before touching any family: a line that fails here
    // must create nothing.
    std::string_view payload;
    family_type declared_type = family_type::unknown;
    if (help) {
      payload = c.rest();
    } else {
      payload = c.token();
      c.skip_blanks();
      if (!c.done()) return fail_block(std::string("unexpected text after the ") + (unit ? "unit" : "type") + " of '" + std::string(name) + "'");
      if (type && !parse_type(payload, format_, declared_type)) {
        return fail_block("unknown type '" + std::string(payload) + "' for '" + std::string(name) + "'");
      }
    }

    // The family a metadata line describes is the one the previous metadata
    // line opened, while it has no samples yet, or a new one. Both formats keep
    // a family's lines together and its metadata first, so a name already
    // declared or sampled is a second declaration - with one exception.
    std::size_t at = own_block ? current_ : npos;
    std::size_t partner = npos;
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
        // the later block must declare the other one and carry a sample.
        // Whether it does is in the lines ahead, so they are read before this
        // one is taken: a block that is not such a pair is a second
        // declaration, reported here, on the line that makes it.
        const std::size_t earlier = taken->second.first;
        if (format_ != format::openmetrics_1_0 || taken->second.second != npos || !state_[earlier].type) return fail(declared_again(name));
        switch (pair_ahead(name, out_.families[earlier].type, type, declared_type)) {
          case verdict::not_a_pair:
            return fail(declared_again(name));
          case verdict::undecided:
            // The body ends inside the block, which is what a cut there looks
            // like: read as far as the block, no further.
            stopped_ = true;
            return true;
          case verdict::pair:
            break;
        }
        partner = earlier;
      }
      if (!room_for_family()) return false;
    } else {
      const family_state &seen = state_[at];
      if ((help && seen.help) || (type && seen.type) || (unit && seen.unit)) {
        return fail_block("second '# " + std::string(keyword) + "' line for '" + std::string(name) + "'");
      }
    }
    if (type) {
      // Samples that arrived before this line under a name the type now claims
      // (`lat_bucket` before `# TYPE lat histogram`) would be split off into a
      // family of their own.
      for (const char *suffix : sample_suffixes) {
        if (*suffix == '\0' || !owns_suffix(format_, declared_type, suffix)) continue;
        scratch_.assign(name.data(), name.size());
        scratch_ += suffix;
        if (families_by_name_.find(scratch_) != families_by_name_.end()) {
          return fail_block("'" + scratch_ + "' came before the '# TYPE' line of '" + std::string(name) + "'");
        }
      }
    }

    if (at == npos) at = add_family(name, partner);
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

  // Reads ahead through the block a repeated OpenMetrics name opens, without
  // taking anything from it, to the line that decides whether it is the second
  // of a same-name pair: its `# HELP`, `# UNIT` and `# TYPE` lines must name it,
  // its type must make exactly one of the two a counter, and its first sample
  // must be one of the new family's own. Anything else - another family, a
  // second `# TYPE`, `# EOF` - decides it is not. A body that ends first is
  // undecided. The line being read opens the block, and is a `# TYPE` line
  // when `typed`. Each block is read ahead once, so the whole parse stays one
  // pass over the body.
  verdict pair_ahead(const std::string_view name, const family_type earlier, bool typed, family_type declared) const {
    if (typed && (earlier == family_type::counter) == (declared == family_type::counter)) return verdict::not_a_pair;
    std::size_t start = next_line_;
    while (start < body_.size()) {
      const std::size_t end = body_.find('\n', start);
      const bool terminated = end != std::string_view::npos;
      const std::string_view text = normalise(body_.substr(start, (terminated ? end : body_.size()) - start));
      start = terminated ? end + 1 : body_.size();
      if (text.empty()) continue;
      // An unterminated last line is a cut, unless it is the terminator.
      if (!terminated && !is_eof_marker(text)) return verdict::undecided;
      cursor c;
      c.line = text;
      if (text.front() == '#') {
        if (is_eof_marker(text)) return verdict::not_a_pair;
        c.at = 1;
        c.skip_blanks();
        const std::string_view keyword = c.token();
        if (keyword != "HELP" && keyword != "TYPE" && keyword != "UNIT") continue;
        c.skip_blanks();
        if (metric_name(c) != name) return verdict::not_a_pair;
        if (keyword != "TYPE") continue;
        c.skip_blanks();
        if (typed || !parse_type(c.token(), format_, declared)) return verdict::not_a_pair;
        typed = true;
        if ((earlier == family_type::counter) == (declared == family_type::counter)) return verdict::not_a_pair;
        continue;
      }
      const std::string_view sampled = metric_name(c);
      if (!typed || sampled.size() < name.size() || sampled.substr(0, name.size()) != name) return verdict::not_a_pair;
      return owns_suffix(format_, declared, sampled.substr(name.size())) ? verdict::pair : verdict::not_a_pair;
    }
    return verdict::undecided;
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

  std::size_t add_family(const std::string_view name, const std::size_t partner) {
    family added;
    added.name = std::string(name);
    out_.families.push_back(std::move(added));
    family_state seen;
    seen.partner = partner;
    state_.push_back(seen);
    const std::size_t at = out_.families.size() - 1;
    name_entry &entry = families_by_name_[out_.families[at].name];
    if (partner == npos) {
      entry.first = at;
    } else {
      entry.second = at;
      state_[partner].partner = at;
    }
    current_ = at;
    current_block_failed_ = false;
    return at;
  }

  // The family being read is always the last one: families are contiguous.
  void drop_current() {
    const name_index::iterator it = families_by_name_.find(out_.families[current_].name);
    if (it->second.second == current_) {
      it->second.second = npos;
      state_[it->second.first].partner = npos;
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

  // The family a sample of this name belongs to. Both formats keep a family's
  // samples together, so it is the family being read, or a new one - of
  // unknown type, named after the sample, which is what a sample without any
  // metadata is in both. A sample some earlier family owns arrived after that
  // family closed; a bare `h` inside the histogram `h` is not one of its
  // samples. Either way the name would no longer say which family is meant.
  std::size_t family_for(const std::string_view name) {
    if (current_ != npos && owns(current_, name)) return claim_created(current_, name) ? current_ : npos;
    const match m = find(name);
    if (m.owner != npos) {
      fail("'" + std::string(name) + "' comes after another family, apart from the rest of the family '" + out_.families[m.owner].name + "'");
      return npos;
    }
    if (m.same_name != npos) {
      fail("'" + std::string(name) + "' is not a sample of the " + type_name(out_.families[m.same_name].type) + " family '" + std::string(name) + "'");
      return npos;
    }
    if (!room_for_family()) return npos;
    return add_family(name, npos);
  }

  // The one sample name both families of a same-name pair own is `X_created`
  // (a counter, and a histogram or summary): whichever takes it first has it.
  // Recorded for every family, since the first of a pair takes its samples
  // before it knows it has a partner.
  bool claim_created(const std::size_t at, const std::string_view name) {
    family_state &seen = state_[at];
    if (name.size() != out_.families[at].name.size() + 8 || !ends_with(name, "_created")) return true;
    if (seen.partner != npos && state_[seen.partner].took_created) {
      return fail("'" + std::string(name) + "' is already a sample of the " + type_name(out_.families[seen.partner].type) + " family '" +
                  out_.families[at].name + "'");
    }
    seen.took_created = true;
    return true;
  }

  static std::string_view metric_name(cursor &c) {
    const std::size_t start = c.at;
    if (c.done() || !is_name_start(c.peek())) return std::string_view();
    while (!c.done() && is_name_char(c.peek())) ++c.at;
    return c.line.substr(start, c.at - start);
  }

  bool sample_line(const std::string_view text) {
    cursor c;
    c.line = text;
    sample parsed;
    const std::string_view name = metric_name(c);
    if (name.empty()) return fail("expected a metric name at the start of the line");
    parsed.name = std::string(name);
    bool separated = false;
    if (!c.done() && is_blank(c.peek())) {
      c.skip_blanks();
      separated = true;
    }
    if (!c.done() && c.peek() == '{') {
      if (!labels(c, parsed)) return false;
      separated = false;
      if (!c.done() && is_blank(c.peek())) {
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
      while (!c.done() && is_label_char(c.peek())) ++c.at;
      const std::string_view name = c.line.substr(start, c.at - start);
      c.skip_blanks();
      if (c.done() || c.peek() != '=') return fail("expected '=' after label '" + std::string(name) + "' on '" + parsed.name + "'");
      ++c.at;
      c.skip_blanks();
      if (c.done() || c.peek() != '"') return fail("expected a quoted value for label '" + std::string(name) + "' on '" + parsed.name + "'");
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
  // The whole body, for reading ahead.
  const std::string_view body_;
  const format format_;
  const limits &bounds_;
  std::size_t line_number_ = 0;
  std::size_t next_line_ = 0;
  bool stopped_ = false;
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

// Reads lines until the body ends or one fails.
void read_lines(const std::string_view text, const format body_format, const limits &bounds, parser &reader, result &out) {
  std::size_t start = 0;
  std::size_t number = 0;
  while (start < text.size()) {
    const std::size_t end = text.find('\n', start);
    reader.begin_line(++number, end == std::string_view::npos ? text.size() : end + 1);
    const bool terminated = end != std::string_view::npos;
    const std::size_t length = (terminated ? end : text.size()) - start;
    if (bounds.max_line_bytes != 0 && length > bounds.max_line_bytes) {
      reader.fail("line longer than " + std::to_string(bounds.max_line_bytes) + " bytes");
      return;
    }
    const std::string_view line = text.substr(start, length);
    // Both formats end every line, the last one included, with a line feed. A
    // body that stops without one was cut off - by a size cap, a timeout or a
    // dying exporter - and its last line cannot be trusted even when it reads:
    // `foo 12` cut to `foo 1` is a perfectly good sample. The one exception is
    // an OpenMetrics terminator, or a blank line after it, since plenty of
    // exporters leave its line feed off. A Prometheus text body has no
    // terminator, so there a `# EOF` is a comment that may have been cut from
    // a longer one, and the body is refused like any other.
    if (!terminated) {
      const std::string_view last = normalise(line);
      const bool terminator = body_format == format::openmetrics_1_0 && (is_eof_marker(last) || (out.saw_eof && last.empty()));
      if (!terminator) {
        reader.fail("the body ends in the middle of a line");
        return;
      }
    }
    if (!reader.line(line) || reader.stopped()) return;
    if (!terminated) return;
    start = end + 1;
  }
}

}  // namespace

result parse(const std::string &body, const format body_format, const limits &bounds) {
  result out;
  parser reader(out, body, body_format, bounds);
  read_lines(body, body_format, bounds, reader, out);
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
