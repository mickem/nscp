// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

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