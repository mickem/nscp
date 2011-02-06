#pragma once

namespace parsers {
	namespace where {

		template<typename THandler>
		struct binary_op {
			typedef typename THandler::object_type object_type;

			binary_op(operators op, expression_ast<THandler> const& left, expression_ast<THandler> const& right): op(op), left(left), right(right) {}

			expression_ast<THandler> evaluate(value_type type, object_type &handler) const;

			operators op;
			expression_ast<THandler> left;
			expression_ast<THandler> right;
		};

		template<typename THandler>
		struct unary_op {
			typedef typename THandler::object_type object_type;

			unary_op(operators op, expression_ast<THandler> const& subject): op(op), subject(subject) {}

			expression_ast<THandler> evaluate(object_type &handler) const;

			operators op;
			expression_ast<THandler> subject;
		};


	}
}