// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <cctype>
#include <check/duration_keyword.hpp>
#include <list>
#include <parsers/where/helpers.hpp>
#include <str/format.hpp>
#include <str/xtos.hpp>
#include <string>

namespace mysql_filter {

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

// Duration-literal converter for the uptime keyword (register with
// add_converter on a type_custom_int_* keyword): "30m", "2d", "500ms" to
// seconds, a plain integer passed straight through. The shared one, so the
// unit is read whole and one it does not know is reported.
template <class TObject>
parsers::where::node_type parse_time(TObject object, parsers::where::evaluation_context context, parsers::where::node_type subject) {
  return duration_keyword::parse_duration<TObject>(object, context, subject);
}

}  // namespace mysql_filter
