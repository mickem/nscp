// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <cctype>
#include <cmath>
#include <list>
#include <parsers/where.hpp>
#include <str/format.hpp>
#include <str/xtos.hpp>
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

// Converter for keywords that hold a number of seconds, so a threshold can be
// written the way an operator thinks about it - `age > 30m`, `uptime < 2d` -
// instead of in raw seconds. Register it alongside a custom int type:
//
//   static const value_type type_custom_age = type_custom_int_1;
//   registry_.add_int_var("age", type_custom_age, &obj::get_age, "...");
//   registry_.add_converter(type_custom_age, &duration_keyword::parse_duration<obj_ptr>);
//
// Without the converter a plain type_int keyword silently reads "30m" as 30,
// which compares true almost immediately and looks like a working threshold.
template <class TObject>
parsers::where::node_type parse_duration(TObject /*object*/, parsers::where::evaluation_context context, parsers::where::node_type subject) {
  // The parser hands over either one string literal ("30m") or a two element
  // list [number, unit] for tokenized inputs like "2d"; list_node::get_value
  // would join the latter as "2, d" and fail to validate (#452 / #589), so
  // reassemble it by hand.
  const std::list<parsers::where::node_type> tokens = subject->get_list_value(context);
  std::string expression;
  if (tokens.size() == 2) {
    auto token = tokens.begin();
    const long long count = (*token)->get_int_value(context);
    const double count_f = (*token)->get_float_value(context);
    ++token;
    const std::string unit = (*token)->get_value(context, parsers::where::type_string).get_string("");
    // A fractional count ("2.5h") cannot survive the string round-trip: the
    // int accessor already truncated it to 2. Scale it numerically instead.
    if (count_f != static_cast<double>(count)) {
      return parsers::where::factory::create_int(llround(count_f * str::format::time_unit_multiplier(unit)));
    }
    expression = str::xtos(count) + unit;
  } else {
    expression = subject->get_string_value(context);
  }
  // A plain number keeps meaning seconds - and must not go through the time
  // parser: validate_time_spec rejects a sign, filter_converter turns the
  // exception into create_false(), and "age = -1" (the documented empty-queue
  // sentinel) would silently never fire.
  if (is_plain_integer(expression)) return parsers::where::factory::create_int(str::stox<long long>(expression, 0));
  return parsers::where::factory::create_int(str::format::stox_as_time_sec<long long>(expression, "s"));
}

}  // namespace duration_keyword
