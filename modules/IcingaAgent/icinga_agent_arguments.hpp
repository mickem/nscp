// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// A CheckCommand's `arguments` block, as the master sends it in
// `macros.ifw_api_arguments`, turned into NSClient++ query arguments.
//
// Each entry is either a plain value (`"warning" = "$warn$"`) or a dictionary
// with the attributes Icinga's command-line builder knows: value, set_if,
// order, skip_key, repeat_key, key and required. The agent applies them the
// way Icinga does when it builds a command line, but renders each argument as
// the `key=value` token NSClient++ queries take (leading dashes dropped:
// `--warning` becomes `warning=...`).

#include <boost/json.hpp>
#include <boost/optional.hpp>
#include <list>
#include <stdexcept>
#include <string>
#include <vector>

namespace icinga_agent {

// A required argument whose macro the master did not send.
struct argument_error : std::runtime_error {
  using std::runtime_error::runtime_error;
};

struct argument_spec {
  std::string key;         // without leading dashes; empty: value only
  long order = 0;          // Icinga sorts by order, then by key
  bool skip_key = false;   // pass the value without its key
  bool repeat_key = true;  // a list value: key=a key=b, rather than key=a,b
  bool required = false;   // a missing macro is an error, not a dropped argument
  boost::optional<boost::json::value> set_if;
  boost::optional<boost::json::value> value;  // none: a flag (key only)
};

// `--warning` -> `warning`.
std::string strip_dashes(const std::string &key);

// One entry of the arguments block.
argument_spec parse_argument_spec(const std::string &name, const boost::json::value &definition);

// Whether `set_if` lets the argument through (true when there is none). A
// set_if whose macro is missing counts as false.
bool argument_enabled(const argument_spec &spec, const boost::json::object &macros);

// The argument's values once expanded: one per list element, otherwise one.
// None when the argument is to be left out (a missing macro, an empty
// value); an empty vector for a flag. Throws argument_error when a required
// argument's macro is missing.
boost::optional<std::vector<std::string>> resolve_argument_values(const argument_spec &spec, const boost::json::object &macros);

// The tokens for one argument with its resolved values.
std::list<std::string> render_argument(const argument_spec &spec, const std::vector<std::string> &values);

// The whole block, in Icinga's order. Throws argument_error or
// macro_syntax_error.
std::list<std::string> build_arguments(const boost::json::object &arguments, const boost::json::object &macros);

}  // namespace icinga_agent
