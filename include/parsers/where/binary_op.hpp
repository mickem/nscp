#pragma once

#include <parsers/where/expression_ast.hpp>

namespace parsers {
	namespace where {
		struct binary_op {

			binary_op(operators op, expression_ast const& left, expression_ast const& right): op(op), left(left), right(right) {}

			expression_ast evaluate(filter_handler handler, value_type type) const;

		private:
			binary_op() {}
		public: // toido: change this!
			operators op;
			expression_ast left;
			expression_ast right;
		};
	}
}