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

#include <parsers/where/helpers.hpp>
#include <parsers/where/node.hpp>

namespace parsers {
namespace where {
template <class T>
struct node_value_impl : any_node {
  T value_;
  bool is_unsure_;

  node_value_impl(T value, const value_type type, const bool is_unsure) : any_node(type), value_(value), is_unsure_(is_unsure) {}
  node_value_impl(const node_value_impl<T> &other) : value_(other.value_), is_unsure_(other.is_unsure_) {}
  const node_value_impl<T> &operator=(const node_value_impl<T> &other) {
    value_ = other.value_;
    is_unsure_ = other.is_unsure_;
    return *this;
  }
  std::list<node_type> get_list_value(evaluation_context context) const override {
    std::list<node_type> ret;
    ret.push_back(factory::create_ios(value_));
    return ret;
  }
  bool can_evaluate() const override { return false; }
  std::shared_ptr<any_node> evaluate(evaluation_context context) const override { return factory::create_false(); }
  bool bind(object_converter context) override { return true; }
  bool static_evaluate(evaluation_context context) const override { return true; }
  bool require_object(evaluation_context context) const override { return false; }
};

struct string_value : node_value_impl<std::string>, std::enable_shared_from_this<string_value> {
  explicit string_value(const std::string &value, const bool is_unsure = false) : node_value_impl<std::string>(value, type_string, is_unsure) {}
  value_container get_value(evaluation_context context, value_type type) const override;
  std::string to_string() const override;
  std::string to_string(evaluation_context context) const override;
  value_type infer_type(object_converter, value_type) override { return type_string; }
  value_type infer_type(object_converter) override { return type_string; }
  bool find_performance_data(evaluation_context context, performance_collector &collector) override;
};
struct int_value : node_value_impl<long long>, std::enable_shared_from_this<int_value> {
  explicit int_value(const long long &value, const bool is_unsure = false) : node_value_impl<long long>(value, type_int, is_unsure) {}
  value_container get_value(evaluation_context context, value_type type) const override;
  std::string to_string() const override;
  std::string to_string(evaluation_context context) const override;
  value_type infer_type(object_converter converter, const value_type vt) override {
    if (helpers::type_is_int(vt)) return type_int;
    if (helpers::type_is_float(vt)) {
      set_type(vt);
      return vt;
    }
    return type_int;
  }
  value_type infer_type(object_converter converter) override { return type_int; }
  bool find_performance_data(evaluation_context context, performance_collector &collector) override;
};
struct float_value : node_value_impl<double>, std::enable_shared_from_this<float_value> {
  explicit float_value(const double &value, const bool is_unsure = false) : node_value_impl<double>(value, type_float, is_unsure) {}
  value_container get_value(evaluation_context context, value_type type) const override;
  std::string to_string() const override;
  std::string to_string(evaluation_context context) const override;
  value_type infer_type(object_converter converter, const value_type vt) override {
    if (helpers::type_is_float(vt)) {
      return type_float;
    }
    if (helpers::type_is_int(vt)) {
      set_type(vt);
      return vt;
    }
    return type_float;
  }
  value_type infer_type(object_converter converter) override { return type_float; }
  bool find_performance_data(evaluation_context context, performance_collector &collector) override;
};
}  // namespace where
}  // namespace parsers