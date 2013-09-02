#pragma once

#include <parsers/where/node.hpp>

namespace parsers {
	namespace where {
		struct op_factory {
			typedef boost::shared_ptr<binary_operator_impl> bin_op_type;
			typedef boost::shared_ptr<binary_function_impl> bin_fun_type;
			typedef boost::shared_ptr<unary_operator_impl> un_op_type;

			static bin_op_type get_binary_operator(operators op, const node_type left, const node_type right);
			static bin_fun_type get_binary_function(evaluation_context errors, std::string name, const node_type subject);
			static bool is_binary_function(std::string name);
			static un_op_type get_unary_operator(operators op);
		};
	}
}
