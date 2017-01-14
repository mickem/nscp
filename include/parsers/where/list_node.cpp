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

#include <boost/foreach.hpp>

#include <parsers/where/list_node.hpp>

namespace parsers {
	namespace where {
		std::string list_node::to_string() const {
			std::string ret;
			BOOST_FOREACH(const node_type n, value_) {
				if (!ret.empty())
					ret += ", ";
				ret += n->to_string();
			}
			return ret;
		}
		std::string list_node::to_string(evaluation_context errors) const {
			std::string ret;
			BOOST_FOREACH(const node_type n, value_) {
				if (!ret.empty())
					ret += ", ";
				ret += n->to_string(errors);
			}
			return ret;
		}

		value_container list_node::get_value(evaluation_context errors, value_type type) const {
			if (type == type_int) {
				errors->error("Cant get number from a list");
				return value_container::create_nil();
			}
			if (type == type_float) {
				errors->error("Cant get number from a list");
				return value_container::create_nil();
			}
			if (type == type_string) {
				std::string s;
				BOOST_FOREACH(const node_type n, value_) {
					if (!s.empty())
						s += ", ";
					s += n->get_string_value(errors);
				}
				return value_container::create_string(s);
			}
			errors->error("Invalid type");
			return value_container::create_nil();
		}

		node_type list_node::evaluate(evaluation_context errors) const {
			BOOST_FOREACH(const node_type n, value_) {
				n->evaluate(errors);
			}
			return factory::create_false();
		}
		bool list_node::bind(object_converter errors) {
			bool ret = true;
			BOOST_FOREACH(const node_type n, value_) {
				if (!n->bind(errors))
					ret = false;
			}
			return ret;
		}
		value_type list_node::infer_type(object_converter converter, value_type suggestion) {
			BOOST_FOREACH(const node_type n, value_) {
				n->infer_type(converter, suggestion);
			}
			return type_tbd;
		}
		value_type list_node::infer_type(object_converter converter) {
			bool first = true;
			value_type types = type_tbd;

			BOOST_FOREACH(const node_type n, value_) {
				if (first) {
					types = n->infer_type(converter);
					first = false;
				} else if (types != n->infer_type(converter)) {
					if (types != type_tbd) {
						types = type_tbd;
					}
				}
			}
			if (types != type_tbd)
				set_type(types);
			return types;
		}
		bool list_node::find_performance_data(evaluation_context context, performance_collector &collector) {
			BOOST_FOREACH(const node_type n, value_) {
				n->find_performance_data(context, collector);
			}
			return false;
		}
		bool list_node::static_evaluate(evaluation_context errors) const {
			BOOST_FOREACH(const node_type n, value_) {
				n->static_evaluate(errors);
			}
			return true;
		}
		bool list_node::require_object(evaluation_context errors) const {
			BOOST_FOREACH(const node_type n, value_) {
				n->require_object(errors);
			}
			return true;
		}
	}
}