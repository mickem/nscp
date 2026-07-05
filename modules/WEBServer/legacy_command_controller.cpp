// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "legacy_command_controller.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/json.hpp>
#include <client/simple_client.hpp>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/nagios.hpp>

namespace json = boost::json;

legacy_command_controller::legacy_command_controller(const std::shared_ptr<session_manager_interface> &session, const nscapi::core_wrapper *core,
                                                     unsigned int plugin_id, std::shared_ptr<client::cli_client> client)
    : RegexpController(""), session(session), core(core), plugin_id(plugin_id), client(client) {
  addRoute("GET", "/query/([^/]+)/?$", this, &legacy_command_controller::handle_query);
}

void legacy_command_controller::handle_query(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &http_response) {
  if (!session->is_logged_in("legacy", request, http_response)) return;

  if (what.size() != 2) {
    http_response.setCodeNotFound("Query not found");
    return;
  }
  std::string query = what.str(1);

  PB::Commands::QueryRequestMessage rm;
  PB::Commands::QueryRequestMessage::Request *payload = rm.add_payload();

  payload->set_command(query);
  Mongoose::Request::arg_vector args = request.getVariablesVector();

  for (const Mongoose::Request::arg_vector::value_type &e : args) {
    if (e.second.empty())
      payload->add_arguments(e.first);
    else
      payload->add_arguments(e.first + "=" + e.second);
  }

  // Stamp the caller identity onto the request so the core permission
  // layer can see who's asking. plugin_id is WEBServer; the principal is
  // the authenticated user from the session cookie. Same metadata keys
  // core_helper::simple_query_as uses, kept in lockstep with the
  // decision point in service/plugins/plugin_manager.cpp.
  {
    std::string user, token;
    session_manager_interface::get_user_from_response(http_response, user, token);
    auto *meta_plugin = rm.mutable_header()->add_metadata();
    meta_plugin->set_key("nscp.caller_plugin_id");
    meta_plugin->set_value(std::to_string(plugin_id));
    if (!user.empty()) {
      auto *meta_user = rm.mutable_header()->add_metadata();
      meta_user->set_key("nscp.principal");
      meta_user->set_value(user);
    }
  }

  std::string pb_response, json_response;
  core->query(rm.SerializeAsString(), pb_response);
  PB::Commands::QueryResponseMessage response;
  response.ParseFromString(pb_response);

  json::object node;
  for (const PB::Commands::QueryResponseMessage::Response &r : response.payload()) {
    node["command"] = r.command();
    if (r.result() == PB::Common::ResultCode::OK) {
      node["result"] = "OK";
    } else if (r.result() == PB::Common::ResultCode::WARNING) {
      node["result"] = "WARNING";
    } else if (r.result() == PB::Common::ResultCode::CRITICAL) {
      node["result"] = "CRITICAL";
    } else {
      node["result"] = "UNKNOWN";
    }
    json::array lines;
    for (const PB::Commands::QueryResponseMessage::Response::Line &l : r.lines()) {
      json::object line;
      line["message"] = l.message();

      json::array perfs;
      for (const PB::Common::PerformanceData &p : l.perf()) {
        json::object perf;
        perf["alias"] = p.alias();
        if (p.has_float_value()) {
          const auto &fv = p.float_value();
          json::object float_value;
          float_value["value"] = fv.value();
          if (fv.has_minimum()) float_value["minimum"] = fv.minimum().value();
          if (fv.has_maximum()) float_value["maximum"] = fv.maximum().value();
          // Prefer the original Nagios range syntax over the numeric
          // lower bound so the API doesn't silently drop "4:5"-style
          // thresholds (issue #748). Same shape as the v2 queries
          // endpoint in query_controller.cpp.
          if (!fv.warning_range().empty())
            float_value["warning"] = fv.warning_range();
          else if (fv.has_warning())
            float_value["warning"] = fv.warning().value();
          if (!fv.critical_range().empty())
            float_value["critical"] = fv.critical_range();
          else if (fv.has_critical())
            float_value["critical"] = fv.critical().value();
          float_value["unit"] = fv.unit();
          perf["float_value"] = float_value;
        }
        if (p.has_string_value()) {
          json::object string_value;
          string_value["value"] = p.string_value().value();
          perf["string_value"] = string_value;
        }
        perfs.push_back(perf);
      }
      line["perf"] = perfs;
      lines.push_back(line);
    }
    node["lines"] = lines;
    break;
  }
  json::object root;
  json::array payloads;
  payloads.push_back(node);
  root["payload"] = payloads;
  http_response.setCodeOk();
  http_response.append(json::serialize(root));
}