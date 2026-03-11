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

#include <parsers/where/node.hpp>
#include <utility>

namespace parsers {
namespace where {
struct binary_op : any_node {
  binary_op() = delete;
  binary_op(const operators op, node_type left, node_type right) : op(op), left(std::move(left)), right(std::move(right)) {}

  std::string to_string() const override;
  std::string to_string(evaluation_context context) const override;

  value_container get_value(evaluation_context contxt, value_type type) const override;
  std::list<std::shared_ptr<any_node> > get_list_value(evaluation_context context) const override;

  bool can_evaluate() const override;
  node_type evaluate(evaluation_context contxt) const override;
  bool bind(object_converter contxt) override;
  value_type infer_type(object_converter converter) override;
  value_type infer_type(object_converter converter, value_type suggestion) override;
  bool find_performance_data(evaluation_context context, performance_collector &collector) override;
  bool static_evaluate(evaluation_context contxt) const override;
  bool require_object(evaluation_context contxt) const override;

 private:
  operators op;
  node_type left;
  node_type right;
};
}  // namespace where
}  // namespace parsers