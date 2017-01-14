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

#include <boost/enable_shared_from_this.hpp>

#include <parsers/where/node.hpp>
#include <parsers/operators.hpp>

namespace parsers {
	namespace where {
		struct unary_fun : public any_node, boost::enable_shared_from_this<unary_fun> {
		private:
			std::string name;
			node_type subject;
			boost::shared_ptr<binary_function_impl> function;

		public:
			unary_fun(std::string name, node_type const subject) : name(name), subject(subject) {}

			virtual std::string to_string() const;
			virtual std::string to_string(evaluation_context errors) const;

			virtual value_container get_value(evaluation_context context, value_type type) const;
			virtual std::list<node_type> get_list_value(evaluation_context errors) const;

			virtual bool can_evaluate() const;
			virtual node_type evaluate(evaluation_context context) const;
			virtual bool bind(object_converter context);
			virtual value_type infer_type(object_converter converter, value_type) {
				return infer_type(converter);
			}
			virtual value_type infer_type(object_converter converter) {
				return type_tbd;
			}
			virtual bool find_performance_data(evaluation_context context, performance_collector &collector);
			virtual bool static_evaluate(evaluation_context context) const;
			virtual bool require_object(evaluation_context context) const;

		private:
			bool is_transparent(value_type type) const;
			bool is_bound() const;
		private:
			unary_fun() {}
		};
	}
}