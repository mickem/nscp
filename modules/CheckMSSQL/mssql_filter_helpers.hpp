// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <cctype>
#include <list>
#include <parsers/where/helpers.hpp>
#include <str/format.hpp>
#include <str/xtos.hpp>
#include <string>

namespace mssql_filter {

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

// Duration-literal converter for age/uptime keywords (register with
// add_converter on a type_custom_int_* keyword): turns "30m" / "2d" — or the
// tokenized [number, unit] list form — into seconds, so expressions like
// full_age > 7d work. Same shape as check_uptime_filter::parse_time in
// CheckSystem/filter.cpp.
//
// Registering a converter for the keyword's type makes the where-parser route
// *every* literal compared against it through here, including plain integers.
// str::format::stox_as_time_sec rejects signs outright (validate_time_spec,
// issue #589), and filter_converter::evaluate turns the resulting exception
// into create_false() == int_value(0) — so "full_age = -1" would silently
// become "full_age = 0" and never fire, defeating the -1 = never sentinel that
// every age keyword documents. Pass plain integers straight through instead.
// Multiplier semantics of the built-in size literals (parsers/operators.cpp
// parse_size): single letter, case-insensitive, 1024-based, b = bytes. The
// string-splitting overload of decode_byte_units cannot be used instead of this
// whole converter: it does not handle the leading sign the -1 sentinel needs.
inline long long apply_size_unit(const long long value, const std::string &unit) { return str::format::decode_byte_units<long long>(value, unit); }

// Size-literal converter for byte keywords that carry a negative sentinel
// (-1 = unknown), registered like parse_time on a type_custom_int_* keyword.
// The built-in type_size cannot compare against plain integers at all
// (can_convert(type_size, type_int) is false), so `x >= 0` fails to parse and
// the -1 sentinel is inexpressible - worse, `x < 1G` silently matches -1.
// This converter accepts both plain integers (including negatives) and the
// usual single-letter size suffixes (5G, 200M).
template <class TObject>
parsers::where::node_type parse_size(TObject object, parsers::where::evaluation_context context, parsers::where::node_type subject) {
  using namespace parsers::where;
  std::list<node_type> tokens = subject->get_list_value(context);
  std::string expr;
  if (tokens.size() == 2) {
    auto cit = tokens.begin();
    const long long n = (*cit)->get_int_value(context);
    ++cit;
    const std::string unit = (*cit)->get_value(context, type_string).get_string("");
    return factory::create_int(apply_size_unit(n, unit));
  }
  expr = subject->get_string_value(context);
  if (is_plain_integer(expr)) return factory::create_int(str::stox<long long>(expr, 0));
  if (expr.size() > 1) {
    const std::string number = expr.substr(0, expr.size() - 1);
    if (is_plain_integer(number)) return factory::create_int(apply_size_unit(str::stox<long long>(number, 0), expr.substr(expr.size() - 1)));
  }
  return factory::create_int(str::stox<long long>(expr, 0));
}

template <class TObject>
parsers::where::node_type parse_time(TObject object, parsers::where::evaluation_context context, parsers::where::node_type subject) {
  using namespace parsers::where;
  std::list<node_type> tokens = subject->get_list_value(context);
  std::string expr;
  if (tokens.size() == 2) {
    auto cit = tokens.begin();
    const long long n = (*cit)->get_int_value(context);
    ++cit;
    const std::string unit = (*cit)->get_value(context, type_string).get_string("");
    expr = str::xtos(n) + unit;
  } else {
    expr = subject->get_string_value(context);
  }
  if (is_plain_integer(expr)) return factory::create_int(str::stox<long long>(expr, 0));
  return factory::create_int(str::format::stox_as_time_sec<long long>(expr, "s"));
}

}  // namespace mssql_filter
