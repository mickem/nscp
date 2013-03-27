#pragma once

namespace parsers {
	namespace where {
		///////////////////////////////////////////////////////////////////////////
		//  Walk the tree
		///////////////////////////////////////////////////////////////////////////
		struct ast_static_eval {
			typedef bool result_type;
			typedef std::list<std::string> error_type;

			filter_handler handler;
			ast_static_eval(filter_handler handler) : handler(handler) {}

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
			bool operator()(binary_op & expr) {
				bool r1 = operator()(expr.left);
				bool r2 = operator()(expr.right);
				return r1 && r2;
			}
			bool operator()(unary_op & expr) {
				return operator()(expr.subject);
			}

			bool operator()(unary_fun & expr) {
				if ((expr.name == "convert") || (expr.name == "auto_convert" || expr.is_transparent(type_tbd) ) ) {
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
				return true;
			}
			bool operator()(int_value & expr) {
				return true;
			}
			bool operator()(variable & expr) {
				return false;
			}

			bool operator()(nil & expr) {
				return false;
			}
		};
	}
}