#include <parsers/where/unary_fun.hpp>
#include <parsers/where/list_value.hpp>
#include <parsers/where/binary_op.hpp>
#include <parsers/where/unary_op.hpp>
#include <parsers/where/variable.hpp>

#include <parsers/operators.hpp>

namespace parsers {
	namespace where {
		expression_ast binary_op::evaluate(filter_handler handler, value_type type) const {
			factory::bin_op_type impl = factory::get_binary_operator(op, left, right);
			value_type expected_type = get_return_type(op, type);
			if (type_is_int(type)) {
				return impl->evaluate(handler, left, right);
			}
			handler->error(_T("Missing operator implementation"));
			return expression_ast(int_value(FALSE));
		}
	}
}

