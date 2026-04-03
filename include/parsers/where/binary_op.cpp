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

#include <parsers/operators.hpp>
#include <parsers/where/binary_op.hpp>
#include <parsers/where/helpers.hpp>
#include <sstream>

namespace parsers {
namespace where {
bool binary_op::can_evaluate() const { return true; }
node_type binary_op::evaluate(const evaluation_context contxt) const {
  const op_factory::bin_op_type impl = op_factory::get_binary_operator(op, left, right);
  if (is_int() || is_string()) {
    return impl->evaluate(contxt, left, right);
  }
  contxt->error("Binary operator does not work with " + helpers::type_to_string(get_type()));
  return factory::create_false();
}
bool binary_op::bind(const object_converter contxt) { return left->bind(contxt) && right->bind(contxt); }

value_container binary_op::get_value(const evaluation_context contxt, value_type wanted_type) const { return evaluate(contxt)->get_value(contxt, wanted_type); }
std::list<node_type> binary_op::get_list_value(evaluation_context context) const { return {}; }

value_type binary_op::infer_type(const object_converter converter, value_type) { return infer_type(converter); }

value_type binary_op::infer_type(const object_converter converter) {
  value_type inferred_type = helpers::infer_binary_type(converter, left, right);
  if (inferred_type == type_invalid) return inferred_type;
  inferred_type = helpers::get_return_type(op, inferred_type);
  set_type(inferred_type);
  return inferred_type;
}

std::string binary_op::to_string() const {
  std::stringstream ss;
  ss << "{" << helpers::type_to_string(get_type()) + "}(" << left->to_string() << " " << helpers::operator_to_string(op) << " " << right->to_string() << ")";
  return ss.str();
}
std::string binary_op::to_string(const evaluation_context context) const {
  std::stringstream ss;
  ss << left->to_string(context) << " " << helpers::operator_to_string(op) << " " << right->to_string(context);
  return ss.str();
}

bool binary_op::find_performance_data(const evaluation_context context, performance_collector &collector) {
  if (op == op_nin || op == op_in) return false;
  performance_collector sub_collector_left;
  performance_collector sub_collector_right;
  const bool l = left->find_performance_data(context, sub_collector_left);
  const bool r = right->find_performance_data(context, sub_collector_right);
  if (l || r) {
    // We found performance data
    collector.add_perf(sub_collector_left);
    collector.add_perf(sub_collector_right);
    return true;
  }
  if (sub_collector_left.has_candidates() && sub_collector_right.has_candidates()) {
    if (helpers::is_upper(op)) return collector.add_bounds_candidates(sub_collector_left, sub_collector_right);
    if (helpers::is_lower(op)) return collector.add_bounds_candidates(sub_collector_right, sub_collector_left);
    return collector.add_neutral_candidates(sub_collector_left, sub_collector_right);
  }
  return false;
}

bool binary_op::require_object(const evaluation_context contxt) const { return left->require_object(contxt) || right->require_object(contxt); }
bool binary_op::static_evaluate(const evaluation_context contxt) const { return left->static_evaluate(contxt) && right->static_evaluate(contxt); }
}  // namespace where
}  // namespace parsers