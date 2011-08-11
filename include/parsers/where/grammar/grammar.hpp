#pragma once

#include <list>
#include <iostream> 
#include <sstream>

#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/include/phoenix_object.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/io.hpp>
#include <boost/function.hpp>

#include <strEx.h>

#include <parsers/where/expression_ast.hpp>

namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;

namespace parsers {
	namespace where {

		struct where_grammar : qi::grammar<std::wstring::const_iterator, expression_ast(), ascii::space_type> {
			typedef std::wstring::const_iterator iterator_type;
			where_grammar();
			
			qi::rule<iterator_type, expression_ast(), ascii::space_type>  expression, and_expr, not_expr, cond_expr, identifier_expr, identifier, list_expr;
			qi::rule<iterator_type, std::wstring(), ascii::space_type> string_literal, variable_name, string_literal_ex;
			qi::rule<iterator_type, unsigned int(), ascii::space_type> number;
			qi::rule<iterator_type, operators(), ascii::space_type> op, bitop;
			qi::rule<iterator_type, list_value(), ascii::space_type> value_list;
		};
	}
}


