/*
 * Copyright (C) 2004-2016 Michael Medin
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

#include <boost/config/warning_disable.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/include/phoenix_stl.hpp>
#include <boost/spirit/include/phoenix_object.hpp>

namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;
namespace phoenix = boost::phoenix;

typedef parsers::simple_expression::entry entry;
struct spirit_expression_parser {
	template<class Iterator>
	bool parse_raw(Iterator first, Iterator last, parsers::simple_expression::result_type& v) {
		using qi::_1;
		using qi::_3;
		using qi::_val;
		using phoenix::push_back;
		using qi::lexeme;
		qi::rule<Iterator, entry()> normal_rule;
		qi::rule<Iterator, entry()> variable_rule_d;
		qi::rule<Iterator, entry()> variable_rule_p;
		normal_rule = lexeme[+(qi::char_ - "${" - "%(")][_val = phoenix::construct<entry>(false, _1)];
		variable_rule_d = ("${" >> lexeme[+(qi::char_ - '}')] >> "}")[_val = phoenix::construct<entry>(true, _1)];
		variable_rule_p = ("%(" >> lexeme[+(qi::char_ - ')')] >> ")")[_val = phoenix::construct<entry>(true, _1)];
		return qi::parse(first, last,
			*(
				normal_rule[push_back(phoenix::ref(v), _1)]
				||
				variable_rule_d[push_back(phoenix::ref(v), _1)]
				||
				variable_rule_p[push_back(phoenix::ref(v), _1)]
				)
			);
	}
};

bool parsers::simple_expression::parse(const std::string &str, result_type& v) {
	spirit_expression_parser parser;
	return parser.parse_raw(str.begin(), str.end(), v);
}