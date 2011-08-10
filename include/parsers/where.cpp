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

#include <parsers/where.hpp>
#include <parsers/where/expression_ast.hpp>
#include <parsers/where/ast_type_inference.hpp>
#include <parsers/where/ast_static_eval.hpp>
#include <parsers/where/ast_bind.hpp>

#include <parsers/helpers.hpp>
#include <parsers/where/grammar/grammar.hpp>

namespace parsers {
	namespace where {

		bool parser::parse(std::wstring expr) {
			constants::reset();
			//std::wcout << _T("Current time is: ") << constants::get_now() << std::endl;
			typedef where_grammar grammar;

			grammar calc; // Our grammar

			grammar::iterator_type iter = expr.begin();
			grammar::iterator_type end = expr.end();
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

		bool parser::derive_types(filter_handler handler) {
			try {
				ast_type_inference resolver(handler);
				resolver(resulting_tree);
				return true;
			} catch (...) {
				handler->error(_T("Unhandled exception resolving types: ") + result_as_tree());
				return false;
			}
		}

		bool parser::static_eval(filter_handler handler) {
			try {
				ast_static_eval evaluator(handler);
				evaluator(resulting_tree);
				return true;
			} catch (...) {
				handler->error(_T("Unhandled exception static eval: ") + result_as_tree());
				return false;
			}
		}
		bool parser::bind(filter_handler handler) {
			try {
				ast_bind binder(handler);
				binder(resulting_tree);
				return true;
			} catch (...) {
				handler->error(_T("Unhandled exception static eval: ") + result_as_tree());
				return false;
			}
		}

		bool parser::evaluate(filter_handler handler) {
			try {
				expression_ast ast = resulting_tree.evaluate(handler);
				return ast.get_int(handler) == 1;
			} catch (...) {
				handler->error(_T("Unhandled exception static eval: ") + result_as_tree());
				return false;
			}
		}

		std::wstring parser::result_as_tree() const {
			return resulting_tree.to_string();
		}
	}
}


