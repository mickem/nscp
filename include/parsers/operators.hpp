// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <parsers/where/node.hpp>

namespace parsers {
namespace where {
struct op_factory {
  typedef std::shared_ptr<binary_operator_impl> bin_op_type;
  typedef std::shared_ptr<binary_function_impl> bin_fun_type;
  typedef std::shared_ptr<unary_operator_impl> un_op_type;

  static bin_op_type get_binary_operator(operators op, const node_type &left, const node_type &right);
  static bin_fun_type get_binary_function(evaluation_context context, const std::string &name, const node_type &subject);
  static bool is_binary_function(const std::string &name);
  static un_op_type get_unary_operator(operators op);
};
}  // namespace where
}  // namespace parsers