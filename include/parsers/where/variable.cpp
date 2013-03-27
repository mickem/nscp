#include <parsers/where/unary_fun.hpp>
#include <parsers/where/list_value.hpp>
#include <parsers/where/binary_op.hpp>
#include <parsers/where/unary_op.hpp>
#include <parsers/where/variable.hpp>


namespace parsers {
	namespace where {

		bool variable::bind(value_type type, filter_handler handler) {
			if (type_is_int(type)) {
				int_ptr_id = handler->bind_int(name);
				if (!int_ptr_id)
					handler->error("Failed to bind (int) variable: " + name);
				return int_ptr_id;
			} else {
				string_ptr_id = handler->bind_string(name);
				if (!string_ptr_id)
					handler->error("Failed to bind (string) variable: " + name);
				return string_ptr_id;
			}
			handler->error("Failed to bind (unknown) variable: " + name);
			return false;
		}

		long long variable::get_int(filter_handler handler) const {
			if (int_ptr_id)
				return handler->execute_int(int_ptr_id);
			if (string_ptr_id)
				handler->error("Int variable bound to string: " + name);
			else
				handler->error("Int variable not bound: " + name);
			return -1;
		}
		std::string variable::get_string(filter_handler handler) const {
			if (string_ptr_id)
				return handler->execute_string(string_ptr_id);
			if (int_ptr_id)
				handler->error("String variable bound to int: " + name);
			else
				handler->error("String variable not bound: " + name);
			return "";
		}
	}
}