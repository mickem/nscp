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

#include <memory>
#include <parsers/where/node.hpp>

namespace parsers {
namespace where {
struct unary_fun : any_node, std::enable_shared_from_this<unary_fun> {
 private:
  std::string name;
  node_type subject;
  std::shared_ptr<binary_function_impl> function;

 public:
  unary_fun(const std::string &name, node_type const &subject) : name(name), subject(subject) {}
  unary_fun() = delete;

  std::string to_string() const override;
  std::string to_string(evaluation_context context) const override;

  value_container get_value(evaluation_context context, value_type type) const override;
  std::list<node_type> get_list_value(evaluation_context context) const override;

  bool can_evaluate() const override;
  node_type evaluate(evaluation_context context) const override;
  bool bind(object_converter converter) override;
  value_type infer_type(const object_converter converter, value_type) override { return infer_type(converter); }
  value_type infer_type(object_converter converter) override { return type_tbd; }
  bool find_performance_data(evaluation_context context, performance_collector &collector) override;
  bool static_evaluate(evaluation_context context) const override;
  bool require_object(evaluation_context context) const override;

 private:
  bool is_transparent(value_type type) const;
  bool is_bound() const;
};
}  // namespace where
}  // namespace parsers