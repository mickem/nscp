// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <parsers/where/dll_defines.hpp>
#include <parsers/where/node.hpp>

namespace parsers {
namespace where {
struct parser {
  node_type resulting_tree;
  std::string rest;
  bool parse(object_factory factory, std::string expr);
  bool derive_types(object_converter converter);
  bool static_eval(evaluation_context context);
  bool bind(object_converter context);
  value_container evaluate(evaluation_context context);
  bool collect_perfkeys(evaluation_context context, performance_collector &boundries);
  std::string result_as_tree() const;
  std::string result_as_tree(evaluation_context context) const;
  bool require_object(evaluation_context context) const;
};

// Convenience wrapper that lives inside nscp_where_filter.dll so callers in
// other modules can route a string through the where-grammar without each
// module needing to link parser internals. Used by modern_filter's
// detail-syntax placeholder routing. Returns an empty node_type on failure.
NSCAPI_EXPORT node_type parse_expression(object_factory factory, const std::string &expr);
}  // namespace where
}  // namespace parsers