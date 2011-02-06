#pragma once

namespace parsers {
	namespace where {

		template<typename THandler>
		struct binary_operator_impl {
			virtual expression_ast<THandler> evaluate(typename THandler::object_type &handler, const expression_ast<THandler> &left, const expression_ast<THandler> & right) const = 0;
		};
		template<typename THandler>
		struct binary_function_impl {
			virtual expression_ast<THandler> evaluate(value_type type, typename THandler::object_type &handler, const expression_ast<THandler> &subject) const = 0;
		};
		template<typename THandler>
		struct unary_operator_impl {
			virtual expression_ast<THandler> evaluate(typename THandler::object_type &handler, const expression_ast<THandler> &subject) const = 0;
		};
	}
}