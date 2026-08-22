// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <list>
#include <parsers/where/node.hpp>
#include <str/format.hpp>
#include <string>

// Formatting and scaling functions for filter expressions.
//
// The filter grammar has comparisons and boolean operators but no arithmetic,
// so a check whose keywords are raw byte counts cannot render or threshold
// them in any other unit from configuration alone: `detail-syntax` can only
// print `21967407`. These functions close that gap and are registered by any
// check with byte- or rate-valued keywords via register_format_functions().
//
// Usage examples (all valid in both filter expressions and detail-syntax):
//   detail-syntax = "%(name): %(format_bytes(total_bytes_per_sec))/s"
//   detail-syntax = "%(format_bytes(used, 'GB')) used"
//   detail-syntax = "%(format_bytes(used, 'GB', 1)) GB used"
//   detail-syntax = "%(format_number(used_pct, 1))% used"
//   warning       = "convert_bytes(write_bytes_per_sec, 'MB') > 100"
//   detail-syntax = "%(scale(value, 1000000)) Mbps"
//
// Everything rendered here follows the check's number format (the `decimals`,
// `byte-unit`, `decimal-separator` and `thousands-separator` options), so a
// check configured for two decimals and a decimal comma keeps them here too;
// an explicit `decimals` argument wins over the option.
//
// Originally added for check_pdh (issue #281), generalised in #1392 and given
// configurable precision and separators in #1428.

namespace parsers {
namespace where {
namespace format_functions {

// True when `node` is an optional number that currently has no value (e.g.
// check_disk_health's space keywords on a row with no filesystem).
//
// Such a value must survive being passed through one of these functions
// untouched: reading it as a number yields the neutral 0 the container
// carries for legacy callers, which would render as `0B` and make
// `convert_bytes(free,'GB') < 10` true on a disk that has no free space to
// speak of. Handing the original node back instead keeps both halves of the
// contract - it renders as the variable's no-value string, and every numeric
// comparison against it stays sure-false.
inline bool has_no_value(const evaluation_context &context, const node_type &node, const value_type numeric_type) {
  return node->get_value(context, numeric_type).is_no_value;
}

// Read the optional trailing `decimals` argument of a format function.
inline str::number_format with_decimals(const str::number_format &base, const evaluation_context &context, const node_type &node) {
  str::number_format fmt = base;
  fmt.decimals = static_cast<int>(node->get_int_value(context));
  if (fmt.decimals < 0) throw std::invalid_argument("decimals must not be negative");
  return fmt;
}

// Format a number as a human-readable byte string: format_bytes(value) auto-
// scales and appends the unit, format_bytes(value, unit) pins the unit and
// leaves it out of the result (the syntax string spells it out), and a third
// argument overrides the number of decimals.
inline node_type fn_format_bytes(value_type, evaluation_context context, const node_type subject) {
  try {
    const std::list<node_type> args = subject->get_list_value(context);
    if (args.empty() || args.size() > 3) {
      context->error("format_bytes expects 1 to 3 arguments: format_bytes(value), format_bytes(value, unit) or format_bytes(value, unit, decimals)");
      return factory::create_false();
    }
    auto it = args.begin();
    if (has_no_value(context, *it, type_int)) return *it;
    const long long v = (*it)->get_int_value(context);
    str::number_format fmt = context->get_number_format();
    if (args.size() == 1) {
      return factory::create_string(str::format::format_byte_units(v, fmt));
    }
    ++it;
    const std::string unit = (*it)->get_string_value(context);
    if (args.size() == 3) {
      ++it;
      fmt = with_decimals(fmt, context, *it);
    }
    return factory::create_string(str::format::format_byte_units(v, unit, fmt));
  } catch (const std::exception &e) {
    context->error(std::string("format_bytes failed: ") + e.what());
    return factory::create_false();
  }
}

// Render any number with a fixed number of decimals, honouring the check's
// separators: format_number(used_pct, 1) turns 13.9333 into "13.9".
inline node_type fn_format_number(value_type, evaluation_context context, const node_type subject) {
  try {
    const std::list<node_type> args = subject->get_list_value(context);
    if (args.empty() || args.size() > 2) {
      context->error("format_number expects 1 or 2 arguments: format_number(value) or format_number(value, decimals)");
      return factory::create_false();
    }
    auto it = args.begin();
    if (has_no_value(context, *it, type_float)) return *it;
    const double v = (*it)->get_float_value(context);
    str::number_format fmt = context->get_number_format();
    if (args.size() == 2) {
      ++it;
      fmt = with_decimals(fmt, context, *it);
    }
    return factory::create_string(str::render_number(v, fmt));
  } catch (const std::exception &e) {
    context->error(std::string("format_number failed: ") + e.what());
    return factory::create_false();
  }
}

// Convert a byte count to a given unit and return it as a number, so it can be
// compared in a threshold.
inline node_type fn_convert_bytes(value_type, evaluation_context context, const node_type subject) {
  try {
    const std::list<node_type> args = subject->get_list_value(context);
    if (args.size() != 2) {
      context->error("convert_bytes expects 2 arguments: convert_bytes(value, unit)");
      return factory::create_false();
    }
    auto it = args.begin();
    if (has_no_value(context, *it, type_int)) return *it;
    const long long v = (*it)->get_int_value(context);
    ++it;
    const std::string unit = (*it)->get_string_value(context);
    return factory::create_float(str::format::convert_to_byte_units(v, unit));
  } catch (const std::exception &e) {
    context->error(std::string("convert_bytes failed: ") + e.what());
    return factory::create_false();
  }
}

// Divide by an arbitrary divisor, for units the byte helpers do not cover
// (decimal Mbps, thousands of operations, ...).
inline node_type fn_scale(value_type, evaluation_context context, const node_type subject) {
  try {
    const std::list<node_type> args = subject->get_list_value(context);
    if (args.size() != 2) {
      context->error("scale expects 2 arguments: scale(value, divisor)");
      return factory::create_false();
    }
    auto it = args.begin();
    if (has_no_value(context, *it, type_float)) return *it;
    const double v = (*it)->get_float_value(context);
    ++it;
    const double divisor = (*it)->get_float_value(context);
    if (divisor == 0.0) {
      context->error("scale: divisor must be non-zero");
      return factory::create_false();
    }
    return factory::create_float(v / divisor);
  } catch (const std::exception &e) {
    context->error(std::string("scale failed: ") + e.what());
    return factory::create_false();
  }
}

// Register format_bytes / convert_bytes / scale on a filter registry.
template <class TRegistry>
void register_format_functions(TRegistry &registry) {
  registry
      .add_string_fun("format_bytes", &fn_format_bytes,
                      "Format a number as a human-readable byte string.\n"
                      "Syntax: format_bytes(value) auto-scales to B/KB/MB/GB/... (1024-based) and appends the unit.\n"
                      "        format_bytes(value, unit) formats to a fixed unit (\"B\", \"K\"/\"KB\", \"M\"/\"MB\", \"G\"/\"GB\", \"T\"/\"TB\") "
                      "and does not append it; an empty unit renders the raw byte count.\n"
                      "        format_bytes(value, unit, decimals) does the same with a fixed number of decimals.\n"
                      "Without an explicit decimals argument the decimals, byte-unit, decimal-separator and thousands-separator options apply.")
      .add_string_fun("format_number", &fn_format_number,
                      "Render a number with a fixed number of decimals, using the check's decimal and thousands separators.\n"
                      "Syntax: format_number(value) uses the decimals option; format_number(value, decimals) overrides it.\n"
                      "Useful for percentages and other non-byte values: format_number(used_pct, 1).")
      .add_custom_fun("convert_bytes", parsers::where::type_float, &fn_convert_bytes,
                      "Convert a byte count to a specific unit and return the numeric value (1024-based). Useful in thresholds.\n"
                      "Syntax: convert_bytes(value, unit) where unit is \"B\", \"K\"/\"KB\", \"M\"/\"MB\", \"G\"/\"GB\", \"T\"/\"TB\".")
      .add_custom_fun("scale", parsers::where::type_float, &fn_scale,
                      "Divide a value by a divisor. Useful for arbitrary unit conversions (e.g. decimal Mbps with scale(value, 1000000)).\n"
                      "Syntax: scale(value, divisor).");
}

}  // namespace format_functions
}  // namespace where
}  // namespace parsers
