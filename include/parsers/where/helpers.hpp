#pragma once

#include <string>
#include <list>

#include <boost/shared_ptr.hpp>

#include <parsers/where/node.hpp>

namespace parsers {
	namespace where {
		namespace helpers {
			std::string type_to_string(value_type type);
			bool type_is_int(value_type type);
			bool type_is_string(value_type type);
			value_type get_return_type(operators op, value_type type);
			std::string operator_to_string(operators const& identifier);
			value_type infer_binary_type(object_converter converter, node_type &left, node_type &right);
			bool can_convert(value_type src, value_type dst);
			bool is_upper(operators op);
			bool is_lower(operators op);
		}
	}
}
