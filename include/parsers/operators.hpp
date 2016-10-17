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