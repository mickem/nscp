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
#include <parsers/where/unary_fun.hpp>
#include <str/xtos.hpp>

namespace parsers {
namespace where {
std::string unary_fun::to_string() const { return "{" + helpers::type_to_string(get_type()) + "}" + name + "(" + subject->to_string() + ")"; }
std::string unary_fun::to_string(const evaluation_context context) const {
  if (function) return name + "(" + function->evaluate(type_string, context, subject)->to_string(context) + ")";
  return name + "(" + subject->to_string(context) + ")";
}

value_container unary_fun::get_value(const evaluation_context context, value_type new_type) const { return evaluate(context)->get_value(context, new_type); }
std::list<node_type> unary_fun::get_list_value(const evaluation_context context) const {
  std::list<node_type> ret;
  for (const node_type n : subject->get_list_value(context)) {
    if (function)
      ret.push_back(function->evaluate(get_type(), context, n));
    else
      ret.push_back(factory::create_false());
  }
  return ret;
}

bool unary_fun::can_evaluate() const { return true; }
std::shared_ptr<any_node> unary_fun::evaluate(const evaluation_context context) const {
  if (function) return function->evaluate(get_type(), context, subject);
  context->error("Missing function binding: " + name + "bound: " + str::xtos(is_bound()));
  return factory::create_false();
}
bool unary_fun::bind(const object_converter converter) {
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
bool unary_fun::find_performance_data(const evaluation_context context, performance_collector &collector) {
  if ((name == "convert") || (name == "auto_convert" || is_transparent(type_tbd))) {
    performance_collector sub_collector;
    subject->find_performance_data(context, sub_collector);
    if (sub_collector.has_candidate_value()) {
      collector.set_candidate_value(shared_from_this());
    }
  }
  return false;
}
bool unary_fun::static_evaluate(const evaluation_context context) const {
  if ((name == "convert") || (name == "auto_convert" || is_transparent(type_tbd))) {
    return subject->static_evaluate(context);
  }
  subject->static_evaluate(context);
  return false;
}
bool unary_fun::require_object(const evaluation_context context) const { return subject->require_object(context); }

bool unary_fun::is_transparent(value_type) const {
  if (name == "neg") return true;
  return false;
}

bool unary_fun::is_bound() const { return static_cast<bool>(function); }
}  // namespace where
}  // namespace parsers