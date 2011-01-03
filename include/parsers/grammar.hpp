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

#include <parsers/ast.hpp>

namespace parsers {
	namespace where {

		template <typename THandler, typename Iterator>
		struct where_grammar : qi::grammar<Iterator, expression_ast<THandler>(), ascii::space_type> {
			where_grammar();
			
			qi::rule<Iterator, expression_ast<THandler>(), ascii::space_type>  expression, and_expr, not_expr, cond_expr, identifier, list_expr;
			qi::rule<Iterator, std::wstring(), ascii::space_type> string_literal, variable_name, string_literal_ex;
			qi::rule<Iterator, unsigned int(), ascii::space_type> number;
			qi::rule<Iterator, operators(), ascii::space_type> op;
			qi::rule<Iterator, list_value<THandler>(), ascii::space_type> value_list;
		};
	}
}


