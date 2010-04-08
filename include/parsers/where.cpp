/*
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
*/
#include <strEx.h>

#include <parsers/ast.hpp>
#include <parsers/grammar.hpp>
#include <parsers/where.hpp>
#include <parsers/eval.hpp>

namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;


namespace parsers {
	namespace where {

		///////////////////////////////////////////////////////////////////////////
		//  Walk the tree
		///////////////////////////////////////////////////////////////////////////
		template<typename THandler>
		struct ast_type_inference {
			typedef value_type result_type;

			varible_type_handler & handler;
			ast_type_inference(varible_type_handler & handler) : handler(handler) {}

			value_type operator()(expression_ast<THandler> & ast) {
				value_type type = ast.get_type();
				if (type != type_tbd)
					return type;
				type = boost::apply_visitor(*this, ast.expr);
				ast.set_type(type);
				return type;
			}
			bool can_convert(value_type src, value_type dst) {
				if (src == type_invalid || dst == type_invalid)
					return false;
				if (src == type_tbd)
					return false;
				if (dst == type_tbd)
					return true;
				if (src == type_int && dst == type_string)
					return true;
				if (src == type_string && dst == type_int)
					return true;
				if (src >= type_custom_int && src < type_custom_int_end && dst == type_int)
					return true;
				if (src >= type_custom_string && src < type_custom_string_end && dst == type_int)
					return true;
				return false;
			}
			value_type infer_binary_type(expression_ast<THandler> & left, expression_ast<THandler> & right) {
				value_type rt = operator()(right);
				value_type lt = operator()(left);
				if (lt == rt)
					return lt;
				if (rt == type_invalid || lt == type_invalid)
					return type_invalid;
				if (rt == type_tbd && lt == type_tbd)
					return type_tbd;
				if (handler.can_convert(rt, lt)) {
					right.force_type(lt);
					return lt;
				}
				if (handler.can_convert(lt, rt)) {
					left.force_type(rt);
					return rt;
				}
				if (can_convert(rt, lt)) {
					right.force_type(lt);
					return rt;
				}
				if (can_convert(lt, rt)) {
					left.force_type(rt);
					return lt;
				}
				handler.error(_T("Invalid type detected for nodes: ") + left.to_string() + _T(" and " )+ right.to_string());
				return type_invalid;
			}

			value_type operator()(binary_op<THandler> & expr) {
				value_type type = infer_binary_type(expr.left, expr.right);
				if (type == type_invalid)
					return type;
				return get_return_type(expr.op, type);
			}
			value_type operator()(unary_op<THandler> & expr) {
				value_type type = operator()(expr.subject);
				return get_return_type(expr.op, type);
			}

			value_type operator()(unary_fun<THandler> & expr) {
				return type_tbd;
			}

			value_type operator()(list_value<THandler> & expr) {
				return type_tbd;
			}

			value_type operator()(string_value & expr) {
				return type_string;
			}
			value_type operator()(int_value & expr) {
				return type_int;
			}
			value_type operator()(variable<THandler> & expr) {
				if (!handler.has_variable(expr.name)) {
					handler.error(_T("Variable not found: ") + expr.name);
					return type_invalid;
				}
				return handler.get_type(expr.name);
			}

			value_type operator()(nil & expr) {
				handler.error(_T("NULL node encountered"));
				return type_invalid;
			}
		};
		///////////////////////////////////////////////////////////////////////////
		//  Walk the tree
		///////////////////////////////////////////////////////////////////////////
		template<typename THandler>
		struct ast_static_eval {
			typedef bool result_type;
			typedef std::list<std::wstring> error_type;

			THandler & handler;
			ast_static_eval(THandler & handler) : handler(handler) {}

			bool operator()(expression_ast<THandler> & ast) {
				bool result = boost::apply_visitor(*this, ast.expr);
				if (result) {
					if (ast.can_evaluate()) {
						ast.bind(handler);
						expression_ast<THandler> nexpr = ast.evaluate(handler);
						ast.expr = nexpr.expr;
					}
				}
				return result;
			}
			bool operator()(binary_op<THandler> & expr) {
				bool r1 = operator()(expr.left);
				bool r2 = operator()(expr.right);
				return r1 && r2;
			}
			bool operator()(unary_op<THandler> & expr) {
				return operator()(expr.subject);
			}

			bool operator()(unary_fun<THandler> & expr) {
				if ((expr.name == _T("convert")) || (expr.name == _T("auto_convert")) ) {
					return boost::apply_visitor(*this, expr.subject.expr);
				}
				return false;
			}

			bool operator()(list_value<THandler> & expr) {
				// TODO: this is incorrect!
				return true;
			}

			bool operator()(string_value & expr) {
				return true;
			}
			bool operator()(int_value & expr) {
				return true;
			}
			bool operator()(variable<THandler> & expr) {
				return false;
			}

			bool operator()(nil & expr) {
				return false;
			}
		};
		///////////////////////////////////////////////////////////////////////////
		//  Walk the tree
		///////////////////////////////////////////////////////////////////////////
		template<typename THandler>
		struct ast_bind {
			typedef bool result_type;
			typedef std::list<std::wstring> error_type;

			THandler & handler;
			ast_bind(THandler & handler) : handler(handler) {}

			bool operator()(expression_ast<THandler> & ast) {
				return ast.bind(handler) && boost::apply_visitor(*this, ast.expr);
			}
			bool operator()(binary_op<THandler> & expr) {
				bool r1 = operator()(expr.left);
				bool r2 = operator()(expr.right);
				return r1 && r2;
			}
			bool operator()(unary_op<THandler> & expr) {
				return operator()(expr.subject);
			}

			bool operator()(unary_fun<THandler> & expr) {
				return false;
			}

			bool operator()(list_value<THandler> & expr) {
				// TODO: this is incorrect!
				return true;
			}

			bool operator()(string_value & expr) {
				return true;
			}
			bool operator()(int_value & expr) {
				return true;
			}
			bool operator()(variable<THandler> & expr) {
				return true;
			}

			bool operator()(nil & expr) {
				return false;
			}
		};

		template<typename THandler>
		bool parser<THandler>::parse(std::wstring expr) {
			typedef std::wstring::const_iterator iterator_type;
			typedef where_grammar<THandler, iterator_type> grammar;

			grammar calc; // Our grammar

			iterator_type iter = expr.begin();
			iterator_type end = expr.end();
			if (phrase_parse(iter, end, calc, ascii::space, resulting_tree))
				return true;
			rest = std::wstring(iter, end);
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
		bool parser<THandler>::evaluate(THandler & handler) {
			try {
				expression_ast<THandler> ast = resulting_tree.evaluate(handler);
				return ast.get_int(handler);
			} catch (...) {
				handler.error(_T("Unhandled exception static eval: ") + result_as_tree());
				return false;
			}
		}

		template<typename THandler>
		std::wstring parser<THandler>::result_as_tree() const {
			return resulting_tree.to_string();
		}
	}
}


