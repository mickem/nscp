#pragma once

#include <parsers/where/expression_ast.hpp>

namespace parsers {
	namespace where {

		struct unary_op {
			unary_op(operators op, expression_ast const& subject): op(op), subject(subject) {}

			expression_ast evaluate(filter_handler handler) const;

		private:
			unary_op() {}
		public: // todo change this!
			operators op;
			expression_ast subject;
		};


	}
}