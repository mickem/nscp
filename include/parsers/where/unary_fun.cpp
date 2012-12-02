#include <parsers/where/unary_fun.hpp>
#include <parsers/where/list_value.hpp>
#include <parsers/where/binary_op.hpp>
#include <parsers/where/unary_op.hpp>
#include <parsers/where/variable.hpp>

#include <parsers/operators.hpp>

namespace parsers {
	namespace where {

		expression_ast unary_fun::evaluate(filter_handler handler, value_type type) const {
			if (i_fn)
				return i_fn->evaluate(type, handler, subject);
			if (function_id)
				return handler->execute_function(function_id, type, handler, &subject);
			handler->error(_T("Missing function binding: ") + name + _T("bound: ") + strEx::itos(is_bound()));
			return expression_ast(int_value(FALSE));
		}

		bool unary_fun::bind(value_type type, filter_handler handler) {
			try {
				if (handler->has_function(type, name, &subject)) {
					function_id = handler->bind_function(type, name, &subject);
					if (!function_id) {
						handler->error(_T("Failed to bind function: ") + name);
						return false;
					}
					return true;
				}
				i_fn = parsers::where::factory::get_binary_function(name, subject);
				if (!i_fn) {
					handler->error(_T("Failed to create function: ") + name);
					return false;
				}
				return true;
			} catch (...) {
				handler->error(_T("Failed to bind function: ") + name);
				return false;
			}
		}
		bool unary_fun::is_transparent(value_type type) {
			// TODO make the handler be allowed to have a say here
			if (name == _T("neg"))
				return true;
			return false;
		}

		bool unary_fun::is_bound() const {
			return function_id || i_fn;
		}		
	}
}
