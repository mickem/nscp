// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/tuple/tuple.hpp>
#include <parsers/where/dll_defines.hpp>
#include <parsers/where/node.hpp>
#include <string>

namespace parsers {
namespace where {
namespace helpers {
NSCP_WHERE_EXPORT std::string type_to_string(value_type type);
NSCP_WHERE_EXPORT bool type_is_int(value_type type);
NSCP_WHERE_EXPORT bool type_is_float(value_type type);
NSCP_WHERE_EXPORT bool type_is_string(value_type type);
NSCP_WHERE_EXPORT value_type get_return_type(operators op, value_type type);
NSCP_WHERE_EXPORT std::string operator_to_string(operators const &identifier);
NSCP_WHERE_EXPORT bool is_comparison_operator(operators op);
NSCP_WHERE_EXPORT value_type infer_binary_type(const object_converter &converter, operators op, node_type &left, node_type &right);
NSCP_WHERE_EXPORT bool can_convert(value_type src, value_type dst);
NSCP_WHERE_EXPORT bool is_upper(operators op);
NSCP_WHERE_EXPORT bool is_lower(operators op);

typedef boost::tuple<long long, double, std::string> read_arg_type;
NSCP_WHERE_EXPORT read_arg_type read_arguments(const evaluation_context &context, const node_type &subject, const std::string &default_unit);
NSCP_WHERE_EXPORT node_type add_convert_node(node_type subject, value_type new_type);
}  // namespace helpers
}  // namespace where
}  // namespace parsers