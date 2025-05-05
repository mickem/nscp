#include "legacy_command_controller.hpp"

#include "error_handler_interface.hpp"
#include "web_cli_handler.hpp"

#include <client/simple_client.hpp>

#include <nscapi/nscapi_protobuf_command.hpp>
#include <nscapi/nscapi_protobuf_settings.hpp>

#include <str/xtos.hpp>

#include <json_spirit.h>

#include <boost/asio/ip/address.hpp>
#include <boost/thread/locks.hpp>
#include <boost/algorithm/string.hpp>

legacy_command_controller::legacy_command_controller(boost::shared_ptr<session_manager_interface> session, nscapi::core_wrapper *core, unsigned int plugin_id,
                                                     boost::shared_ptr<client::cli_client> client)
    : session(session), core(core), plugin_id(plugin_id), client(client), RegexpController("") {
  addRoute("GET", "/query/([^/]+)/?$", this, &legacy_command_controller::handle_query);
}

void legacy_command_controller::handle_query(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &http_response) {
  if (!session->is_loggedin("legacy", request, http_response)) return;

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

  std::string pb_response, json_response;
  core->query(rm.SerializeAsString(), pb_response);
  PB::Commands::QueryResponseMessage response;
  response.ParseFromString(pb_response);

  json_spirit::Object node;
  for (const PB::Commands::QueryResponseMessage::Response &r : response.payload()) {
    node["command"] = r.command();
    node["result"] = "OK";
    json_spirit::Array lines;
    for (const PB::Commands::QueryResponseMessage::Response::Line &l : r.lines()) {
      json_spirit::Object line;
      line["message"] = l.message();

      json_spirit::Array perfs;
      for (const PB::Common::PerformanceData &p : l.perf()) {
        json_spirit::Object perf;
        perf["alias"] = p.alias();
        if (p.has_float_value()) {
          json_spirit::Object float_value;
          float_value["value"] = p.float_value().value();
          if (p.float_value().has_minimum()) float_value["minimum"] = p.float_value().minimum().value();
          if (p.float_value().has_maximum()) float_value["maximum"] = p.float_value().maximum().value();
          if (p.float_value().has_warning()) float_value["warning"] = p.float_value().warning().value();
          if (p.float_value().has_critical()) float_value["critical"] = p.float_value().critical().value();
          float_value["unit"] = p.float_value().unit();
          perf["float_value"] = float_value;
        }
        if (p.has_string_value()) {
          json_spirit::Object string_value;
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
  json_spirit::Object root;
  json_spirit::Array payloads;
  payloads.push_back(node);
  root["payload"] = payloads;
  http_response.setCodeOk();
  http_response.append(json_spirit::write(root));
}