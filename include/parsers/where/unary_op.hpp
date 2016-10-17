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
		struct unary_op : public any_node {
			unary_op(operators op, node_type const subject) : op(op), subject(subject) {}

			virtual std::string to_string(evaluation_context errors) const;
			virtual std::string to_string() const;

			virtual value_container get_value(evaluation_context context, value_type type) const;
			virtual std::list<node_type> get_list_value(evaluation_context errors) const;

			virtual bool can_evaluate() const;
			virtual node_type evaluate(evaluation_context context) const;
			virtual bool bind(object_converter context);
			value_type infer_type(object_converter converter);
			value_type infer_type(object_converter converter, value_type suggestion);
			virtual bool find_performance_data(evaluation_context context, performance_collector &collector);
			virtual bool static_evaluate(evaluation_context context) const;
			virtual bool require_object(evaluation_context context) const;

		private:
			unary_op() {}
			operators op;
			node_type subject;
		};
	}
}