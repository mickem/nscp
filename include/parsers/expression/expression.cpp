/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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