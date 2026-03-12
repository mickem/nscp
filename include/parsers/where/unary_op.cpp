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
#include <parsers/where/helpers.hpp>
#include <parsers/where/unary_op.hpp>

namespace parsers {
namespace where {
std::string unary_op::to_string() const { return helpers::operator_to_string(op) + " ( " + subject->to_string() + " ) "; }
std::string unary_op::to_string(const evaluation_context context) const { return helpers::operator_to_string(op) + " ( " + subject->to_string(context) + " ) "; }

value_container unary_op::get_value(const evaluation_context context, value_type new_type) const { return evaluate(context)->get_value(context, new_type); }
std::list<node_type> unary_op::get_list_value(evaluation_context context) const { return std::list<node_type>(); }

bool unary_op::can_evaluate() const { return true; }
node_type unary_op::evaluate(const evaluation_context context) const {
  const op_factory::un_op_type impl = op_factory::get_unary_operator(op);
  const value_type wanted_type = helpers::get_return_type(op, type_invalid);
  if (helpers::type_is_int(wanted_type)) {
    return impl->evaluate(context, subject);
  }
  context->error("Missing operator implementation");
  return factory::create_false();
}
bool unary_op::bind(const object_converter context) { return subject->bind(context); }
value_type unary_op::infer_type(const object_converter converter, const value_type suggestion) {
  const value_type wanted_type = subject->infer_type(converter, suggestion);
  return helpers::get_return_type(op, wanted_type);
}
value_type unary_op::infer_type(const object_converter converter) {
  const value_type wanted_type = subject->infer_type(converter);
  return helpers::get_return_type(op, wanted_type);
}
bool unary_op::find_performance_data(const evaluation_context context, performance_collector &collector) {
  return subject->find_performance_data(context, collector);
}
bool unary_op::static_evaluate(const evaluation_context context) const { return subject->static_evaluate(context); }
bool unary_op::require_object(const evaluation_context context) const { return subject->require_object(context); }
}  // namespace where
}  // namespace parsers