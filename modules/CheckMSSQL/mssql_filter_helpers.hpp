// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <list>
#include <parsers/where/helpers.hpp>
#include <str/format.hpp>
#include <str/xtos.hpp>
#include <string>

namespace mssql_filter {

// Duration-literal converter for age/uptime keywords (register with
// add_converter on a type_custom_int_* keyword): turns "30m" / "2d" — or the
// tokenized [number, unit] list form — into seconds, so expressions like
// full_age > 7d work. Same shape as check_uptime_filter::parse_time in
// CheckSystem/filter.cpp.
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
  return factory::create_int(str::format::stox_as_time_sec<long long>(expr, "s"));
}

}  // namespace mssql_filter
