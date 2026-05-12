/*
 * Copyright (C) 2004-2026 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <parsers/expression/expression.hpp>
#include <string>

namespace {

// Find the position of the matching ')' for a '%(' placeholder body. Tries a
// balanced-parens scan first (counting nested `(...)` pairs and skipping over
// single-quoted strings) so that placeholders like `%(format_bytes(value,
// 'MB'))` resolve to the OUTER ')' — i.e. the body includes the inner call.
// Falls back to the legacy "first ')'" rule when no balanced match exists, so
// historical content such as `%(test((2)` (which the old Spirit grammar
// happily matched as the variable name `test((2`) keeps the same shape.
//
// Returns the position of the chosen ')' in str, or std::string::npos when
// neither pass finds a closer (truly unterminated input).
std::size_t find_placeholder_close(const std::string &str, std::size_t body_start) {
  const std::size_t n = str.size();
  // Pass 1: balanced parens with quoted-string skipping.
  {
    std::size_t j = body_start;
    int depth = 1;
    while (j < n && depth > 0) {
      const char c = str[j];
      if (c == '\'') {
        // Step over a single-quoted string literal so a ')' inside it cannot
        // close the placeholder. No escape handling is needed here — the
        // where-grammar itself doesn't support escapes inside string
        // literals either.
        ++j;
        while (j < n && str[j] != '\'') ++j;
        if (j < n) ++j;  // skip closing '
        continue;
      }
      if (c == '(') {
        ++depth;
      } else if (c == ')') {
        if (--depth == 0) return j;
      }
      ++j;
    }
  }
  // Pass 2: legacy first ')' for inputs the balanced scan couldn't match
  // (e.g. unbalanced parens that the old grammar still happily captured).
  {
    std::size_t j = body_start;
    while (j < n && str[j] != ')') ++j;
    return (j < n) ? j : std::string::npos;
  }
}

}  // namespace

bool parsers::simple_expression::parse(const std::string &str, result_type &v) {
  std::string literal;
  auto flush_literal = [&]() {
    if (!literal.empty()) {
      v.emplace_back(false, literal);
      literal.clear();
    }
  };

  const std::size_t n = str.size();
  std::size_t i = 0;
  while (i < n) {
    // ${name} placeholder — content runs to the first '}'. No nesting; that
    // matches the historical Spirit grammar (`+(char_ - '}')`) and is what
    // existing config files rely on.
    if (i + 1 < n && str[i] == '$' && str[i + 1] == '{') {
      const std::size_t body_start = i + 2;
      std::size_t j = body_start;
      while (j < n && str[j] != '}') ++j;
      if (j < n && j > body_start) {
        flush_literal();
        v.emplace_back(true, str.substr(body_start, j - body_start));
        i = j + 1;
        continue;
      }
      // Unterminated or empty body — emit '$' as a single-char literal entry
      // (matching the old fallback_rule behaviour) and resume at the next
      // character so '{' falls into the next plain-text run.
      flush_literal();
      v.emplace_back(false, std::string(1, str[i]));
      ++i;
      continue;
    }
    // %(expr) placeholder — content runs to a balanced ')' (issue #281), with
    // the legacy first-')' fallback retained for backward compatibility.
    if (i + 1 < n && str[i] == '%' && str[i + 1] == '(') {
      const std::size_t body_start = i + 2;
      const std::size_t close = find_placeholder_close(str, body_start);
      if (close != std::string::npos && close > body_start) {
        flush_literal();
        v.emplace_back(true, str.substr(body_start, close - body_start));
        i = close + 1;
        continue;
      }
      // No closer found (or empty body) — emit '%' as a literal entry, same
      // as the historical fallback path, and resume.
      flush_literal();
      v.emplace_back(false, std::string(1, str[i]));
      ++i;
      continue;
    }
    // Plain literal character.
    literal.push_back(str[i]);
    ++i;
  }
  flush_literal();
  return true;
}
