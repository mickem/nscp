#pragma once

namespace parsers {
	namespace where {
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
				operator()(expr.subject);
				return true;
			}

			bool operator()(list_value<THandler> & expr) {
				BOOST_FOREACH(expression_ast<THandler> &e, expr.list) {
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
				return true;
			}

			bool operator()(nil & expr) {
				return false;
			}
		};
	}
}