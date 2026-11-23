// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// Icinga macro expansion over the `macros` dictionary of an
// event::ExecuteCommand: the master resolves every macro a CheckCommand's
// arguments reference and sends the values; the agent substitutes them.
//
// The rules follow Icinga's MacroProcessor for the parts an agent sees:
// `$name$` is a macro, `$$` an escaped dollar, a string that is exactly one
// macro keeps the macro's JSON type, and an array macro inside a longer
// string is joined with commas.

#include <boost/json.hpp>
#include <boost/optional.hpp>
#include <stdexcept>
#include <string>

namespace icinga_agent {

// A string Icinga could not have produced: a `$` without its closing `$`.
struct macro_syntax_error : std::runtime_error {
  using std::runtime_error::runtime_error;
};

// The name when `text` is exactly one macro (`$name$`), none otherwise.
boost::optional<std::string> sole_macro_name(const std::string &text);

// A macro value as it reads inside a longer string: arrays joined with ",".
std::string macro_value_text(const boost::json::value &value);

// macro_value_text() of the macro `name`; empty when the master did not send it.
std::string macro_text(const boost::json::object &macros, const char *name);

// Expand every macro in `text`. None when a macro is not in `macros`: Icinga
// drops an argument whose macro is missing, and the callers do the same.
// Throws macro_syntax_error on an unterminated macro.
boost::optional<boost::json::value> expand_macros(const std::string &text, const boost::json::object &macros);

// expand_macros() over a JSON value: strings are expanded, arrays element by
// element (none if any element is missing), anything else is returned as is.
boost::optional<boost::json::value> expand_value(const boost::json::value &value, const boost::json::object &macros);

}  // namespace icinga_agent
