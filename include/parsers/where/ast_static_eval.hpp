#pragma once

namespace parsers {
	namespace where {
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
						typename THandler::object_type static_object = handler.get_static_object();
						expression_ast<THandler> nexpr = ast.evaluate(static_object);
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
				if ((expr.name == _T("convert")) || (expr.name == _T("auto_convert") || expr.is_transparent(type_tbd) ) ) {
					return boost::apply_visitor(*this, expr.subject.expr);
				}
				return false;
			}

			bool operator()(list_value<THandler> & expr) {
				BOOST_FOREACH(expression_ast<THandler> e, expr.list) {
					operator()(e);
				}
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
	}
}