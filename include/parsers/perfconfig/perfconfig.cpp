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

#include <parsers/perfconfig/perfconfig.hpp>
#include <boost/config/warning_disable.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/include/phoenix_stl.hpp>
#include <boost/spirit/include/phoenix_object.hpp>
#include <boost/spirit/include/phoenix_fusion.hpp>
#include <boost/fusion/include/adapt_struct.hpp>

namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;
namespace phoenix = boost::phoenix;

typedef parsers::perfconfig::perf_rule perf_rule;
typedef parsers::perfconfig::perf_option perf_option;

BOOST_FUSION_ADAPT_STRUCT(
	perf_option,
	(std::string, key)
	(std::string, value)
	)

	BOOST_FUSION_ADAPT_STRUCT(
		perf_rule,
		(std::string, name)
		(std::vector<perf_option>, options)
		)

struct spirit_perfconfig_parser {
	template<class Iterator>
	bool parse_raw(Iterator first, Iterator last, parsers::perfconfig::result_type& v) {
		using qi::lexeme;
		using qi::_1;
		using qi::_2;
		using qi::_val;
		using phoenix::at_c;

		qi::rule<Iterator, std::vector<perf_rule>(), ascii::space_type> rules;
		qi::rule<Iterator, perf_rule(), ascii::space_type> rule;
		qi::rule<Iterator, std::vector<perf_option>(), ascii::space_type> options;
		qi::rule<Iterator, perf_option(), ascii::space_type> option;
		qi::rule<Iterator, std::string(), ascii::space_type> op_key;
		qi::rule<Iterator, std::string(), ascii::space_type> op_value;
		qi::rule<Iterator, std::string(), ascii::space_type> keyword;
#if BOOST_VERSION >= 104900
		qi::rule<Iterator, std::string(), ascii::space_type> valid_keyword;
		qi::rule<Iterator, std::string(), ascii::space_type> valid_value;
#else
#if BOOST_VERSION >= 104200
		qi::rule<Iterator, std::string()> valid_keyword, valid_keyword_1, valid_keyword_2;
#else
		qi::rule<Iterator, std::string(), ascii::space_type> valid_keyword;
#endif
#endif

		rules %= *rule;
		rule %= keyword >> "(" >> options >> ")";
		options = *(option >> ";") >> option;
		option = op_key[at_c<0>(_val) = _1]
			>> ":" >> op_value[at_c<1>(_val) = _1]
			| op_key[at_c<0>(_val) = _1];
		keyword %= valid_keyword;

#if BOOST_VERSION >= 104900
		op_key %= valid_keyword;
		op_value = qi::lexeme['\''
			>> +(ascii::char_ - '\'')[_val += _1]
			>> '\'']
			| "''"
			| valid_keyword[_val = _1];
		//valid_value		%= lexeme[+(qi::char_("-_a-zA-Z0-9*+%")) >> *(qi::hold[+(qi::char_(' ')) >> +(qi::char_("-_a-zA-Z0-9+%"))])];
		valid_keyword %= lexeme[+(qi::char_("-_a-zA-Z0-9*+%'.")) >> *(qi::hold[+(qi::char_(' ')) >> +(qi::char_("-_a-zA-Z0-9+%'."))])];
#else
		op_key %= valid_keyword;
		op_value %= valid_keyword;
#if BOOST_VERSION >= 104200
		// THis works with boost prior to 1.49 but has some issues (see removed test simple_space_5 and simple_space_6)
		valid_keyword %= valid_keyword_1 >> *valid_keyword_2[_val += _1];
		valid_keyword_1 %= +qi::char_("-_a-zA-Z0-9*+%");
		valid_keyword_2 %= qi::hold[+qi::char_(' ') >> +qi::char_("-_a-zA-Z0-9+%")];
#else
		// THis works with boost prior to 1.42 but has some issues (see removed test ...)
		valid_keyword %= *qi::char_("-_a-zA-Z0-9*+%")[_val += _1];
		//		valid_keyword_1 %= +qi::char_("-_a-zA-Z0-9*+%");
		//		valid_keyword_2 %= +qi::char_("-_a-zA-Z0-9+%");
#endif
#endif

		return qi::phrase_parse(first, last, rules, ascii::space, v);
	}
};

bool parsers::perfconfig::parse(const std::string &str, result_type& v) {
	spirit_perfconfig_parser parser;
	return parser.parse_raw(str.begin(), str.end(), v);
}