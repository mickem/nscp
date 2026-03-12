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
#include <parsers/where/list_node.hpp>
#include <parsers/where/node.hpp>
#include <parsers/where/unary_fun.hpp>
#include <parsers/where/unary_op.hpp>
#include <parsers/where/value_node.hpp>
#include <str/xtos.hpp>
#include <utf8.hpp>

namespace parsers {
namespace where {
std::string value_container::get_string() const {
  if (i_value) return str::xtos(*i_value);
  if (f_value) return str::xtos(*f_value);
  if (s_value) return *s_value;
  throw filter_exception("Type is not string");
}
std::string value_container::get_string(std::string def) const {
  if (i_value) return str::xtos(*i_value);
  if (f_value) return str::xtos(*f_value);
  if (s_value) return *s_value;
  return def;
}

std::string filter_exception::reason() const throw() { return utf8::utf8_from_native(what()); }

bool any_node::is_int() const { return helpers::type_is_int(type); }
bool any_node::is_float() const { return helpers::type_is_float(type); }
bool any_node::is_string() const { return helpers::type_is_string(type); }

void performance_collector::add_perf(const performance_collector &other) {
  if (!other.has_candidate_value()) candidate_value_ = other.candidate_value_;
  if (!other.has_candidate_variable()) candidate_variable_ = other.candidate_variable_;
  boundaries.insert(other.boundaries.begin(), other.boundaries.end());
}

bool performance_collector::has_candidates() const { return has_candidate_value() || has_candidate_variable(); }

bool performance_collector::add_bounds_candidates(const performance_collector &lower, const performance_collector &upper) {
  if (lower.has_candidate_variable() && upper.has_candidate_value()) {
    if (boundaries.find(lower.get_variable()) == boundaries.end()) {
      performance_node node;
      node.value = upper.get_value();
      node.variable = lower.get_variable();
      node.perf_node_type = performance_node::perf_type_upper;
      boundaries[node.variable] = node;
    } else {
      // TODO: evaluate size and increase is possible...
    }
    return true;
  }
  if (upper.has_candidate_variable() && lower.has_candidate_value()) {
    if (boundaries.find(upper.get_variable()) == boundaries.end()) {
      performance_node node;
      node.value = lower.get_value();
      node.variable = upper.get_variable();
      node.perf_node_type = performance_node::perf_type_lower;
      boundaries[node.variable] = node;
    } else {
      // TODO: evaluate size and increase is possible...
    }
    return true;
  }
  return false;
}

bool performance_collector::add_neutral_candidates(const performance_collector &left, const performance_collector &right) {
  if (left.has_candidate_variable() && right.has_candidate_value()) {
    if (boundaries.find(left.get_variable()) == boundaries.end()) {
      performance_node node;
      node.value = right.get_value();
      node.variable = left.get_variable();
      node.perf_node_type = performance_node::perf_type_neutral;
      boundaries[node.variable] = node;
    }
    return true;
  }
  if (right.has_candidate_variable() && left.has_candidate_value()) {
    if (boundaries.find(right.get_variable()) == boundaries.end()) {
      performance_node node;
      node.value = left.get_value();
      node.variable = right.get_variable();
      node.perf_node_type = performance_node::perf_type_neutral;
      boundaries[node.variable] = node;
    }
    return true;
  }
  return false;
}

bool performance_collector::has_candidate_value() const { return static_cast<bool>(candidate_value_); }

bool performance_collector::has_candidate_variable() const { return !candidate_variable_.empty(); }

std::string performance_collector::get_variable() const { return candidate_variable_; }

node_type performance_collector::get_value() const { return candidate_value_; }

void performance_collector::set_candidate_value(const node_type &value) { candidate_value_ = value; }
void performance_collector::set_candidate_variable(const std::string &name) { candidate_variable_ = name; }

performance_collector::boundaries_type performance_collector::get_candidates() {
  boundaries_type ret;
  ret.insert(boundaries.begin(), boundaries.end());
  return ret;
}

//////////////////////////////////////////////////////////////////////////
//
// Factory implementations
node_type factory::create_list(const std::list<std::string> &other) {
  std::shared_ptr<list_node_interface> node(new list_node);
  for (const std::string &v : other) {
    node->push_back(create_string(v));
  }
  return node;
}
list_node_type factory::create_list() { return list_node_type(new list_node); }
node_type factory::create_list(const std::list<long long> &other) {
  std::shared_ptr<list_node_interface> node(new list_node);
  for (const long long &v : other) {
    node->push_back(create_int(v));
  }
  return node;
}
node_type factory::create_list(const std::list<double> &other) {
  std::shared_ptr<list_node_interface> node(new list_node);
  for (const double &v : other) {
    node->push_back(create_float(v));
  }
  return node;
}

node_type factory::create_bin_op(const operators &op, node_type lhs, node_type rhs) { return std::make_shared<binary_op>(op, lhs, rhs); }
node_type factory::create_un_op(const operators op, const node_type &node) { return std::make_shared<unary_op>(op, node); }
node_type factory::create_fun(const object_factory &factory, const std::string &op, const node_type &node) {
  if (op_factory::is_binary_function(op)) return std::make_shared<unary_fun>(op, node);
  if (factory->has_function(op)) return factory->create_function(op, node);
  factory->error("Function not found: " + op);
  return create_false();
}
node_type factory::create_conversion(node_type node) { return std::make_shared<unary_fun>("convert", node); }
node_type factory::create_ios(const long long &value) { return create_int(value); }
node_type factory::create_ios(const double &value) { return create_float(value); }
node_type factory::create_ios(const std::string &value) { return create_string(value); }
node_type factory::create_string(const std::string &value) { return std::make_shared<string_value>(value); }
node_type factory::create_int(const long long &value) { return std::make_shared<int_value>(value); }
node_type factory::create_float(const double &value) { return std::make_shared<float_value>(value); }
node_type factory::create_neg_int(const long long &value) { return std::make_shared<int_value>(-value); }
node_type factory::create_variable(const object_factory &factory, const std::string &name) {
  if (factory->has_variable(name)) {
    return factory->create_variable(name, false);
  }
  factory->error("Variable not found: " + name);
  return create_false();
}
node_type factory::create_false() { return std::make_shared<int_value>(0); }
node_type factory::create_true() { return std::make_shared<int_value>(1); }

node_type factory::create_num(value_container value) {
  if (value.is(type_int)) return std::make_shared<int_value>(value.get_int(0), value.is_unsure);
  if (value.is(type_float)) return std::make_shared<float_value>(value.get_float(0.0), value.is_unsure);
  if (value.is(type_string)) return std::make_shared<string_value>(value.get_string("0"), value.is_unsure);
  return std::make_shared<int_value>(0);
}
}  // namespace where
}  // namespace parsers