// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#ifdef WIN32
#pragma warning(push)
#pragma warning(disable : 4459)
#endif
#include <boost/spirit/include/qi.hpp>
#ifdef WIN32
#pragma warning(pop)
#endif
#include <boost/function.hpp>
#include <parsers/where/node.hpp>

namespace qi = boost::spirit::qi;

namespace charset = boost::spirit::standard;

namespace parsers {
namespace where {
struct where_grammar : qi::grammar<std::string::const_iterator, node_type(), charset::space_type> {
  typedef std::string::const_iterator iterator_type;
  where_grammar(object_factory obj_factory);

  qi::rule<iterator_type, node_type(), charset::space_type> expression, and_expr, not_expr, cond_expr, identifier_expr, identifier, list_expr;
  qi::rule<iterator_type, std::string(), charset::space_type> string_literal, variable_name, string_literal_ex;
  // The unit after a number: a run of letters ("5m", "500ms", "10GB") or "%".
  qi::rule<iterator_type, std::string()> unit_suffix;
  qi::rule<iterator_type, operators(), charset::space_type> op, bitop;
  // The word keywords. Each matches only as a whole word (case-insensitive,
  // not followed by a letter, digit or '_'), so a variable whose name starts
  // with one - notice, order, android, index, next, likes - stays a variable.
  // No skipper: an implied lexeme, pre-skipped by the calling rule.
  qi::rule<iterator_type> word_end, kw_not, kw_and, kw_or, kw_in, kw_like, kw_regexp, kw_le, kw_lt, kw_eq, kw_ne, kw_ge, kw_gt;
};
}  // namespace where
}  // namespace parsers