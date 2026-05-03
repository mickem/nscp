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
#include <parsers/where/engine.hpp>
#include <str/format.hpp>

namespace parsers {
namespace where {

bool engine_filter::validate(error_handler error, object_factory context, bool perf_collection, parsers::where::performance_collector &boundries) {
  if (error->is_debug()) error->log_debug("Parsing: " + filter_string);

  if (!ast_parser.parse(context, filter_string)) {
    error->log_error("Parsing failed of '" + filter_string + "' at: " + ast_parser.rest);
    return false;
  }
  if (error->is_debug()) error->log_debug("Parsing succeeded: " + ast_parser.result_as_tree());

  if (!ast_parser.derive_types(context) || context->has_error()) {
    error->log_error("Invalid types: " + context->get_error());
    return false;
  }
  if (error->is_debug()) error->log_debug("Type resolution succeeded: " + ast_parser.result_as_tree());

  if (!ast_parser.bind(context) || context->has_error()) {
    error->log_error("Variable and function binding failed: " + context->get_error());
    return false;
  }
  if (error->is_debug()) error->log_debug("Binding succeeded: " + ast_parser.result_as_tree());

  if (!ast_parser.static_eval(context) || context->has_error()) {
    error->log_error("Static evaluation failed: " + context->get_error());
    return false;
  }
  if (error->is_debug()) error->log_debug("Static evaluation succeeded: " + ast_parser.result_as_tree());

  if (perf_collection) {
    if (!ast_parser.collect_perfkeys(context, boundries) || context->has_error()) {
      error->log_error("Collection of perfkeys failed: " + context->get_error());
      return false;
    }
  }
  return true;
}

bool engine_filter::require_object(execution_context_type context) {
  if (requires_object) return *requires_object;
  requires_object = ast_parser.require_object(context);
  return *requires_object;
}

bool engine_filter::match(error_handler error, execution_context_type context, bool expect_object) {
  if (expect_object && !require_object(context)) return false;
  if (!expect_object && require_object(context)) return false;
  value_container v = ast_parser.evaluate(context);
  if (context->has_error()) {
    error->log_error(context->get_error() + ": " + ast_parser.result_as_tree(context));
  }
  if (context->has_warn()) {
    error->log_warning(context->get_warn() + ": " + ast_parser.result_as_tree(context));
  }
  context->clear();
  if (v.is_unsure) {
    error->log_warning("Ignoring unsure result: " + ast_parser.result_as_tree(context));
  }
  return v.is_true();
}

force_match_result engine_filter::match_force(error_handler error, execution_context_type context) {
  // Like match() but without the require_object/expect_object guard. Used by
  // modern_filter::match_post when the iteration produced no matched rows so
  // that mixed expressions (e.g. `state='stopped' OR count=0`) are still
  // evaluated. Object-bound variables resolve to a default (false) value when
  // no object is present; summary variables resolve to their final values.
  // Returns the AST's truth value plus the value_container::is_unsure flag,
  // so the caller can distinguish a sure verdict from "object-bound subterms
  // could not fully resolve". The unsure case lets modern_filter surface
  // UNKNOWN rather than silently treating the result as a sure verdict.
  value_container v = ast_parser.evaluate(context);
  if (context->has_error()) {
    error->log_error(context->get_error() + ": " + ast_parser.result_as_tree(context));
  }
  // Suppress warnings here: the lack of an object is intentional in this
  // mode, so per-variable "no object instance" warnings are noise.
  context->clear();
  return {v.is_true(), v.is_unsure};
}

std::string engine_filter::to_string() const { return filter_string; }

engine::engine(std::vector<std::string> filter, error_handler error) : error(error) {
  for (const std::string &s : filter) {
    filters_.push_back(engine_filter(s));
  }
}

engine::boundries_type engine::fetch_performance_data() { return boundries.get_candidates(); }

void engine::enabled_performance_collection() { perf_collection = true; }

bool engine::validate(object_factory context) {
  for (engine_filter &f : filters_) {
    if (!f.validate(error, context, perf_collection, boundries)) return false;
  }
  return true;
}

bool engine::match(execution_context_type context, bool expect_object) {
  for (engine_filter &f : filters_) {
    if (f.match(error, context, expect_object)) return true;
  }
  return false;
}

force_match_result engine::match_force(execution_context_type context) {
  // Any-of semantics: a sure-true filter short-circuits and wins. If no filter
  // is sure-true but at least one was unsure, return matched=false with
  // is_unsure=true so the caller can surface UNKNOWN.
  bool any_unsure = false;
  for (engine_filter &f : filters_) {
    const force_match_result r = f.match_force(error, context);
    if (r.matched && !r.is_unsure) return {true, false};
    if (r.is_unsure) any_unsure = true;
  }
  return {false, any_unsure};
}

std::string engine::to_string() const {
  std::string ret = "";
  for (const engine_filter &f : filters_) {
    str::format::append_list(ret, f.to_string(), ", ");
  }
  return ret;
}

}  // namespace where
}  // namespace parsers