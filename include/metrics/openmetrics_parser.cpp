// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <clocale>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <locale>
#include <metrics/openmetrics_parser.hpp>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

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
  // `strtod` reads the C locale's decimal point. The token is held to the
  // grammar already, so the decimal point is the only character the locale can
  // change the meaning of.
  const char *point = std::localeconv()->decimal_point;
  if (point == nullptr || std::strlen(point) != 1) {
    // A multi-byte decimal point cannot be substituted in place; read it the
    // slow way, which never consults the global locale.
    std::istringstream stream{std::string(raw)};
    stream.imbue(std::locale::classic());
    double value = 0;
    stream >> value;
    if (stream.fail() || !std::isfinite(value)) return false;
    out = value;
    return true;
  }
  if (*point != '.') {
    for (std::size_t i = 0; i < raw.size(); ++i) {
      if (text[i] == '.') text[i] = *point;
    }
  }
  char *end = nullptr;
  const double value = std::strtod(text, &end);
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

// What the metadata lines of a family have said so far, kept beside the family
// rather than in it because none of it is part of what the caller reads.
struct declared {
  bool help = false;
  bool type = false;
  bool unit = false;
};

// Up to this many labels on one sample, a repeated name is looked for by
// scanning the ones already read; past it, through a hash set, so a line of
// tens of thousands of labels costs the same per label as a line of three.
const std::size_t linear_label_scan = 8;

class parser {
 public:
  parser(result &out, const format f, const limits &bounds) : out_(out), format_(f), bounds_(bounds) {}

  void begin_line(const std::size_t number) { line_number_ = number; }

  // The one way a parse stops: the reason and the line it stopped on are set
  // together, so no error can be reported without its line.
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

 private:
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
    const std::string word(keyword);
    if (c.done() || !is_blank(c.peek())) return fail("expected a metric name after '# " + word + "'");
    c.skip_blanks();
    const std::string name = metric_name(c);
    if (name.empty()) return fail("expected a metric name after '# " + word + "'");
    if (!c.done() && !is_blank(c.peek())) return fail("invalid character in metric name '" + name + "'");
    c.skip_blanks();

    // Read the whole line before touching any family: a line that fails here
    // must leave nothing behind.
    std::string help_text;
    std::string unit_text;
    family_type declared_type = family_type::unknown;
    if (help) {
      help_text = unescape_help(c.rest());
    } else if (unit) {
      unit_text = std::string(c.token());
      c.skip_blanks();
      if (!c.done()) return fail("unexpected text after the unit of '" + name + "'");
    } else {
      const std::string_view type_word = c.token();
      c.skip_blanks();
      if (!c.done()) return fail("unexpected text after the type of '" + name + "'");
      if (!parse_type(type_word, format_, declared_type)) return fail("unknown type '" + std::string(type_word) + "' for '" + name + "'");
    }

    // The family a metadata line describes is the one the previous metadata
    // line opened, while it has no samples yet, or a new one. Both formats keep
    // a family's lines together and put its metadata first, so a name already
    // seen anywhere else is either a second declaration or a family split in
    // two, and the body is not one this parser can describe.
    std::size_t at = npos;
    if (current_ != npos && out_.families[current_].name == name && out_.families[current_].samples.empty()) {
      at = current_;
    } else {
      if (index_.find(name) != index_.end()) return fail("metadata for '" + name + "' after that family was already declared or sampled");
      const std::size_t owner = owner_of(name);
      if (owner != npos) return fail("'" + name + "' is a sample of the family '" + out_.families[owner].name + "'");
      if (!room_for_family()) return false;
    }
    if (at != npos) {
      const declared &seen = declared_[at];
      if ((help && seen.help) || (type && seen.type) || (unit && seen.unit)) return fail("second '# " + word + "' line for '" + name + "'");
    }
    if (type) {
      // Samples that arrived before this line under a name the type now claims
      // (`lat_bucket` before `# TYPE lat histogram`) would be split off into a
      // family of their own.
      for (const char *suffix : sample_suffixes) {
        if (*suffix == '\0' || !owns_suffix(format_, declared_type, suffix)) continue;
        const std::string sampled = name + suffix;
        if (index_.find(sampled) != index_.end()) return fail("'" + sampled + "' came before the '# TYPE' line of '" + name + "'");
      }
    }

    if (at == npos) at = add_family(name);
    family &target = out_.families[at];
    declared &seen = declared_[at];
    if (help) {
      seen.help = true;
      target.help = help_text;
    } else if (unit) {
      seen.unit = true;
      target.unit = unit_text;
    } else {
      seen.type = true;
      target.type = declared_type;
    }
    return true;
  }

  // The family that already owns `name` as one of its samples, other than as
  // its bare name: a `# TYPE foo_count` after the summary `foo` would describe
  // samples that belong to `foo`.
  std::size_t owner_of(const std::string &name) const {
    for (const char *suffix : sample_suffixes) {
      if (*suffix == '\0' || !ends_with(name, suffix)) continue;
      const std::unordered_map<std::string, std::size_t>::const_iterator it = index_.find(name.substr(0, name.size() - std::strlen(suffix)));
      if (it != index_.end() && owns_suffix(format_, out_.families[it->second].type, suffix)) return it->second;
    }
    return npos;
  }

  bool room_for_family() {
    if (bounds_.max_families != 0 && out_.families.size() >= bounds_.max_families) {
      return fail("more than " + std::to_string(bounds_.max_families) + " families");
    }
    return true;
  }

  std::size_t add_family(const std::string &name) {
    family added;
    added.name = name;
    out_.families.push_back(std::move(added));
    declared_.push_back(declared());
    const std::size_t at = out_.families.size() - 1;
    index_[name] = at;
    current_ = at;
    return at;
  }

  bool owns(const std::size_t at, const std::string_view name) const {
    const family &target = out_.families[at];
    if (name.size() < target.name.size() || name.substr(0, target.name.size()) != target.name) return false;
    return owns_suffix(format_, target.type, name.substr(target.name.size()));
  }

  // The family a sample of this name belongs to: the current family when it
  // owns the name, else any earlier family that does, else a new family of
  // unknown type named after the sample - which is what a sample without any
  // metadata is in both formats. `npos`, with the error set, when the name is
  // a family's own name but not one of its samples (a bare `h` inside the
  // histogram `h`): a second family `h` would make the name ambiguous.
  std::size_t family_for(const std::string &name) {
    if (current_ != npos && owns(current_, name)) return current_;
    for (const char *suffix : sample_suffixes) {
      const std::size_t length = std::strlen(suffix);
      if (length != 0 && !ends_with(name, suffix)) continue;
      const std::unordered_map<std::string, std::size_t>::const_iterator it = index_.find(name.substr(0, name.size() - length));
      if (it == index_.end()) continue;
      const family &found = out_.families[it->second];
      if (owns_suffix(format_, found.type, suffix)) {
        current_ = it->second;
        return it->second;
      }
      if (length == 0) {
        fail("'" + name + "' is not a sample of the " + type_name(found.type) + " family '" + name + "'");
        return npos;
      }
    }
    if (!room_for_family()) return npos;
    return add_family(name);
  }

  static std::string metric_name(cursor &c) {
    const std::size_t start = c.at;
    if (c.done() || !is_name_start(c.peek())) return "";
    while (!c.done() && is_name_char(c.peek())) ++c.at;
    return std::string(c.line.substr(start, c.at - start));
  }

  bool sample_line(const std::string_view text) {
    cursor c;
    c.line = text;
    sample parsed;
    parsed.name = metric_name(c);
    if (parsed.name.empty()) return fail("expected a metric name at the start of the line");
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
    const std::size_t at = family_for(parsed.name);
    if (at == npos) return false;
    out_.families[at].samples.push_back(std::move(parsed));
    ++out_.sample_count;
    return true;
  }

  // Whether `name` is already on the sample being read. The names are views
  // into the line, which outlives the sample.
  bool repeated_label(const std::string_view name) {
    if (label_names_.size() < linear_label_scan) {
      for (const std::string_view &seen : label_names_) {
        if (seen == name) return true;
      }
    } else {
      if (label_set_.empty()) label_set_.insert(label_names_.begin(), label_names_.end());
      if (!label_set_.insert(name).second) return true;
    }
    label_names_.push_back(name);
    return false;
  }

  // `{name="value",...}`, with an optional trailing comma (the Prometheus text
  // format allows one) and blanks around the separators.
  bool labels(cursor &c, sample &parsed) {
    label_names_.clear();
    label_set_.clear();
    ++c.at;
    while (true) {
      c.skip_blanks();
      if (c.done()) return fail("unterminated label set on '" + parsed.name + "'");
      if (c.peek() == '}') {
        ++c.at;
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
      if (repeated_label(name)) return fail("label '" + std::string(name) + "' twice on '" + parsed.name + "'");
      parsed.labels.emplace_back(std::string(name), std::move(value));
      c.skip_blanks();
      if (c.done()) return fail("unterminated label set on '" + parsed.name + "'");
      if (c.peek() == ',') {
        ++c.at;
        continue;
      }
      if (c.peek() != '}') return fail("expected ',' or '}' after label '" + std::string(name) + "' on '" + parsed.name + "'");
    }
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
  // Family name -> index into `out_.families`. Every family name is in it
  // exactly once, which is what keeps names unique.
  std::unordered_map<std::string, std::size_t> index_;
  std::vector<declared> declared_;
  std::size_t current_ = npos;
  // The label names of the sample being read; reused across samples.
  std::vector<std::string_view> label_names_;
  std::unordered_set<std::string_view> label_set_;
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

result parse(const std::string &body, const format body_format, const limits &bounds) {
  result out;
  parser reader(out, body_format, bounds);
  const std::string_view text(body);
  std::size_t start = 0;
  std::size_t number = 0;
  while (start < text.size()) {
    reader.begin_line(++number);
    const std::size_t end = text.find('\n', start);
    const bool terminated = end != std::string_view::npos;
    const std::size_t length = (terminated ? end : text.size()) - start;
    if (bounds.max_line_bytes != 0 && length > bounds.max_line_bytes) {
      reader.fail("line longer than " + std::to_string(bounds.max_line_bytes) + " bytes");
      return out;
    }
    const std::string_view line = text.substr(start, length);
    // Both formats end every line, the last one included, with a line feed. A
    // body that stops without one was cut off - by a size cap, a timeout or a
    // dying exporter - and its last line cannot be trusted even when it reads:
    // `foo 12` cut to `foo 1` is a perfectly good sample. A blank line and
    // `# EOF` are the exceptions, since neither can be the start of anything
    // longer, and plenty of exporters leave the terminator's line feed off.
    if (!terminated) {
      const std::string_view last = normalise(line);
      if (!last.empty() && !is_eof_marker(last)) {
        reader.fail("the body ends in the middle of a line");
        return out;
      }
    }
    if (!reader.line(line)) return out;
    if (!terminated) break;
    start = end + 1;
  }
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
