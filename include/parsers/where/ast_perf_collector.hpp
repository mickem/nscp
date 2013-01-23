#pragma once

#include <set>

namespace parsers {
	namespace where {
		///////////////////////////////////////////////////////////////////////////
		//  Walk the tree
		///////////////////////////////////////////////////////////////////////////
		struct ast_perf_collector {
			typedef std::map<std::wstring,std::wstring> boundries_type;
			boundries_type boundries;
			typedef bool result_type;
			typedef std::list<std::wstring> error_type;

			filter_handler handler;
			ast_perf_collector(filter_handler handler) : handler(handler) {}
			std::wstring last_value;
			std::wstring last_variable;

			bool operator()(expression_ast & ast) {
				bool result = boost::apply_visitor(*this, ast.expr);
				if (result) {
					if (ast.can_evaluate()) {
						ast.bind(handler);
						expression_ast nexpr = ast.evaluate(handler);
						ast.expr = nexpr.expr;
					}
				}
				return result;
			}
			void push(const std::wstring &var, const std::wstring &val) {
				boundries_type::iterator it = boundries.find(var);
				if (it == boundries.end()) {
					std::wcout << _T("*** Found: ") <<  var << _T(" : ") << val << std::endl;
					boundries[var] = val;
				} else {
					// TODO: increase if possible...
				}
			}
			bool operator()(binary_op & expr) {
				bool r1 = operator()(expr.left);
				bool r2 = operator()(expr.right);
				if (r1 && r2) {
				} else if (last_value.empty() || last_variable.empty()) {
					// ignore partially empty setups
				} else {
					push(last_variable, last_value);
				}
				last_value = _T("");
				last_variable = _T("");
				return false;
			}
			bool operator()(unary_op & expr) {
				return operator()(expr.subject);
			}

			bool operator()(unary_fun & expr) {
				if ((expr.name == _T("convert")) || (expr.name == _T("auto_convert") || expr.is_transparent(type_tbd) ) ) {
					return boost::apply_visitor(*this, expr.subject.expr);
				}
				return false;
			}

			bool operator()(list_value & expr) {
				BOOST_FOREACH(expression_ast e, expr.list) {
					operator()(e);
				}
				return true;
			}

			bool operator()(string_value & expr) {
				last_value = expr.value;
				return true;
			}
			bool operator()(int_value & expr) {
				last_value = strEx::itos(expr.value);
				return true;
			}
			bool operator()(variable & expr) {
				last_variable = expr.get_name();
				return false;
			}

			bool operator()(nil & expr) {
				return false;
			}
		};
	}
}