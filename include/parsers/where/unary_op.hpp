// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <parsers/where/node.hpp>

namespace parsers {
namespace where {
struct unary_op : any_node {
  unary_op(const operators op, node_type const &subject) : op(op), subject(subject) {}
  unary_op() = delete;

  std::string to_string(evaluation_context context) const override;
  std::string to_string() const override;

  value_container get_value(evaluation_context context, value_type type) const override;
  std::list<node_type> get_list_value(evaluation_context context) const override;

  bool can_evaluate() const override;
  node_type evaluate(evaluation_context context) const override;
  bool bind(object_converter context) override;
  value_type infer_type(object_converter converter) override;
  value_type infer_type(object_converter converter, value_type suggestion) override;
  bool find_performance_data(evaluation_context context, performance_collector &collector) override;
  bool static_evaluate(evaluation_context context) const override;
  bool require_object(evaluation_context context) const override;

  operators op;
  node_type subject;
};
}  // namespace where
}  // namespace parsers