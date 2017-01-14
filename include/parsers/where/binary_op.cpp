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

#include <sstream>

#include <parsers/where/binary_op.hpp>
#include <parsers/where/helpers.hpp>
#include <parsers/operators.hpp>

namespace parsers {
	namespace where {
		bool binary_op::can_evaluate() const {
			return true;
		}
		node_type binary_op::evaluate(evaluation_context errors) const {
			op_factory::bin_op_type impl = op_factory::get_binary_operator(op, left, right);
			if (is_int() || is_string()) {
				return impl->evaluate(errors, left, right);
			}
			errors->error("Missing operator implementation");
			return factory::create_false();
		}
		bool binary_op::bind(object_converter errors) {
			return left->bind(errors) && right->bind(errors);
		}

		value_container binary_op::get_value(evaluation_context errors, value_type type) const {
			return evaluate(errors)->get_value(errors, type);
		}
		std::list<node_type> binary_op::get_list_value(evaluation_context errors) const {
			return std::list<node_type>();
		}

		value_type binary_op::infer_type(object_converter converter, value_type) {
			return infer_type(converter);
		}

		value_type binary_op::infer_type(object_converter converter) {
			value_type type = helpers::infer_binary_type(converter, left, right);
			if (type == type_invalid)
				return type;
			type = helpers::get_return_type(op, type);
			set_type(type);
			return type;
		}

		std::string binary_op::to_string() const {
			std::stringstream ss;
			ss << "(" << helpers::type_to_string(get_type()) + "){" << left->to_string() << " " << helpers::operator_to_string(op) << " " << right->to_string() << "}";
			return ss.str();
		}
		std::string binary_op::to_string(evaluation_context errors) const {
			std::stringstream ss;
			ss << left->to_string(errors) << " " << helpers::operator_to_string(op) << " " << right->to_string(errors);
			return ss.str();
		}

		bool binary_op::find_performance_data(evaluation_context context, performance_collector &collector) {
			if (op == op_nin || op == op_in)
				return false;
			performance_collector sub_collector_left;
			performance_collector sub_collector_right;
			bool l = left->find_performance_data(context, sub_collector_left);
			bool r = right->find_performance_data(context, sub_collector_right);
			if (l || r) {
				// We found performance data
				collector.add_perf(sub_collector_left);
				collector.add_perf(sub_collector_right);
				return true;
			} else if (sub_collector_left.has_candidates() && sub_collector_right.has_candidates()) {
				if (helpers::is_upper(op))
					return collector.add_bounds_candidates(sub_collector_left, sub_collector_right);
				else if (helpers::is_lower(op))
					return collector.add_bounds_candidates(sub_collector_right, sub_collector_left);
				else
					return collector.add_neutral_candidates(sub_collector_left, sub_collector_right);
			}
			return false;
		}

		bool binary_op::require_object(evaluation_context errors) const {
			return left->require_object(errors) || right->require_object(errors);
		}
		bool binary_op::static_evaluate(evaluation_context errors) const {
			return left->static_evaluate(errors) && right->static_evaluate(errors);
		}
	}
}