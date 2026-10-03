// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <cmath>
#include <cstring>
#include <limits>
#include <locale>
#include <metrics/openmetrics_parser.hpp>
#include <sstream>
#include <string_view>
#include <unordered_map>

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

bool ends_with(const std::string &value, const std::string &suffix) {
  return value.size() > suffix.size() && value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string lower(std::string_view raw) {
  std::string ret(raw);
  for (char &c : ret) {
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  }
  return ret;
}

// One line of the body, read left to right. A view rather than a copy: a
// label value can be as long as the whole body, and the body can be as long as
// `max response mb` allows.
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

class number_reader {
 public:
  number_reader() { stream_.imbue(std::locale::classic()); }

  // A sample value or timestamp. Both formats spell the non-finite values in
  // words, and Go's `ParseFloat` - which is what the reference parser and most
  // exporters use - accepts them in any case, so this does too. Everything
  // else is held to the decimal float grammar before it reaches the stream,
  // so a hex float, a stray `0x` or a trailing `ms` is an error rather than a
  // partial read.
  bool read(std::string_view raw, double &out) {
    if (raw.empty()) return false;
    const std::string word = lower(raw);
    if (word == "nan" || word == "+nan" || word == "-nan") {
      out = std::numeric_limits<double>::quiet_NaN();
      return true;
    }
    if (word == "inf" || word == "+inf" || word == "infinity" || word == "+infinity") {
      out = std::numeric_limits<double>::infinity();
      return true;
    }
    if (word == "-inf" || word == "-infinity") {
      out = -std::numeric_limits<double>::infinity();
      return true;
    }
    if (!is_decimal(raw)) return false;
    stream_.clear();
    stream_.str(std::string(raw));
    double value = 0;
    stream_ >> value;
    // A decimal that does not fit a double (`1e400`) is refused rather than
    // read as infinity: the non-finite values have their own spellings, and
    // whether the stream flags the overflow or hands back HUGE_VAL differs
    // between standard libraries.
    if (stream_.fail() || std::isinf(value)) return false;
    out = value;
    return true;
  }

 private:
  // `[+-]? (digits [. digits?] | . digits) ([eE] [+-]? digits)?`
  static bool is_decimal(std::string_view raw) {
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

  std::istringstream stream_;
};

// What the metadata lines of a family have said so far, kept beside the family
// rather than in it because none of it is part of what the caller reads.
struct declared {
  // The name the metadata lines used, which for a counter in the Prometheus
  // text format is the sample name (`foo_total`) rather than the family name.
  std::string name;
  bool help = false;
  bool type = false;
  bool unit = false;
};

// Which sample names a family of this type owns, by the suffix on its name.
bool owns_suffix(const family_type type, const std::string &suffix) {
  switch (type) {
    case family_type::counter:
      // The bare name too: the Prometheus text format never required the
      // `_total`, and plenty of older exporters leave it off.
      return suffix.empty() || suffix == "_total" || suffix == "_created";
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

bool parse_type(const std::string &word, family_type &out) {
  if (word == "counter") {
    out = family_type::counter;
  } else if (word == "gauge") {
    out = family_type::gauge;
  } else if (word == "histogram") {
    out = family_type::histogram;
  } else if (word == "gaugehistogram") {
    out = family_type::gaugehistogram;
  } else if (word == "summary") {
    out = family_type::summary;
  } else if (word == "info") {
    out = family_type::info;
  } else if (word == "stateset") {
    out = family_type::stateset;
  } else if (word == "unknown" || word == "untyped") {
    // `untyped` is the Prometheus text format's word for the same thing.
    out = family_type::unknown;
  } else {
    return false;
  }
  return true;
}

// `# HELP` text. Both formats escape a backslash and a line feed; OpenMetrics
// also escapes a double quote. Any other backslash is kept as served: help is
// prose for a person, and refusing a whole scrape over one would cost far more
// than it protects.
std::string unescape_help(std::string_view raw) {
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

class parser {
 public:
  parser(result &out, const limits &bounds) : out_(out), bounds_(bounds) {}

  // Reads one line, without its line feed. Returns false, with the error set,
  // when the line cannot be read.
  bool line(std::string_view text) {
    if (!text.empty() && text[text.size() - 1] == '\r') text = text.substr(0, text.size() - 1);
    // Blank lines are not part of OpenMetrics, but the Prometheus text format
    // allows them and some exporters separate families with one - or end the
    // body with one after `# EOF`.
    std::size_t first = 0;
    while (first < text.size() && is_blank(text[first])) ++first;
    if (first == text.size()) return true;
    // OpenMetrics ends the document at `# EOF`. Anything after it is either a
    // second document glued on or a proxy appending to the body, and neither
    // belongs in this scrape.
    if (out_.saw_eof) return fail("text after '# EOF'");
    text = text.substr(first);
    if (text[0] == '#') return comment(text);
    return sample_line(text);
  }

  bool fail(const std::string &why) {
    out_.error = why;
    return false;
  }

 private:
  bool comment(std::string_view text) {
    cursor c;
    c.line = text;
    c.at = 1;
    c.skip_blanks();
    const std::string_view keyword = c.token();
    if (keyword == "EOF") {
      c.skip_blanks();
      if (!c.done()) return fail("unexpected text after '# EOF'");
      out_.saw_eof = true;
      return true;
    }
    const bool help = keyword == "HELP";
    const bool type = keyword == "TYPE";
    const bool unit = keyword == "UNIT";
    // Anything else is a comment, which the Prometheus text format allows.
    if (!help && !type && !unit) return true;
    if (c.done() || !is_blank(c.peek())) return fail("expected a metric name after '# " + std::string(keyword) + "'");
    c.skip_blanks();
    const std::string name = metric_name(c);
    if (name.empty()) return fail("expected a metric name after '# " + std::string(keyword) + "'");
    if (!c.done() && !is_blank(c.peek())) return fail("invalid character in metric name '" + name + "'");
    c.skip_blanks();

    const std::size_t at = open_metadata(name);
    if (at == npos) return false;
    family &target = out_.families[at];
    declared &seen = declared_[at];

    if (help) {
      if (seen.help) return fail("second '# HELP' line for '" + name + "'");
      seen.help = true;
      target.help = unescape_help(c.rest());
      return true;
    }
    if (unit) {
      if (seen.unit) return fail("second '# UNIT' line for '" + name + "'");
      seen.unit = true;
      const std::string_view value = c.token();
      c.skip_blanks();
      if (!c.done()) return fail("unexpected text after the unit of '" + name + "'");
      target.unit = std::string(value);
      return true;
    }
    if (seen.type) return fail("second '# TYPE' line for '" + name + "'");
    seen.type = true;
    const std::string word = std::string(c.token());
    c.skip_blanks();
    if (!c.done()) return fail("unexpected text after the type of '" + name + "'");
    if (!parse_type(word, target.type)) return fail("unknown type '" + word + "' for '" + name + "'");
    return fold_name(at);
  }

  // A counter declared under its sample name (`# TYPE foo_total counter`, the
  // Prometheus text format), or an info family under `foo_info`, is renamed to
  // the family the OpenMetrics form would have declared, so both dialects of
  // one exposition land on the same family.
  bool fold_name(const std::size_t at) {
    family &target = out_.families[at];
    std::string suffix;
    if (target.type == family_type::counter) suffix = "_total";
    if (target.type == family_type::info) suffix = "_info";
    if (suffix.empty() || !ends_with(target.name, suffix)) return true;
    const std::string folded = target.name.substr(0, target.name.size() - suffix.size());
    if (index_.find(folded) != index_.end()) return fail("'" + target.name + "' folds onto '" + folded + "', which is already a family");
    index_.erase(target.name);
    index_[folded] = at;
    target.name = folded;
    return true;
  }

  // The family a metadata line describes: the one the previous metadata line
  // opened, while it has no samples yet, or a new one. Both formats keep a
  // family's lines together and put its metadata first, so a name that was
  // already seen anywhere else is either a second declaration or a family
  // split in two, and the body is not one this parser can describe.
  std::size_t open_metadata(const std::string &name) {
    if (current_ != npos) {
      const family &target = out_.families[current_];
      if (target.samples.empty() && (target.name == name || declared_[current_].name == name)) return current_;
    }
    if (index_.find(name) != index_.end() || declared_names_.find(name) != declared_names_.end()) {
      fail("metadata for '" + name + "' after that family was already declared or sampled");
      return npos;
    }
    const std::size_t at = add_family(name);
    declared_[at].name = name;
    declared_names_[name] = at;
    return at;
  }

  std::size_t add_family(const std::string &name) {
    family added;
    added.name = name;
    out_.families.push_back(added);
    declared_.push_back(declared());
    const std::size_t at = out_.families.size() - 1;
    index_[name] = at;
    current_ = at;
    return at;
  }

  // The family a sample of this name belongs to: the current family when it
  // owns the name, else any earlier family that does, else a new family of
  // unknown type named after the sample - which is what a sample without any
  // metadata is in both formats.
  std::size_t family_for(const std::string &name) {
    if (current_ != npos && owns(current_, name)) return current_;
    for (const char *suffix : sample_suffixes) {
      const std::size_t length = std::strlen(suffix);
      if (length != 0 && !ends_with(name, suffix)) continue;
      const std::unordered_map<std::string, std::size_t>::const_iterator it = index_.find(name.substr(0, name.size() - length));
      if (it == index_.end()) continue;
      if (owns_suffix(out_.families[it->second].type, suffix)) {
        current_ = it->second;
        return it->second;
      }
    }
    return add_family(name);
  }

  bool owns(const std::size_t at, const std::string &name) const {
    const family &target = out_.families[at];
    if (name.compare(0, target.name.size(), target.name) != 0) return false;
    return owns_suffix(target.type, name.substr(target.name.size()));
  }

  static std::string metric_name(cursor &c) {
    const std::size_t start = c.at;
    if (c.done() || !is_name_start(c.peek())) return "";
    while (!c.done() && is_name_char(c.peek())) ++c.at;
    return std::string(c.line.substr(start, c.at - start));
  }

  bool sample_line(std::string_view text) {
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
    if (!numbers_.read(value, parsed.value)) return fail("invalid value '" + std::string(value) + "' for '" + parsed.name + "'");
    c.skip_blanks();
    if (!c.done() && c.peek() != '#') {
      const std::string_view stamp = c.token();
      double timestamp = 0;
      if (!numbers_.read(stamp, timestamp)) return fail("invalid timestamp '" + std::string(stamp) + "' for '" + parsed.name + "'");
      parsed.timestamp = timestamp;
      c.skip_blanks();
    }
    // An exemplar (`# {trace_id="..."} 1`) is the only thing allowed after the
    // value or timestamp. It describes one traced request rather than the
    // series, so it is not kept.
    if (!c.done() && c.peek() != '#') return fail("unexpected text after the value of '" + parsed.name + "'");

    if (bounds_.max_series != 0 && out_.sample_count >= bounds_.max_series) {
      return fail("more than " + std::to_string(bounds_.max_series) + " series");
    }
    const std::size_t at = family_for(parsed.name);
    out_.families[at].samples.push_back(std::move(parsed));
    ++out_.sample_count;
    return true;
  }

  // `{name="value",...}`, with an optional trailing comma (the Prometheus text
  // format allows one) and blanks around the separators.
  bool labels(cursor &c, sample &parsed) {
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
      std::string name(c.line.substr(start, c.at - start));
      c.skip_blanks();
      if (c.done() || c.peek() != '=') return fail("expected '=' after label '" + name + "' on '" + parsed.name + "'");
      ++c.at;
      c.skip_blanks();
      if (c.done() || c.peek() != '"') return fail("expected a quoted value for label '" + name + "' on '" + parsed.name + "'");
      ++c.at;
      std::string value;
      if (!label_value(c, value)) return fail("unterminated or badly escaped value for label '" + name + "' on '" + parsed.name + "'");
      for (const std::pair<std::string, std::string> &existing : parsed.labels) {
        if (existing.first == name) return fail("label '" + name + "' twice on '" + parsed.name + "'");
      }
      parsed.labels.emplace_back(std::move(name), std::move(value));
      c.skip_blanks();
      if (c.done()) return fail("unterminated label set on '" + parsed.name + "'");
      if (c.peek() == ',') {
        ++c.at;
        continue;
      }
      if (c.peek() != '}') return fail("expected ',' or '}' after label '" + parsed.labels.back().first + "' on '" + parsed.name + "'");
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
  const limits &bounds_;
  number_reader numbers_;
  // Family name -> index into `out_.families`, kept in step with the folds.
  std::unordered_map<std::string, std::size_t> index_;
  // The names metadata lines have used, including the pre-fold ones.
  std::unordered_map<std::string, std::size_t> declared_names_;
  std::vector<declared> declared_;
  std::size_t current_ = npos;
};

}  // namespace

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

result parse(const std::string &body, const limits &bounds) {
  result out;
  parser reader(out, bounds);
  const std::string_view text(body);
  std::size_t start = 0;
  std::size_t number = 0;
  while (start < text.size()) {
    ++number;
    const std::size_t end = text.find('\n', start);
    const bool terminated = end != std::string_view::npos;
    const std::size_t length = (terminated ? end : text.size()) - start;
    if (bounds.max_line_bytes != 0 && length > bounds.max_line_bytes) {
      reader.fail("line longer than " + std::to_string(bounds.max_line_bytes) + " bytes");
      out.error_line = number;
      return out;
    }
    const std::string_view line = text.substr(start, length);
    // Both formats end every line, the last one included, with a line feed. A
    // body that stops without one was cut off - by a size cap, a timeout or a
    // dying exporter - and its last line cannot be trusted even when it reads:
    // `foo 12` cut to `foo 1` is a perfectly good sample. `# EOF` is the one
    // exception, since it is the terminator itself and plenty of exporters
    // leave its line feed off.
    if (!terminated && line != "# EOF" && line != "# EOF\r") {
      reader.fail("the body ends in the middle of a line");
      out.error_line = number;
      return out;
    }
    if (!reader.line(line)) {
      out.error_line = number;
      return out;
    }
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
