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