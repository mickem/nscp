// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <cctype>
#include <cmath>
#include <locale>
#include <list>
#include <parsers/where.hpp>
#include <str/format.hpp>
#include <str/xtos.hpp>
#include <sstream>
#include <stdexcept>
#include <string>

namespace duration_keyword {

// True for an optionally-signed run of digits ("0", "-1", "+259200").
inline bool is_plain_integer(const std::string &expr) {
  std::size_t i = 0;
  if (i < expr.size() && (expr[i] == '-' || expr[i] == '+')) ++i;
  if (i >= expr.size()) return false;
  for (; i < expr.size(); ++i) {
    if (!std::isdigit(static_cast<unsigned char>(expr[i]))) return false;
  }
  return true;
}

// Seconds in one duration unit (ms, s, m, h, d, w), 0 for anything else.
inline double seconds_per_unit(const std::string &unit) { return str::format::duration_unit_seconds(unit); }

namespace detail {
// The number at the front of `text` and the unit after it: an optional sign,
// digits and an optional fraction ("2", "-1", "2.5", ".5"). False when there
// is no number. Only that prefix is handed to the stream, in the classic
// locale: a stream reading a double greedily can take a unit letter for part
// of the number (libc++ does with "2d", "2e" and "2b"), and a threshold is
// written with a '.' whatever the host's decimal separator is.
inline bool split_number(const std::string &text, double &number, std::string &unit) {
  std::string::size_type i = 0;
  if (i < text.size() && (text[i] == '-' || text[i] == '+')) ++i;
  std::string::size_type digits = 0;
  while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i]))) ++i, ++digits;
  if (i < text.size() && text[i] == '.') {
    ++i;
    while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i]))) ++i, ++digits;
  }
  if (digits == 0) return false;
  std::istringstream in(text.substr(0, i));
  in.imbue(std::locale::classic());
  if (!(in >> number)) return false;
  unit = text.substr(i);
  return true;
}

// `value` as the keyword's integer, or an error when it does not fit one: a
// threshold such as `age > 99999999999999999w` must not wrap round to the
// smallest number there is and fire on every run.
inline parsers::where::node_type to_int(parsers::where::evaluation_context context, const double value, const std::string &text) {
  try {
    return parsers::where::factory::create_int(str::format::llround_checked(value, "Duration " + text));
  } catch (const std::out_of_range &e) {
    context->error(e.what());
    return parsers::where::factory::create_int(0);
  }
}

// The value a duration literal amounts to in the keyword's own unit, which is
// `per_second` of them to the second; a bare number is already in it. The
// keyword holds an integer, so a fraction is rounded to the nearest whole
// ("1.5ms" is 2). A bare fraction in an expression never gets here: it is
// compared as written. A plain integer is passed straight through, so the sentinels
// keywords document ("age = -1") keep working. A unit that is not a duration,
// or a value too large to hold, is reported on the context, which ends the
// check UNKNOWN, rather than read as something else.
inline parsers::where::node_type parse(parsers::where::evaluation_context context, parsers::where::node_type subject, const double per_second,
                                      const double smallest_unit = 0.0) {
  // The parser hands over either one literal ("30m", "-1") or a two element
  // list [number, unit] for tokenized inputs like "2d"; list_node::get_value
  // would join the latter as "2, d" (#452 / #589), so take it apart by hand.
  const std::list<parsers::where::node_type> tokens = subject->get_list_value(context);
  double count = 0;
  std::string unit;
  std::string text;
  if (tokens.size() == 2) {
    auto token = tokens.begin();
    count = (*token)->get_float_value(context);
    ++token;
    unit = (*token)->get_value(context, parsers::where::type_string).get_string("");
    text = str::xtos(count) + unit;
  } else {
    text = subject->get_string_value(context);
    if (is_plain_integer(text)) {
      try {
        return parsers::where::factory::create_int(std::stoll(text));
      } catch (const std::out_of_range &) {
        context->error("Duration " + text + " is too large");
        return parsers::where::factory::create_int(0);
      }
    }
    if (!split_number(text, count, unit)) {
      context->error("Not a duration: " + text);
      return parsers::where::factory::create_int(0);
    }
  }
  if (unit.empty()) return to_int(context, count, text);
  const double seconds = seconds_per_unit(unit);
  if (seconds == 0.0) {
    context->error("Invalid unit: " + unit + " (expected one of ms, s, m, h, d, w)");
    return parsers::where::factory::create_int(0);
  }
  if (seconds < smallest_unit) {
    context->error("Invalid unit: " + unit + " (the keyword counts days: expected d or w)");
    return parsers::where::factory::create_int(0);
  }
  return to_int(context, count * seconds * per_second, text);
}
}  // namespace detail

// Converter for keywords that hold a number of seconds, so a threshold can be
// written the way an operator thinks about it - `age > 30m`, `uptime < 2d` -
// instead of in raw seconds. Register it alongside a custom int type:
//
//   static const value_type type_custom_age = type_custom_int_1;
//   registry_.add_int_var("age", type_custom_age, &obj::get_age, "...");
//   registry_.add_converter(type_custom_age, &duration_keyword::parse_duration<obj_ptr>);
//
// Without the converter a plain type_int keyword cannot take "30m" at all: the
// unit is refused and the check ends UNKNOWN.
template <class TObject>
parsers::where::node_type parse_duration(TObject /*object*/, parsers::where::evaluation_context context, parsers::where::node_type subject) {
  return detail::parse(context, subject, 1.0);
}

// The type a millisecond keyword is registered with, next to its converter:
//
//   registry_.add_int_var("time", duration_keyword::type_milliseconds, &obj::get_time, "...");
//   registry_.add_converter(duration_keyword::type_milliseconds, &duration_keyword::parse_duration_ms<obj_ptr>);
//
// One custom slot for all of them: a registry holds one converter per type,
// and every millisecond keyword wants the same one.
static const parsers::where::value_type type_milliseconds = parsers::where::type_custom_int_10;

// The same for keywords that hold a number of milliseconds (a round trip, a
// request time): a bare number stays milliseconds, so `time > 200` keeps its
// meaning, and `time > 1s` is `time > 1000`.
template <class TObject>
parsers::where::node_type parse_duration_ms(TObject /*object*/, parsers::where::evaluation_context context, parsers::where::node_type subject) {
  return detail::parse(context, subject, 1000.0);
}

// The same for keywords that hold whole days (expires_in, signature_age): a
// bare number stays days, `expires_in < 2w` is `expires_in < 14`, and a unit
// below a day - a fraction the keyword never holds - is refused.
template <class TObject>
parsers::where::node_type parse_duration_days(TObject /*object*/, parsers::where::evaluation_context context, parsers::where::node_type subject) {
  return detail::parse(context, subject, 1.0 / 86400.0, 86400.0);
}

// The type a days keyword is registered with, next to its converter.
static const parsers::where::value_type type_days = parsers::where::type_custom_int_9;

}  // namespace duration_keyword
