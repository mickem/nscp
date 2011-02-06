#pragma once

#include <boost/function.hpp>

namespace parsers {
	namespace where {

		template <typename THandler, typename object_type>
		struct varible_handler : public varible_type_handler {
			//typedef typename THandler::object_type object_type;
			typedef boost::function<std::wstring(object_type*)> bound_string_type;
			typedef boost::function<long long(object_type*)> bound_int_type;
			typedef boost::function<expression_ast<THandler>(object_type*,value_type,expression_ast<THandler> const&)> bound_function_type;

			virtual bound_string_type bind_string(std::wstring key) = 0;
			virtual bound_int_type bind_int(std::wstring key) = 0;
			virtual bool has_function(value_type to, std::wstring name, expression_ast<THandler> subject) = 0;
			virtual bound_function_type bind_function(value_type to, std::wstring name, expression_ast<THandler> subject) = 0;
		};

	}
}