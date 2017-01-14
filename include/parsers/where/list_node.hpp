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

#include <parsers/where/node.hpp>

namespace parsers {
	namespace where {
		struct list_node : public list_node_interface {
			list_node() {}
			std::list<node_type> value_;

			void push_back(node_type value) {
				value_.push_back(value);
			}

			virtual std::string to_string() const;
			virtual std::string to_string(evaluation_context errors) const;

			value_container get_value(evaluation_context context, value_type type) const;
			std::list<node_type> get_list_value(evaluation_context errors) const {
				return value_;
			}

			bool can_evaluate() const {
				return false;
			}
			node_type evaluate(evaluation_context context) const;
			bool bind(object_converter context);

			value_type infer_type(object_converter converter);
			value_type infer_type(object_converter converter, value_type suggestion);
			bool find_performance_data(evaluation_context context, performance_collector &collector);
			bool static_evaluate(evaluation_context context) const;
			bool require_object(evaluation_context context) const;
		};
	}
}