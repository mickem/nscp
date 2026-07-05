// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

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