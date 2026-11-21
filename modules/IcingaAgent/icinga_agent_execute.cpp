// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "icinga_agent_execute.hpp"

#include "icinga_agent_arguments.hpp"
#include "icinga_agent_json.hpp"

namespace json = boost::json;

namespace icinga_agent {

namespace {
json::object macros_of(const json::object &params) {
  const json::value *macros = json_util::find(params, "macros");
  return macros && macros->is_object() ? macros->get_object() : json::object();
}

double deadline_of(const json::object &params) {
  const json::value *deadline = json_util::find(params, "deadline");
  return deadline && deadline->is_number() ? deadline->to_number<double>() : 0;
}
}  // namespace

std::string query_name(const std::string &icinga_command, const json::object &macros) {
  const json::value *ifw_command = json_util::find(macros, "ifw_api_command");
  if (ifw_command && !json_util::is_empty(*ifw_command)) return json_util::to_text(*ifw_command);
  return icinga_command;
}

std::list<std::string> query_arguments(const json::object &macros) {
  const json::value *arguments = json_util::find(macros, "ifw_api_arguments");
  if (!arguments || !arguments->is_object()) return {};
  return build_arguments(arguments->get_object(), macros);
}

query_request parse_execute_command(const json::value &params_value) {
  query_request req;
  if (!params_value.is_object()) {
    req.error = "ExecuteCommand without params";
    return req;
  }
  const json::object &params = params_value.get_object();
  req.host = json_util::text_field(params, "host");
  req.service = json_util::text_field(params, "service");
  req.command_type = json_util::text_field(params, "command_type");
  req.icinga_command = json_util::text_field(params, "command");
  req.deadline = deadline_of(params);
  if (req.command_type != "check_command") {
    req.error = "Only check commands are supported, got '" + req.command_type + "'";
    return req;
  }
  const json::object macros = macros_of(params);
  req.query = query_name(req.icinga_command, macros);
  if (req.query.empty()) {
    req.error = "ExecuteCommand without a command";
    return req;
  }
  try {
    req.arguments = query_arguments(macros);
  } catch (const std::exception &e) {
    req.error = std::string("Failed to build arguments: ") + e.what();
  }
  return req;
}

}  // namespace icinga_agent
