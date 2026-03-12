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

#include <list>
#include <parsers/where/node.hpp>
#include <string>

namespace parsers {
namespace where {
struct list_node : list_node_interface {
  list_node() = default;
  std::list<node_type> value_;

  void push_back(const node_type value) override { value_.push_back(value); }

  std::string to_string() const override;
  std::string to_string(evaluation_context context) const override;

  value_container get_value(evaluation_context context, value_type type) const override;
  std::list<node_type> get_list_value(evaluation_context context) const override { return value_; }

  bool can_evaluate() const override { return false; }
  node_type evaluate(evaluation_context context) const override;
  bool bind(object_converter context) override;

  value_type infer_type(object_converter converter) override;
  value_type infer_type(object_converter converter, value_type suggestion) override;
  bool find_performance_data(evaluation_context context, performance_collector &collector) override;
  bool static_evaluate(evaluation_context context) const override;
  bool require_object(evaluation_context context) const override;
};
}  // namespace where
}  // namespace parsers