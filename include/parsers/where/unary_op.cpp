#include <parsers/where/unary_fun.hpp>
#include <parsers/where/list_value.hpp>
#include <parsers/where/binary_op.hpp>
#include <parsers/where/unary_op.hpp>
#include <parsers/where/variable.hpp>

#include <parsers/operators.hpp>

namespace parsers {
	namespace where {
		expression_ast unary_op::evaluate(filter_handler handler) const {
			factory::un_op_type impl = factory::get_unary_operator(op);
			value_type type = get_return_type(op, type_invalid);
			if (type_is_int(type)) {
				return impl->evaluate(handler, subject);
			}
			handler->error("Missing operator implementation");
			return expression_ast(int_value(FALSE));
		}
	
	}
}

