// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <cmath>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

// The -w / -c short thresholds of the filter checks.
//
// Most monitoring plugins take their bounds as "-w 80 -c 90". A filter check
// takes an expression instead ("warning=load > 80"), which is more capable and
// more to type. This turns the short form into the expression, so that
// everything downstream - the engines, the performance data thresholds, the
// syntaxes - sees exactly what it would have seen had the expression been
// written out.
//
// Kept free of everything but the standard library: it is plain text in, plain
// text out, and unit tested as such (threshold_shorthand_test.cpp).
namespace modern_filter {
namespace shorthand {

// Which way a bare number alerts: "-w 80" is "load > 80" for a CPU and
// "free < 80" for a disk. Only a bare number needs it - a range says so itself.
enum direction { above, below };

enum unit {
  // The value is handed to the filter as written, suffix included ("80", "2d",
  // "10G"): the keyword's own type decides what a suffix means.
  unit_plain,
  // A bare number is a percentage ("-w 10" is "free < 10%"); a suffixed value
  // is handed on as written, so "-w 10G" still compares bytes.
  unit_percent,
  // For a keyword that is itself a percentage held as a plain number (load,
  // free_pct): "-w 80%" and "-w 80" are the same thing, and the % is dropped.
  unit_percent_number,
  // A duration for a keyword that counts milliseconds. A bare number is in
  // seconds, as in the standard plugins; ms, s, m, h, d and w are accepted.
  unit_seconds_as_ms,
  // The same durations for a keyword that counts seconds.
  unit_seconds,
  // A duration for a keyword that counts milliseconds, where a bare number is
  // milliseconds too - check_ping's "-w 100,20%", as in the standard plugin.
  // ms, s, m, h, d and w are accepted.
  unit_milliseconds,
  // A keyword that counts whole days (expires_in, signature_age): a bare
  // number is days, and d and w are accepted. A unit below a day is refused
  // rather than rounded.
  unit_days
};

struct spec {
  std::string variable;
  direction dir;
  unit value_unit;
  // What -w and -c stand at when they are not given, written the way they
  // would be typed ("80", "20%", "5m"); empty when the default has no bound on
  // this keyword. They fill in for a value left out of a list ("-w ,20%"
  // keeps the default time) and are what the documentation shows as the
  // default of the short form.
  std::string warn_default;
  std::string crit_default;
  spec(const std::string &variable, const direction dir, const unit value_unit = unit_plain, const std::string &warn_default = "",
       const std::string &crit_default = "")
      : variable(variable), dir(dir), value_unit(value_unit), warn_default(warn_default), crit_default(crit_default) {}
};
typedef std::vector<spec> spec_list;

namespace detail {

inline std::string trim(const std::string &s) {
  const std::string::size_type b = s.find_first_not_of(" \t");
  if (b == std::string::npos) return "";
  const std::string::size_type e = s.find_last_not_of(" \t");
  return s.substr(b, e - b + 1);
}

inline char lower(const char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }
inline bool is_digit(const char c) { return c >= '0' && c <= '9'; }
inline bool is_alpha(const char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

// A number with an optional unit suffix: 80, 80%, -5, 1.5, 2d, 10GB, 500ms.
inline bool is_value(const std::string &s) {
  std::string::size_type i = 0;
  if (i < s.size() && (s[i] == '-' || s[i] == '+')) ++i;
  std::string::size_type digits = 0;
  while (i < s.size() && is_digit(s[i])) ++i, ++digits;
  if (i < s.size() && s[i] == '.') {
    ++i;
    while (i < s.size() && is_digit(s[i])) ++i, ++digits;
  }
  if (digits == 0) return false;
  if (i < s.size() && s[i] == '%') return i + 1 == s.size();
  while (i < s.size() && is_alpha(s[i])) ++i;
  return i == s.size();
}

// One threshold split into the parts of the plugin guidelines' range format:
// [@][start:][end]. "has_colon" tells "10" (a bare number) from "10:".
struct range {
  bool inside;
  bool has_colon;
  std::string start;  // empty, "~" or a value
  std::string end;    // empty or a value
  range() : inside(false), has_colon(false) {}
};

inline bool parse_range(const std::string &text, range &r) {
  std::string s = text;
  if (!s.empty() && s[0] == '@') {
    r.inside = true;
    s = s.substr(1);
  }
  const std::string::size_type colon = s.find(':');
  if (colon == std::string::npos) {
    r.end = s;
    return is_value(r.end);
  }
  r.has_colon = true;
  r.start = s.substr(0, colon);
  r.end = s.substr(colon + 1);
  if (r.start != "~" && !is_value(r.start)) return false;
  if (!r.end.empty() && !is_value(r.end)) return false;
  // "~:" bounds nothing at all.
  return !(r.start == "~" && r.end.empty());
}

inline std::vector<std::string> split_list(const std::string &s) {
  std::vector<std::string> ret;
  std::string::size_type pos = 0;
  while (true) {
    const std::string::size_type next = s.find(',', pos);
    if (next == std::string::npos) {
      ret.push_back(trim(s.substr(pos)));
      return ret;
    }
    ret.push_back(trim(s.substr(pos, next - pos)));
    pos = next + 1;
  }
}

// Never in scientific notation: the result is read back by the filter parser,
// and "-w 1234.5678" on a millisecond keyword is 1234567.8, not 1.23457e+06.
inline std::string render_number(const double value) {
  const double rounded = std::floor(value + 0.5);
  if (std::fabs(value - rounded) < 1e-9 && std::fabs(rounded) < 1e15) {
    std::ostringstream ss;
    ss << static_cast<long long>(rounded);
    return ss.str();
  }
  std::ostringstream ss;
  ss.setf(std::ios::fixed, std::ios::floatfield);
  ss.precision(6);
  ss << value;
  std::string ret = ss.str();
  const std::string::size_type last = ret.find_last_not_of('0');
  ret.erase(ret[last] == '.' ? last : last + 1);
  return ret;
}

// Whether the keyword holds a duration, in whatever unit.
inline bool is_duration(const unit u) { return u == unit_seconds_as_ms || u == unit_seconds || u == unit_milliseconds || u == unit_days; }

// "5", "5s", "500ms", "5m", "1.5h" -> seconds, a bare number being
// `bare_seconds` seconds. False for a suffix that is not a duration.
inline bool parse_duration(const std::string &value, double &seconds, const double bare_seconds = 1.0) {
  std::string::size_type i = 0;
  while (i < value.size() && !is_alpha(value[i]) && value[i] != '%') ++i;
  const std::string number = value.substr(0, i);
  const std::string suffix = value.substr(i);
  const double n = std::strtod(number.c_str(), nullptr);
  if (suffix.empty()) {
    seconds = n * bare_seconds;
  } else if (suffix == "s") {
    seconds = n;
  } else if (suffix == "ms") {
    seconds = n / 1000.0;
  } else if (suffix == "m") {
    seconds = n * 60;
  } else if (suffix == "h") {
    seconds = n * 3600;
  } else if (suffix == "d") {
    seconds = n * 86400;
  } else if (suffix == "w") {
    seconds = n * 604800;
  } else {
    return false;
  }
  return true;
}

// What a bare number of the keyword amounts to, in seconds.
inline double bare_seconds_of(const unit u) {
  if (u == unit_milliseconds) return 0.001;
  if (u == unit_days) return 86400.0;
  return 1.0;
}

inline std::string suffix_of(const std::string &value) {
  std::string::size_type i = 0;
  while (i < value.size() && !is_alpha(value[i]) && value[i] != '%') ++i;
  return value.substr(i);
}

// k, m, g, t, p, e with an optional b, or a b alone: the size suffixes.
inline bool is_size_suffix(const std::string &suffix) {
  if (suffix.empty() || suffix.size() > 2) return false;
  const char first = lower(suffix[0]);
  const bool scale = first == 'k' || first == 'm' || first == 'g' || first == 't' || first == 'p' || first == 'e';
  if (suffix.size() == 1) return scale || first == 'b';
  return scale && lower(suffix[1]) == 'b';
}

// The number of a value, without its suffix.
inline double number_of(const std::string &value) {
  return std::strtod(value.substr(0, value.size() - suffix_of(value).size()).c_str(), nullptr);
}

// Two values of one keyword as numbers that can be compared with each other.
// False when that cannot be told here: a plain keyword gives a suffix its
// meaning itself ("2d", "1w"), so only values with the same one are compared.
inline bool comparable(const spec &s, const std::string &a, const std::string &b, double &na, double &nb) {
  if (is_duration(s.value_unit)) return parse_duration(a, na, bare_seconds_of(s.value_unit)) && parse_duration(b, nb, bare_seconds_of(s.value_unit));
  std::string sa = suffix_of(a), sb = suffix_of(b);
  for (char &c : sa) c = lower(c);
  for (char &c : sb) c = lower(c);
  // A bare number of a percentage keyword is a percentage.
  if (s.value_unit == unit_percent || s.value_unit == unit_percent_number) {
    if (sa.empty()) sa = "%";
    if (sb.empty()) sb = "%";
  }
  na = number_of(a);
  nb = number_of(b);
  if (sa == sb) return true;
  // A bare number next to a size is a number of bytes ("-w 2000:1k").
  if (sa.empty() && is_size_suffix(sb)) sa = "b";
  if (sb.empty() && is_size_suffix(sa)) sb = "b";
  if (!is_size_suffix(sa) || !is_size_suffix(sb)) return false;
  const std::string scales = "bkmgtpe";
  na *= std::pow(1024.0, static_cast<double>(scales.find(sa[0])));
  nb *= std::pow(1024.0, static_cast<double>(scales.find(sb[0])));
  return true;
}

// The value as the filter wants to read it for this keyword.
inline bool render_value(const spec &s, const std::string &value, std::string &out, std::string &error) {
  const std::string suffix = suffix_of(value);
  switch (s.value_unit) {
    case unit_percent:
      if (suffix.empty()) {
        out = value + "%";
      } else if (suffix == "%" || is_size_suffix(suffix)) {
        out = value;
      } else {
        error = "Invalid unit for " + s.variable + ": " + value + " (expected a percentage, or a size such as 10G)";
        return false;
      }
      return true;
    case unit_percent_number:
      if (!suffix.empty() && suffix != "%") {
        error = "Invalid unit for " + s.variable + ": " + value + " (expected a percentage)";
        return false;
      }
      out = suffix.empty() ? value : value.substr(0, value.size() - 1);
      return true;
    case unit_seconds_as_ms:
    case unit_seconds: {
      double seconds = 0;
      if (!parse_duration(value, seconds)) {
        error = "Invalid duration for " + s.variable + ": " + value + " (expected a number of seconds, or a number with one of ms, s, m, h, d, w)";
        return false;
      }
      out = render_number(s.value_unit == unit_seconds_as_ms ? seconds * 1000.0 : seconds);
      return true;
    }
    case unit_milliseconds: {
      double seconds = 0;
      if (!parse_duration(value, seconds, 0.001)) {
        error = "Invalid duration for " + s.variable + ": " + value + " (expected a number of milliseconds, or a number with one of ms, s, m, h, d, w)";
        return false;
      }
      out = render_number(seconds * 1000.0);
      return true;
    }
    case unit_days: {
      double seconds = 0;
      if (suffix == "ms" || suffix == "s" || suffix == "m" || suffix == "h" || !parse_duration(value, seconds, 86400.0)) {
        error = "Invalid duration for " + s.variable + ": " + value + " (the keyword counts days: expected a number of days, or a number with d or w)";
        return false;
      }
      out = render_number(seconds / 86400.0);
      return true;
    }
    case unit_plain:
    default:
      out = value;
      return true;
  }
}

inline bool expand_range(const spec &s, const std::string &text, std::string &out, std::string &error) {
  range r;
  if (!parse_range(text, r)) {
    error = "Invalid threshold: " + text;
    return false;
  }
  std::string start, end;
  if (!r.end.empty() && !render_value(s, r.end, end, error)) return false;
  if (!r.start.empty() && r.start != "~" && !render_value(s, r.start, start, error)) return false;
  // "20:10" would be "load < 20 or load > 10", which everything matches: an
  // alert that never clears. The guidelines require start <= end.
  double low = 0, high = 0;
  if (!start.empty() && !end.empty() && comparable(s, r.start, r.end, low, high) && low > high) {
    error = "Invalid threshold: " + text + " (a range starts at its low end, as in " + r.end + ":" + r.start + ")";
    return false;
  }
  const std::string &v = s.variable;
  if (!r.has_colon) {
    if (r.inside) {
      // "@10" is "inside 0..10".
      out = v + " >= 0 and " + v + " <= " + end;
    } else {
      out = v + (s.dir == above ? " > " : " < ") + end;
    }
    return true;
  }
  const bool open_start = r.start == "~";
  if (r.inside) {
    if (open_start) {
      out = v + " <= " + end;
    } else if (end.empty()) {
      out = v + " >= " + start;
    } else {
      out = v + " >= " + start + " and " + v + " <= " + end;
    }
  } else {
    if (open_start) {
      out = v + " > " + end;
    } else if (end.empty()) {
      out = v + " < " + start;
    } else {
      out = v + " < " + start + " or " + v + " > " + end;
    }
  }
  return true;
}
}  // namespace detail

// Whether the value is a threshold (a number, a range, or a comma separated
// list of them) rather than a filter expression. An expression is handed on
// untouched, so -w "used > 80%" keeps working.
inline bool is_threshold(const std::string &value) {
  const std::vector<std::string> parts = detail::split_list(detail::trim(value));
  bool any = false;
  for (const std::string &part : parts) {
    if (part.empty()) continue;
    detail::range r;
    if (!detail::parse_range(part, r)) return false;
    any = true;
  }
  return any;
}

// ---------------------------------------------------------------------------
// Templates: what the short form stands for.
//
// A check's threshold is often more than a bound. check_tcp is critical on
// "time > 5000 or result != 'ok'", check_nps_auth warns on
// "decisions >= min_requests and reject_pct > 10". Somebody writing "-c 10" or
// "-w 20" is moving the number, not asking for a refused connection to stop
// being critical or for the guard to go away.
//
// So a check says what its short form stands for with a template per side, in
// which ${keyword} is the bound on that keyword:
//
//   "${time} or result != 'ok'"
//   "decisions >= min_requests and ${reject_pct}"
//
// The value for a keyword is what the user passed, or else the keyword's
// default for that side, or else nothing - and an alternative (the parts
// between top level "or"s) whose keyword has no value is left out, guard and
// all. Nothing is ever derived from the text of the check's default
// expression: the template is the single statement of what -w and -c mean.
// ---------------------------------------------------------------------------
enum side { warning, critical };

namespace detail {

inline bool is_word(const char c) { return is_alpha(c) || is_digit(c) || c == '_'; }

// The alternatives of an expression: split on "or" outside quotes and
// parentheses. "a or b and c" is two alternatives, as "and" binds tighter.
inline std::vector<std::string> split_alternatives(const std::string &expression) {
  std::vector<std::string> ret;
  int depth = 0;
  char quote = 0;
  std::string::size_type start = 0;
  for (std::string::size_type i = 0; i < expression.size(); ++i) {
    const char c = expression[i];
    if (quote != 0) {
      if (c == quote) quote = 0;
      continue;
    }
    if (c == '\'' || c == '"') {
      quote = c;
    } else if (c == '(') {
      ++depth;
    } else if (c == ')') {
      --depth;
    } else if (depth == 0 && lower(c) == 'o' && i + 1 < expression.size() && lower(expression[i + 1]) == 'r' && i > 0 && !is_word(expression[i - 1]) &&
               (i + 2 >= expression.size() || !is_word(expression[i + 2]))) {
      ret.push_back(trim(expression.substr(start, i - start)));
      start = i + 2;
      ++i;
    }
  }
  ret.push_back(trim(expression.substr(start)));
  return ret;
}

// One alternative with its ${keyword}s filled in from `bounds` (indexed like
// the specs, empty where the keyword has no value). False when a keyword it
// names has none, or is not one of the check's.
inline bool fill_alternative(const spec_list &specs, const std::vector<std::string> &bounds, const std::string &alternative, std::string &out) {
  out = "";
  std::string::size_type pos = 0;
  while (true) {
    const std::string::size_type open = alternative.find("${", pos);
    if (open == std::string::npos) break;
    const std::string::size_type close = alternative.find('}', open);
    if (close == std::string::npos) break;
    const std::string name = alternative.substr(open + 2, close - open - 2);
    std::string bound;
    for (std::string::size_type i = 0; i < specs.size(); ++i) {
      if (specs[i].variable == name) bound = bounds[i];
    }
    if (bound.empty()) return false;
    out += alternative.substr(pos, open - pos);
    // A range is two comparisons; next to anything else it needs parentheses.
    const bool alone = open == 0 && close + 1 == alternative.size();
    const bool compound = bound.find(" or ") != std::string::npos || bound.find(" and ") != std::string::npos;
    out += (compound && !alone) ? "(" + bound + ")" : bound;
    pos = close + 1;
  }
  out += alternative.substr(pos);
  return true;
}

inline std::string fill_template(const spec_list &specs, const std::vector<std::string> &bounds, const std::string &text) {
  std::string ret;
  for (const std::string &alternative : split_alternatives(text)) {
    if (alternative.empty()) continue;
    std::string filled;
    if (!fill_alternative(specs, bounds, alternative, filled)) continue;
    ret += (ret.empty() ? "" : " or ") + filled;
  }
  return ret;
}

// The template of a check that set none: a bound on each keyword.
inline std::string default_template(const spec_list &specs) {
  std::string ret;
  for (const spec &s : specs) ret += (ret.empty() ? "${" : " or ${") + s.variable + "}";
  return ret;
}
}  // namespace detail

inline std::string missing_value_error(const char flag) {
  return std::string("Missing value for -") + flag + " (use none to remove the threshold)";
}

// The expressions a -w / -c value stands for, one per template (a check with
// none of its own gets a bound on each keyword). `value` holds one threshold
// per keyword, comma separated; one that is left out ("-w ,20%") falls back to
// the keyword's default for that side. When the value is not a threshold at
// all it is an expression and is returned as it is. False, with `error` set,
// when the value is a threshold that does not fit the check.
inline bool expand(const spec_list &specs, const std::vector<std::string> &templates, const side which, const std::string &value,
                   std::vector<std::string> &expressions, std::string &error) {
  const std::string trimmed = detail::trim(value);
  expressions.clear();
  if (trimmed.empty()) {
    error = missing_value_error(which == warning ? 'w' : 'c');
    return false;
  }
  // A list with every value left out ("-w ,", what "-w $ARG1$,$ARG2$" is with
  // both macros unset) names no bound at all. It is not an expression either,
  // so it is refused like any other value that was not given.
  if (trimmed.find_first_not_of(", \t") == std::string::npos) {
    error = missing_value_error(which == warning ? 'w' : 'c');
    return false;
  }
  if (!is_threshold(trimmed)) {
    expressions.push_back(trimmed);
    return true;
  }
  const std::vector<std::string> parts = detail::split_list(trimmed);
  if (parts.size() > specs.size()) {
    std::string names;
    for (const spec &s : specs) names += (names.empty() ? "" : ",") + s.variable;
    std::ostringstream ss;
    ss << "Too many values in threshold " << trimmed << ": expected at most " << specs.size() << " (" << names << ")";
    error = ss.str();
    return false;
  }
  std::vector<std::string> bounds(specs.size());
  for (std::string::size_type i = 0; i < specs.size(); ++i) {
    std::string text = i < parts.size() ? parts[i] : "";
    if (text.empty()) text = which == warning ? specs[i].warn_default : specs[i].crit_default;
    if (text.empty()) continue;
    if (!detail::expand_range(specs[i], text, bounds[i], error)) return false;
  }
  if (templates.empty()) {
    const std::string filled = detail::fill_template(specs, bounds, detail::default_template(specs));
    if (!filled.empty()) expressions.push_back(filled);
    return true;
  }
  for (const std::string &text : templates) {
    const std::string filled = detail::fill_template(specs, bounds, text);
    if (!filled.empty()) expressions.push_back(filled);
  }
  return true;
}

// What the short form reads as, for the option's help text: the templates with
// a symbolic bound in each ${keyword} - "load > N" for one keyword,
// "time > TIME or loss > LOSS%" for a list, "time > N*1000" where N is in
// seconds and the keyword counts milliseconds.
inline std::string describe(const spec_list &specs, const std::vector<std::string> &templates) {
  std::vector<std::string> bounds;
  for (const spec &s : specs) {
    std::string name = "N";
    if (specs.size() > 1) {
      name = s.variable;
      for (char &c : name) c = (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
    }
    // What the bound really is: a number of seconds on a keyword that counts
    // milliseconds is that number times 1000.
    const std::string scale = s.value_unit == unit_percent ? "%" : s.value_unit == unit_seconds_as_ms ? "*1000" : "";
    bounds.push_back(s.variable + (s.dir == above ? " > " : " < ") + name + scale);
  }
  std::string ret;
  if (templates.empty()) return detail::fill_template(specs, bounds, detail::default_template(specs));
  for (const std::string &text : templates) {
    const std::string filled = detail::fill_template(specs, bounds, text);
    if (!filled.empty()) ret += (ret.empty() ? "" : "\" and \"") + filled;
  }
  return ret;
}

// How the short form is typed, for the help text: "-w N" or "-w TIME,LOSS".
inline std::string describe_usage(const spec_list &specs) {
  if (specs.size() == 1) return "N";
  std::string ret;
  for (const spec &s : specs) {
    std::string name = s.variable;
    for (char &c : name) c = (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
    ret += (ret.empty() ? "" : ",") + name;
  }
  return ret;
}

// The defaults of the short form as they would be typed ("80", "60,5%"), or
// empty when no keyword has one for that side.
inline std::string default_value(const spec_list &specs, const side which) {
  std::string ret;
  bool any = false;
  for (std::string::size_type i = 0; i < specs.size(); ++i) {
    const std::string &value = which == warning ? specs[i].warn_default : specs[i].crit_default;
    if (!value.empty()) any = true;
    ret += (i == 0 ? "" : ",") + value;
  }
  return any ? ret : "";
}

// The unit a bare number is read in, for the help text.
inline std::string describe_unit(const spec &s) {
  switch (s.value_unit) {
    case unit_percent:
      return "A number without a unit is a percentage; a size (10G) is used as written.";
    case unit_seconds_as_ms:
      return "A number without a unit is in seconds, which the keyword counts in milliseconds; ms, s, m, h, d and w are accepted as units (500ms, 5m).";
    case unit_milliseconds:
      return "A number without a unit is in milliseconds; ms, s, m, h, d and w are accepted as units (500ms, 2s).";
    case unit_days:
      return "A number without a unit is in days; d and w are accepted as units (14d, 2w).";
    case unit_seconds:
      return "A number without a unit is in seconds; ms, s, m, h, d and w are accepted as units (500ms, 5m).";
    case unit_plain:
    case unit_percent_number:
    default:
      return "";
  }
}

// ---------------------------------------------------------------------------
// Command line: rewriting "-w 80" into something the option parser reads.
//
// Arguments reach a check in one of two shapes, decided by the first of them
// (nscapi::program_options::process_arguments_from_request): "key=value"
// tokens, or "--key value" pairs. Neither reads "-w 80", and a "-w" in front
// would even pick the wrong shape for everything after it. So the short form
// is rewritten into the shape the rest of the line uses before any of that
// runs: "w=80" among key=value arguments, "--w=80" among dashed ones.
// ---------------------------------------------------------------------------

inline bool is_short_flag(const std::string &arg, char &flag, std::string &attached) {
  if (arg.size() < 2 || arg[0] != '-' || (arg[1] != 'w' && arg[1] != 'c')) return false;
  if (arg.size() == 2) {
    flag = arg[1];
    attached = "";
    return true;
  }
  // "-w80", "-w=80", "-w10:", "-w@10:20", "-w~:10", "-w.5", "-c-5:". Anything
  // else that merely starts with -w or -c is somebody else's argument.
  const char next = arg[2];
  if (next == '=') {
    flag = arg[1];
    attached = arg.substr(3);
    return true;
  }
  // A sign only when a number follows it: "-c-5:" but not "-c-foo".
  const bool sign = next == '-' || next == '+';
  const char first = sign ? (arg.size() > 3 ? arg[3] : '\0') : next;
  if (detail::is_digit(first) || first == '.' || (!sign && (first == '@' || first == '~'))) {
    flag = arg[1];
    attached = arg.substr(2);
    return true;
  }
  return false;
}

// Whether the argument is a "key=value" pair: an option name with the "="
// right behind it. An expression has its operator set off by spaces
// ("state = 'ok'") or is not an "=" at all ("used>80%").
inline bool is_key_value(const std::string &arg) {
  if (arg.empty() || !detail::is_alpha(arg[0])) return false;
  std::string::size_type i = 1;
  while (i < arg.size() && (detail::is_word(arg[i]) || arg[i] == '-')) ++i;
  return i < arg.size() && arg[i] == '=';
}

// Whether the argument after a bare "-w" is its value. Another short
// threshold, a dashed option or a key=value argument is not: in "-w -c 90" and
// "-w drive=/" the -w was given no value. That is what an unset macro leaves
// behind ("-w $ARG1$ drive=C:"), and taking the next argument for it would run
// the check without its drive and with "drive=/" for a warning. A negative
// bound ("-c -5:") is a value, and so is an expression ("-w "load = 5"").
inline bool is_flag_value(const std::string &arg) {
  char flag;
  std::string attached;
  if (is_short_flag(arg, flag, attached)) return false;
  if (is_key_value(arg)) return false;
  return !(arg.size() > 2 && arg[0] == '-' && arg[1] == '-');
}

// The -w / -c that was given no value, or 0: "w=" and "--w=" in the arguments
// normalize_arguments() returned. Asked before the option parser runs, as it
// reports an empty "--w=" in its own words and with the whole option list.
inline char find_missing_value(const std::vector<std::string> &normalized) {
  for (const std::string &arg : normalized) {
    if (arg == "w=" || arg == "--w=") return 'w';
    if (arg == "c=" || arg == "--c=") return 'c';
  }
  return 0;
}

inline std::vector<std::string> normalize_arguments(const std::vector<std::string> &args) {
  // The shape of the line is that of its first argument that is not a short
  // threshold (nor the value of one).
  bool dashed = false;
  for (std::string::size_type i = 0; i < args.size(); ++i) {
    char flag;
    std::string attached;
    if (is_short_flag(args[i], flag, attached)) {
      if (args[i].size() == 2 && i + 1 < args.size() && is_flag_value(args[i + 1])) ++i;
      continue;
    }
    dashed = args[i].size() > 2 && args[i][0] == '-' && args[i][1] == '-';
    break;
  }
  std::vector<std::string> ret;
  for (std::string::size_type i = 0; i < args.size(); ++i) {
    char flag;
    std::string value;
    if (!is_short_flag(args[i], flag, value)) {
      ret.push_back(args[i]);
      continue;
    }
    // A "-w" at the end, or in front of another option, has no value: it is
    // passed on empty, which is refused as a missing value.
    if (args[i].size() == 2 && i + 1 < args.size() && is_flag_value(args[i + 1])) value = args[++i];
    if (dashed) {
      // One token: "--c" followed by "-5:" would have the value read as an
      // option.
      ret.push_back(std::string("--") + flag + "=" + value);
    } else {
      ret.push_back(std::string(1, flag) + "=" + value);
    }
  }
  return ret;
}

}  // namespace shorthand
}  // namespace modern_filter
