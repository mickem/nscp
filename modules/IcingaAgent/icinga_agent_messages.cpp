// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "icinga_agent_messages.hpp"

#include "icinga_agent_netstring.hpp"

namespace json = boost::json;

namespace icinga_agent {

std::string make_message(const std::string &method, json::object params) {
  json::object msg;
  msg["jsonrpc"] = "2.0";
  msg["method"] = method;
  msg["params"] = std::move(params);
  return frame(json::serialize(msg));
}

std::string make_hello(const std::string &agent_version) {
  json::object p;
  // We understand the ifw-api argument convention. ExecuteArbitraryCommand is
  // deliberately not announced: the master's ad-hoc /v1/actions/execute-command
  // is never run.
  p["capabilities"] = kCapabilityIfwApiCheckCommand;
  p["version"] = kAnnouncedVersion;
  p["agentversion"] = agent_version;
  return make_message("icinga::Hello", std::move(p));
}

std::string make_heartbeat(const int timeout_seconds) {
  json::object p;
  p["timeout"] = timeout_seconds;
  return make_message("event::Heartbeat", std::move(p));
}

std::string make_log_position(const double unix_time) {
  json::object p;
  p["log_position"] = unix_time;
  return make_message("log::SetLogPosition", std::move(p));
}

std::string make_certificate_request(const std::string &ticket) {
  json::object p;
  if (!ticket.empty()) p["ticket"] = ticket;
  return make_message("pki::RequestCertificate", std::move(p));
}

json::object check_result_object(const check_result &result) {
  json::object cr;
  cr["type"] = "CheckResult";
  cr["exit_status"] = result.exit_status;
  cr["state"] = result.exit_status;
  cr["active"] = true;
  cr["output"] = result.output;
  cr["command"] = result.command_line;
  cr["check_source"] = result.check_source;
  cr["execution_start"] = result.execution_start;
  cr["execution_end"] = result.execution_end;
  cr["schedule_start"] = result.execution_start;
  cr["schedule_end"] = result.execution_end;
  json::array perf;
  for (const std::string &entry : split_perfdata(result.perfdata)) perf.emplace_back(entry);
  cr["performance_data"] = std::move(perf);
  return cr;
}

std::string make_check_result(const std::string &host, const std::string &service, const check_result &result) {
  json::object p;
  p["host"] = host;
  if (!service.empty()) p["service"] = service;
  p["cr"] = check_result_object(result);
  return make_message("event::CheckResult", std::move(p));
}

std::vector<std::string> split_perfdata(const std::string &perfdata) {
  std::vector<std::string> out;
  std::string current;
  bool quoted = false;
  for (const char c : perfdata) {
    if (c == '\'') quoted = !quoted;
    if (!quoted && (c == ' ' || c == '\n')) {
      if (!current.empty()) out.push_back(current);
      current.clear();
      continue;
    }
    current += c;
  }
  if (!current.empty()) out.push_back(current);
  return out;
}

}  // namespace icinga_agent
