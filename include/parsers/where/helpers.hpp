/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <string>
#include <list>

#include <boost/shared_ptr.hpp>
#include <boost/tuple/tuple.hpp>

#include <parsers/where/node.hpp>
#include <parsers/where/dll_defines.hpp>

namespace parsers {
	namespace where {
		namespace helpers {
			NSCAPI_EXPORT std::string type_to_string(value_type type);
			NSCAPI_EXPORT bool type_is_int(value_type type);
			NSCAPI_EXPORT bool type_is_float(value_type type);
			NSCAPI_EXPORT bool type_is_string(value_type type);
			NSCAPI_EXPORT value_type get_return_type(operators op, value_type type);
			NSCAPI_EXPORT std::string operator_to_string(operators const& identifier);
			NSCAPI_EXPORT value_type infer_binary_type(object_converter converter, node_type &left, node_type &right);
			NSCAPI_EXPORT bool can_convert(value_type src, value_type dst);
			NSCAPI_EXPORT bool is_upper(operators op);
			NSCAPI_EXPORT bool is_lower(operators op);

			typedef boost::tuple<long long, double, std::string> read_arg_type;
			NSCAPI_EXPORT read_arg_type read_arguments(parsers::where::evaluation_context context, parsers::where::node_type subject, std::string default_unit);
		}
	}
}