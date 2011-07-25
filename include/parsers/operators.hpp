#pragma once

#include <parsers/where/expression_ast.hpp>

namespace parsers {
	namespace where {

		struct binary_operator_impl {
			virtual expression_ast evaluate(filter_handler handler, const expression_ast & left, const expression_ast & right) const = 0;
		};
		struct binary_function_impl {
			virtual expression_ast evaluate(value_type type, filter_handler handler, const expression_ast &subject) const = 0;
		};
		struct unary_operator_impl {
			virtual expression_ast evaluate(filter_handler handler, const expression_ast &subject) const = 0;
		};


		struct factory {
			typedef boost::shared_ptr<binary_operator_impl> bin_op_type;
			typedef boost::shared_ptr<binary_function_impl> bin_fun_type;
			typedef boost::shared_ptr<unary_operator_impl> un_op_type;

			static bin_op_type get_binary_operator(operators op, const expression_ast &left, const expression_ast &right);
			static bin_fun_type get_binary_function(std::wstring name, const expression_ast &subject);
			static un_op_type get_unary_operator(operators op);
		};
	}
}
