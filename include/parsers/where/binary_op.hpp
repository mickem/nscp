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
		struct binary_op : public any_node {
			binary_op(operators op, node_type left, node_type right) : op(op), left(left), right(right) {}

			//virtual bool can_convert_to(value_type newtype) = 0;
			//virtual void force_type(value_type newtype) = 0;

			virtual std::string to_string() const;
			virtual std::string to_string(evaluation_context errors) const;

			virtual value_container get_value(evaluation_context contxt, value_type type) const;
			virtual std::list<boost::shared_ptr<any_node> > get_list_value(evaluation_context errors) const;

			virtual bool can_evaluate() const;
			virtual node_type evaluate(evaluation_context contxt) const;
			virtual bool bind(object_converter contxt);
			virtual value_type infer_type(object_converter converter);
			virtual value_type infer_type(object_converter converter, value_type suggestion);
			virtual bool find_performance_data(evaluation_context context, performance_collector &collector);
			virtual bool static_evaluate(evaluation_context contxt) const;
			virtual bool require_object(evaluation_context contxt) const;

		private:
			binary_op() {}
			operators op;
			node_type left;
			node_type right;
		};
	}
}