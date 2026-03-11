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

#include <parsers/where/list_node.hpp>

namespace parsers {
namespace where {
std::string list_node::to_string() const {
  std::string ret;
  for (const node_type& n : value_) {
    if (!ret.empty()) ret += ", ";
    ret += n->to_string();
  }
  return ret;
}
std::string list_node::to_string(const evaluation_context context) const {
  std::string ret;
  for (const node_type& n : value_) {
    if (!ret.empty()) ret += ", ";
    ret += n->to_string(context);
  }
  return ret;
}

value_container list_node::get_value(const evaluation_context context, value_type new_type) const {
  if (new_type == type_int) {
    context->error("Cant get number from a list");
    return value_container::create_nil();
  }
  if (new_type == type_float) {
    context->error("Cant get number from a list");
    return value_container::create_nil();
  }
  if (new_type == type_string) {
    std::string s;
    for (const node_type& n : value_) {
      if (!s.empty()) s += ", ";
      s += n->get_string_value(context);
    }
    return value_container::create_string(s);
  }
  context->error("Invalid type");
  return value_container::create_nil();
}

node_type list_node::evaluate(evaluation_context context) const {
  for (const node_type& n : value_) {
    n->evaluate(context);
  }
  return factory::create_false();
}
bool list_node::bind(object_converter context) {
  bool ret = true;
  for (const node_type& n : value_) {
    if (!n->bind(context)) ret = false;
  }
  return ret;
}
value_type list_node::infer_type(const object_converter converter, value_type suggestion) {
  for (const node_type& n : value_) {
    n->infer_type(converter, suggestion);
  }
  return type_tbd;
}
value_type list_node::infer_type(const object_converter converter) {
  bool first = true;
  value_type types = type_tbd;

  for (const node_type& n : value_) {
    if (first) {
      types = n->infer_type(converter);
      first = false;
    } else if (types != n->infer_type(converter)) {
      if (types != type_tbd) {
        types = type_tbd;
      }
    }
  }
  if (types != type_tbd) set_type(types);
  return types;
}
bool list_node::find_performance_data(const evaluation_context context, performance_collector& collector) {
  for (const node_type& n : value_) {
    n->find_performance_data(context, collector);
  }
  return false;
}
bool list_node::static_evaluate(const evaluation_context context) const {
  for (const node_type& n : value_) {
    n->static_evaluate(context);
  }
  return true;
}
bool list_node::require_object(const evaluation_context context) const {
  for (const node_type& n : value_) {
    n->require_object(context);
  }
  return true;
}
}  // namespace where
}  // namespace parsers