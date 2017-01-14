/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
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