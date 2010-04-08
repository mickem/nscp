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

		template<typename THandler>
		struct factory {
			typedef boost::shared_ptr<binary_operator_impl<typename THandler> > bin_op_type;
			typedef boost::shared_ptr<binary_function_impl<typename THandler> > bin_fun_type;
			typedef boost::shared_ptr<unary_operator_impl<typename THandler> > un_op_type;

			static bin_op_type get_binary_operator(operators op);
			//static varible_handler::bound_function_type get_binary_function(std::wstring name, const expression_ast<THandler> &subject);
			static bin_fun_type get_binary_function(std::wstring name, const expression_ast<THandler> &subject);
			static un_op_type get_unary_operator(operators op);
		};


		template<typename THandler>
		struct parser {
			expression_ast<THandler> resulting_tree;
			std::wstring rest;
			bool parse(std::wstring expr);
			bool derive_types(THandler & handler);
			bool static_eval(THandler & handler);
			bool bind(THandler & handler);
			bool evaluate(THandler & handler);
			std::wstring result_as_tree() const;
		};
	}
}


