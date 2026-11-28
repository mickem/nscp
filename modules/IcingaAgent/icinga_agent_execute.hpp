// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// An `event::ExecuteCommand` turned into the NSClient++ query to run.

#include <boost/json.hpp>
#include <boost/optional.hpp>
#include <list>
#include <string>

#include "icinga_agent_translate.hpp"

namespace icinga_agent {

struct query_request {
  std::string host;
  std::string service;         // empty for a host check
  std::string command_type;    // "check_command" / "event_command"
  std::string icinga_command;  // params.command, the CheckCommand object name
  std::string query;           // the NSClient++ query to run
  std::list<std::string> arguments;
  // The arguments are an alias's own, with nothing from the master in them.
  bool fixed_arguments = false;
  double deadline = 0;  // 0 when the master did not send one
  std::string error;    // non-empty: do not run, answer UNKNOWN with this
};

// The query a check command names. Commands follow the `ifw-api` convention
// (Icinga 2.14+, used by Icinga for Windows): `macros.ifw_api_command` names
// the query. Without it the CheckCommand object name is the query, so a plain
// `object CheckCommand "check_cpu"` keeps working for argument-less checks.
std::string query_name(const std::string &icinga_command, const boost::json::object &macros);

// The arguments from `macros.ifw_api_arguments`; empty without it.
// Throws argument_error or macro_syntax_error.
std::list<std::string> query_arguments(const boost::json::object &macros);

// Resolve an `event::ExecuteCommand` params object. Never throws: anything
// that keeps the check from running is reported in `error`.
//
// A command that follows the `ifw-api` convention says itself which query it
// is. Any other is looked up in `translation` (aliases, then the built-in
// translations of the ITL commands) and, when it is not there either, runs as
// the query of its own name without arguments.
query_request parse_execute_command(const boost::json::value &params, const translation_config &translation);

// Without aliases or built-in translations.
query_request parse_execute_command(const boost::json::value &params);

}  // namespace icinga_agent
