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
  unit_seconds
};

struct spec {
  std::string variable;
  direction dir;
  unit value_unit;
  spec(const std::string &variable, const direction dir, const unit value_unit = unit_plain) : variable(variable), dir(dir), value_unit(value_unit) {}
};
typedef std::vector<spec> spec_list;

namespace detail {

inline std::string trim(const std::string &s) {
  const std::string::size_type b = s.find_first_not_of(" \t");
  if (b == std::string::npos) return "";
  const std::string::size_type e = s.find_last_not_of(" \t");
  return s.substr(b, e - b + 1);
}

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

inline std::string render_number(const double value) {
  const double rounded = std::floor(value + 0.5);
  if (std::fabs(value - rounded) < 1e-9) {
    std::ostringstream ss;
    ss << static_cast<long long>(rounded);
    return ss.str();
  }
  std::ostringstream ss;
  ss.precision(6);
  ss << value;
  return ss.str();
}

// "5", "5s", "500ms", "5m", "1.5h" -> seconds. False for a suffix that is not a
// duration.
inline bool parse_duration(const std::string &value, double &seconds) {
  std::string::size_type i = 0;
  while (i < value.size() && !is_alpha(value[i]) && value[i] != '%') ++i;
  const std::string number = value.substr(0, i);
  const std::string suffix = value.substr(i);
  const double n = std::strtod(number.c_str(), nullptr);
  if (suffix.empty() || suffix == "s") {
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

inline bool has_suffix(const std::string &value) { return !value.empty() && !is_digit(value[value.size() - 1]) && value[value.size() - 1] != '.'; }

// The value as the filter wants to read it for this keyword.
inline bool render_value(const spec &s, const std::string &value, std::string &out, std::string &error) {
  switch (s.value_unit) {
    case unit_percent:
      out = has_suffix(value) ? value : value + "%";
      return true;
    case unit_percent_number:
      out = value[value.size() - 1] == '%' ? value.substr(0, value.size() - 1) : value;
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

// The filter expression a -w / -c value stands for. One value per spec, comma
// separated, any of which may be left out ("-w ,20%"): the values are joined
// with "or", as the standard plugins do. Returns false and sets `error` when
// the value is a threshold that does not fit the check.
inline bool expand(const spec_list &specs, const std::string &value, std::string &expression, std::string &error) {
  const std::string trimmed = detail::trim(value);
  if (!is_threshold(trimmed)) {
    expression = trimmed;
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
  std::vector<std::string> expanded;
  for (std::string::size_type i = 0; i < parts.size(); ++i) {
    if (parts[i].empty()) continue;
    std::string one;
    if (!detail::expand_range(specs[i], parts[i], one, error)) return false;
    expanded.push_back(one);
  }
  if (expanded.size() == 1) {
    expression = expanded[0];
    return true;
  }
  expression = "";
  for (const std::string &e : expanded) expression += (expression.empty() ? "(" : " or (") + e + ")";
  return true;
}

// ---------------------------------------------------------------------------
// What a short threshold replaces.
//
// A default is often more than a bound: check_http's critical is
// "code < 200 or code >= 400 or result != 'ok'", check_tcp's is
// "time > 5000 or result != 'ok'". Somebody writing "-c 5" is moving the time
// bound, not asking for a failed request to stop being critical. So of a
// default only the alternatives that bound the keyword are dropped, and the
// rest is kept next to the new threshold.
// ---------------------------------------------------------------------------
namespace detail {

inline char lower(const char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }
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

// Whether the text compares the keyword with < or > (<=, >= included):
// "time > 5000" does, "result != 'ok'" and "checkdb_age = -1" do not.
inline bool bounds_variable(const std::string &text, const std::string &variable) {
  std::string::size_type pos = 0;
  while ((pos = text.find(variable, pos)) != std::string::npos) {
    const std::string::size_type end = pos + variable.size();
    const bool whole = (pos == 0 || !is_word(text[pos - 1])) && (end >= text.size() || !is_word(text[end]));
    if (whole) {
      std::string::size_type i = end;
      while (i < text.size() && (text[i] == ' ' || text[i] == '\t')) ++i;
      if (i < text.size() && (text[i] == '<' || text[i] == '>')) return true;
    }
    pos = end;
  }
  return false;
}
}  // namespace detail

// The default with every alternative that bounds one of the keywords removed;
// empty when nothing is left of it.
inline std::string keep_unbounded(const std::string &expression, const std::vector<std::string> &variables) {
  std::string ret;
  for (const std::string &alternative : detail::split_alternatives(expression)) {
    if (alternative.empty()) continue;
    bool bounds = false;
    for (const std::string &variable : variables) {
      if (detail::bounds_variable(alternative, variable)) {
        bounds = true;
        break;
      }
    }
    if (bounds) continue;
    ret += (ret.empty() ? "" : " or ") + alternative;
  }
  return ret;
}

// How a bare number reads for the check, for the option's help text:
// "load > N", or "time > N or loss > N%" for a list.
inline std::string describe(const spec_list &specs) {
  std::string ret;
  for (const spec &s : specs) {
    if (!ret.empty()) ret += " or ";
    ret += s.variable + (s.dir == above ? " > N" : " < N");
    if (s.value_unit == unit_percent) ret += "%";
  }
  return ret;
}

// The unit a bare number is read in, for the help text.
inline std::string describe_unit(const spec &s) {
  switch (s.value_unit) {
    case unit_percent:
      return "A number without a unit is a percentage; a value with a unit (10G) is used as written.";
    case unit_seconds_as_ms:
    case unit_seconds:
      return "A number without a unit is in seconds; ms, s, m, h, d and w are accepted as units (500ms, 5m).";
    case unit_plain:
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
// runs: "w=80" among key=value arguments, "--w 80" among dashed ones.
// ---------------------------------------------------------------------------

inline bool is_short_flag(const std::string &arg, char &flag, std::string &attached) {
  if (arg.size() < 2 || arg[0] != '-' || (arg[1] != 'w' && arg[1] != 'c')) return false;
  if (arg.size() == 2) {
    flag = arg[1];
    attached = "";
    return true;
  }
  // "-w80", "-w=80", "-w10:", "-w@10:20", "-w~:10", "-w.5". Anything else that
  // merely starts with -w or -c is somebody else's argument.
  const char next = arg[2];
  if (next == '=') {
    flag = arg[1];
    attached = arg.substr(3);
    return true;
  }
  if (detail::is_digit(next) || next == '@' || next == '~' || next == '.') {
    flag = arg[1];
    attached = arg.substr(2);
    return true;
  }
  return false;
}

inline std::vector<std::string> normalize_arguments(const std::vector<std::string> &args) {
  // The shape of the line is that of its first argument that is not a short
  // threshold (nor the value of one).
  bool dashed = false;
  for (std::string::size_type i = 0; i < args.size(); ++i) {
    char flag;
    std::string attached;
    if (is_short_flag(args[i], flag, attached)) {
      if (args[i].size() == 2) ++i;
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
    if (args[i].size() == 2) {
      // A trailing "-w" has no value: leave it for the parser to complain about.
      if (i + 1 >= args.size()) {
        ret.push_back(args[i]);
        continue;
      }
      value = args[++i];
    }
    if (dashed) {
      ret.push_back(std::string("--") + flag);
      ret.push_back(value);
    } else {
      ret.push_back(std::string(1, flag) + "=" + value);
    }
  }
  return ret;
}

}  // namespace shorthand
}  // namespace modern_filter
