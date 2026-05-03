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

#include <parsers/where.hpp>
#include <parsers/where/dll_defines.hpp>
#include <parsers/where/node.hpp>
#include <vector>
#ifdef WIN32
#pragma warning(push)
#pragma warning(disable : 4251)
#pragma warning(disable : 4275)
#endif

namespace parsers {
namespace where {
class NSCAPI_EXPORT error_handler_interface {
 public:
  virtual ~error_handler_interface() = default;
  virtual void log_error(std::string error) = 0;
  virtual void log_warning(std::string error) = 0;
  virtual void log_debug(std::string error) = 0;
  virtual bool is_debug() const = 0;
  virtual void set_debug(bool debug) = 0;
};

// Result of force-evaluating a filter without the require_object guard. The
// `is_unsure` flag carries the value_container::is_unsure bit propagated by
// object-bound subterms that could not fully resolve (e.g. a string variable
// evaluated with no current object). Callers in modern_filter::match_post
// use it to surface UNKNOWN instead of silently treating an unresolvable
// expression as OK or as a sure verdict.
struct NSCAPI_EXPORT force_match_result {
  bool matched = false;
  bool is_unsure = false;
};

struct NSCAPI_EXPORT engine_filter {
  typedef std::shared_ptr<error_handler_interface> error_handler;
  typedef evaluation_context execution_context_type;
  parser ast_parser;
  std::string filter_string;
  boost::optional<bool> requires_object;

  explicit engine_filter(const std::string filter_string) : filter_string(filter_string) {}

  bool validate(error_handler error, object_factory context, bool perf_collection, parsers::where::performance_collector &boundries);

  bool require_object(execution_context_type context);

  bool match(error_handler error, execution_context_type context, bool expect_object);

  // Force-evaluate this filter regardless of the require_object guard. This
  // is used for the "no rows matched" path in modern_filter, where we want
  // to evaluate mixed expressions like `state='stopped' OR count=0` even when
  // there is no current object - object-bound variables resolve to a default
  // (false) value, while summary variables resolve to their final values.
  // The returned `is_unsure` is set when any object-bound subterm contributed
  // a value_container::is_unsure flag — it lets the caller distinguish a
  // sure verdict from an "I could not fully evaluate this" outcome.
  force_match_result match_force(error_handler error, execution_context_type context);

  std::string to_string() const;
};

struct NSCAPI_EXPORT engine {
  typedef std::shared_ptr<error_handler_interface> error_handler;
  typedef evaluation_context execution_context_type;

  std::list<engine_filter> filters_;
  bool perf_collection = false;
  typedef performance_collector::boundaries_type boundries_type;
  performance_collector boundries;
  error_handler error;
  boost::optional<bool> requires_object;

  engine(std::vector<std::string> filter, error_handler error);

  boundries_type fetch_performance_data();

  void enabled_performance_collection();

  bool validate(object_factory context);

  bool match(execution_context_type context, bool expect_object);

  // Force-evaluate every filter in this engine regardless of the
  // require_object guard. See engine_filter::match_force. Any-of semantics:
  // a sure-true filter short-circuits and returns matched=true, is_unsure=false.
  // If no filter is sure-true but at least one was unsure, the result is
  // matched=false, is_unsure=true (so the caller can surface UNKNOWN).
  force_match_result match_force(execution_context_type context);

  std::string get_subject() { return "TODO"; }

  std::string to_string() const;
};
}  // namespace where
}  // namespace parsers
#ifdef WIN32
#pragma warning(pop)
#endif
