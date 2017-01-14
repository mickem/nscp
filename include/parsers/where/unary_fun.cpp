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

#include <str/xtos.hpp>

#include <parsers/where/unary_fun.hpp>
#include <parsers/where/helpers.hpp>

#include <boost/foreach.hpp>

namespace parsers {
	namespace where {
		std::string unary_fun::to_string() const {
			return "{" + helpers::type_to_string(get_type()) + "}" + name + "(" + subject->to_string() + ")";
		}
		std::string unary_fun::to_string(evaluation_context errors) const {
			if (function)
				return name + "(" + function->evaluate(type_string, errors, subject)->to_string(errors) + ")";
			return name + "(" + subject->to_string(errors) + ")";
		}

		value_container unary_fun::get_value(evaluation_context errors, value_type type) const {
			return evaluate(errors)->get_value(errors, type);
		}
		std::list<node_type> unary_fun::get_list_value(evaluation_context errors) const {
			std::list<node_type> ret;
			BOOST_FOREACH(node_type n, subject->get_list_value(errors)) {
				if (function)
					ret.push_back(function->evaluate(get_type(), errors, n));
				else
					ret.push_back(factory::create_false());
			}
			return ret;
		}

		bool unary_fun::can_evaluate() const {
			return true;
		}
		boost::shared_ptr<any_node> unary_fun::evaluate(evaluation_context errors) const {
			if (function)
				return function->evaluate(get_type(), errors, subject);
			errors->error("Missing function binding: " + name + "bound: " + str::xtos(is_bound()));
			return factory::create_false();
		}
		bool unary_fun::bind(object_converter converter) {
			try {
				if (converter->can_convert(name, subject, get_type())) {
					function = converter->create_converter(name, subject, get_type());
				} else {
					function = op_factory::get_binary_function(converter, name, subject);
				}
				if (!function) {
					converter->error("Failed to create function: " + name);
					return false;
				}
				return true;
			} catch (...) {
				converter->error("Failed to bind function: " + name);
				return false;
			}
		}
		bool unary_fun::find_performance_data(evaluation_context context, performance_collector &collector) {
			if ((name == "convert") || (name == "auto_convert" || is_transparent(type_tbd))) {
				performance_collector sub_collector;
				subject->find_performance_data(context, sub_collector);
				if (sub_collector.has_candidate_value()) {
					collector.set_candidate_value(shared_from_this());
				}
			}
			return false;
		}
		bool unary_fun::static_evaluate(evaluation_context context) const {
			if ((name == "convert") || (name == "auto_convert" || is_transparent(type_tbd))) {
				return subject->static_evaluate(context);
			}
			subject->static_evaluate(context);
			return false;
		}
		bool unary_fun::require_object(evaluation_context context) const {
			return subject->require_object(context);
		}

		bool unary_fun::is_transparent(value_type) const {
			if (name == "neg")
				return true;
			return false;
		}

		bool unary_fun::is_bound() const {
			return static_cast<bool>(function);
		}
	}
}