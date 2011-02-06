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
#include <parsers/where/operators_impl.hpp>
#include <parsers/where/varible_handler.hpp>
#include <parsers/helpers.hpp>
#include <parsers/where/grammar/grammar.hpp>
#include <parsers/where/ast_type_inference.hpp>
#include <parsers/where/ast_static_eval.hpp>
#include <parsers/where/ast_bind.hpp>
#include <parsers/where/list_value.hpp>
#include <parsers/where/unary_fun.hpp>
#include <parsers/where/operators.hpp>
#include <parsers/where/variable.hpp>
#include <parsers/where/ast_bind.hpp>
#include <parsers/where/ast_visitors.hpp>
#include <parsers/where/expression_ast_impl.hpp>
#include <parsers/where/operators_impl.hpp>
#include <parsers/where/grammar/grammar_impl.hpp>
//#include <parsers/eval.hpp>

namespace parsers {
	namespace where {

		template<typename THandler>
		struct factory {
			typedef boost::shared_ptr<binary_operator_impl<typename THandler> > bin_op_type;
			typedef boost::shared_ptr<binary_function_impl<typename THandler> > bin_fun_type;
			typedef boost::shared_ptr<unary_operator_impl<typename THandler> > un_op_type;

			static bin_op_type get_binary_operator(operators op, const expression_ast<THandler> &left, const expression_ast<THandler> &right);
			static bin_fun_type get_binary_function(std::wstring name, const expression_ast<THandler> &subject);
			static un_op_type get_unary_operator(operators op);
		};


		template<typename THandler>
		struct parser {
			typedef typename THandler::object_type object_type;
			expression_ast<THandler> resulting_tree;
			std::wstring rest;
			bool parse(std::wstring expr);
			bool derive_types(THandler & handler);
			bool static_eval(THandler & handler);
			bool bind(THandler & handler);
			bool evaluate(object_type & object);
			std::wstring result_as_tree() const;
		};


		template<typename THandler>
		bool parser<THandler>::parse(std::wstring expr) {
			constants::reset();
			//std::wcout << _T("Current time is: ") << constants::get_now() << std::endl;
			typedef std::wstring::const_iterator iterator_type;
			typedef where_grammar<THandler, iterator_type> grammar;

			grammar calc; // Our grammar

			iterator_type iter = expr.begin();
			iterator_type end = expr.end();
			if (phrase_parse(iter, end, calc, ascii::space, resulting_tree)) {
				rest = std::wstring(iter, end);
				return rest.empty();
				//std::wcout<< _T("Rest: ") << rest << std::endl;
				//return true;
			}
			rest = std::wstring(iter, end);
			//std::wcout << _T("Rest: ") << rest << std::endl;
			return false;
		}

		template<typename THandler>
		bool parser<THandler>::derive_types(THandler & handler) {
			try {
				ast_type_inference<THandler> resolver(handler);
				resolver(resulting_tree);
				return true;
			} catch (...) {
				handler.error(_T("Unhandled exception resolving types: ") + result_as_tree());
				return false;
			}
		}

		template<typename THandler>
		bool parser<THandler>::static_eval(THandler & handler) {
			try {
				ast_static_eval<THandler> evaluator(handler);
				evaluator(resulting_tree);
				return true;
			} catch (...) {
				handler.error(_T("Unhandled exception static eval: ") + result_as_tree());
				return false;
			}
		}
		template<typename THandler>
		bool parser<THandler>::bind(THandler & handler) {
			try {
				ast_bind<THandler> binder(handler);
				binder(resulting_tree);
				return true;
			} catch (...) {
				handler.error(_T("Unhandled exception static eval: ") + result_as_tree());
				return false;
			}
		}

		template<typename THandler>
		bool parser<THandler>::evaluate(object_type & object) {
			try {
				expression_ast<THandler> ast = resulting_tree.evaluate(object);
				return ast.get_int(object) == 1;
			} catch (...) {
				object.error(_T("Unhandled exception static eval: ") + result_as_tree());
				return false;
			}
		}

		template<typename THandler>
		std::wstring parser<THandler>::result_as_tree() const {
			return resulting_tree.to_string();
		}
	}
}


