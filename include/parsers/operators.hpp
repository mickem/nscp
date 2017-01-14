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