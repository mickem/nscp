#pragma once

namespace parsers {
	namespace where {
		///////////////////////////////////////////////////////////////////////////
		//  Walk the tree
		///////////////////////////////////////////////////////////////////////////
		struct ast_bind {
			typedef bool result_type;
			typedef std::list<std::string> error_type;

			filter_handler handler;
			ast_bind(filter_handler handler) : handler(handler) {}

			bool operator()(expression_ast & ast) {
				return ast.bind(handler) && boost::apply_visitor(*this, ast.expr);
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
				operator()(expr.subject);
				return true;
			}

			bool operator()(list_value & expr) {
				BOOST_FOREACH(expression_ast &e, expr.list) {
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
				return true;
			}

			bool operator()(nil & expr) {
				return false;
			}
		};
	}
}