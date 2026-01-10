#include "query_controller.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/json.hpp>
#include <boost/regex.hpp>
#include <nscapi/nscapi_helper.hpp>
#include <nscapi/nscapi_protobuf_command.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_protobuf_registry.hpp>
#include <str/xtos.hpp>

#include "helpers.hpp"

namespace json = boost::json;

query_controller::query_controller(const int version, const boost::shared_ptr<session_manager_interface> &session, const nscapi::core_wrapper *core,
                                   unsigned int plugin_id)
    : RegexpController(version == 1 ? "/api/v1/queries" : "/api/v2/queries"), session(session), core(core), plugin_id(plugin_id) {
  addRoute("GET", "/?$", this, &query_controller::get_queries);
  addRoute("GET", "/([^/]+)/?$", this, &query_controller::get_query);
  addRoute("GET", "/([^/]+)/commands/([^/]*)/?$", this, &query_controller::query_command);
}

void query_controller::get_queries(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("queries.list", request, response)) return;

  std::string fetch_all = request.get("all", "true");
  PB::Registry::RegistryRequestMessage rrm;
  PB::Registry::RegistryRequestMessage::Request *payload = rrm.add_payload();
  payload->mutable_inventory()->set_fetch_all(fetch_all == "true");
  payload->mutable_inventory()->add_type(PB::Registry::ItemType::QUERY);
  std::string str_response;
  core->registry_query(rrm.SerializeAsString(), str_response);

  PB::Registry::RegistryResponseMessage pb_response;
  pb_response.ParseFromString(str_response);
  json::array root;

  for (const PB::Registry::RegistryResponseMessage::Response r : pb_response.payload()) {
    for (const PB::Registry::RegistryResponseMessage::Response::Inventory i : r.inventory()) {
      json::object node;
      node["name"] = i.name();
      if (i.info().plugin_size() > 0) {
        node["plugin"] = i.info().plugin(0);
      }
      node["query_url"] = get_base(request) + "/" + i.name() + "/";
      node["title"] = i.info().title();
      json::object keys;
      for (const PB::Common::KeyValue &kvp : i.info().metadata()) {
        keys[kvp.key()] = kvp.value();
      }
      node["metadata"] = keys;
      node["description"] = i.info().description();
      root.push_back(node);
    }
  }
  response.append(json::serialize(root));
}

void query_controller::get_query(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("queries.get", request, response)) return;

  if (what.size() != 2) {
    response.setCodeNotFound("Query not found");
  }
  std::string module = what.str(1);

  PB::Registry::RegistryRequestMessage rrm;
  PB::Registry::RegistryRequestMessage::Request *payload = rrm.add_payload();
  payload->mutable_inventory()->set_name(module);
  payload->mutable_inventory()->set_fetch_all(false);
  payload->mutable_inventory()->add_type(PB::Registry::ItemType::QUERY);
  std::string str_response;
  core->registry_query(rrm.SerializeAsString(), str_response);

  PB::Registry::RegistryResponseMessage pb_response;
  pb_response.ParseFromString(str_response);
  json::object node;

  for (const PB::Registry::RegistryResponseMessage::Response r : pb_response.payload()) {
    for (const PB::Registry::RegistryResponseMessage::Response::Inventory i : r.inventory()) {
      node["name"] = i.name();
      if (i.info().plugin_size() > 0) {
        node["plugin"] = i.info().plugin(0);
      }
      node["title"] = i.info().title();
      node["execute_url"] = get_base(request) + "/" + i.name() + "/commands/execute";
      node["execute_nagios_url"] = get_base(request) + "/" + i.name() + "/commands/execute_nagios";
      json::object keys;
      for (const PB::Common::KeyValue &kvp : i.info().metadata()) {
        keys[kvp.key()] = kvp.value();
      }
      node["metadata"] = keys;
      node["description"] = i.info().description();
    }
  }
  response.setCodeOk();
  response.append(json::serialize(node));
}

void query_controller::query_command(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("queries.execute", request, response)) return;

  if (what.size() != 3) {
    response.setCodeNotFound("Invalid request");
  }
  const std::string module = what.str(1);
  const std::string command = what.str(2);

  if (command == "execute") {
    if (request.readHeader("Accept") == "text/plain") {
      execute_query_text(module, request.getVariablesVector(), response);
    } else {
      execute_query(module, request.getVariablesVector(), response);
    }
  } else if (command == "execute_nagios") {
    if (request.readHeader("Accept") == "text/plain") {
      execute_query_text(module, request.getVariablesVector(), response);
    } else {
      execute_query_nagios(module, request.getVariablesVector(), response);
    }
  } else {
    response.setCodeNotFound("unknown command: " + command);
  }
}

void query_controller::execute_query(std::string module, arg_vector args, Mongoose::StreamResponse &http_response) {
  PB::Commands::QueryRequestMessage qrm;
  PB::Commands::QueryRequestMessage::Request *payload = qrm.add_payload();

  payload->set_command(module);
  for (const Mongoose::Request::arg_vector::value_type &e : args) {
    if (e.second.empty())
      payload->add_arguments(e.first);
    else
      payload->add_arguments(e.first + "=" + e.second);
  }
  std::string pb_response, json_response;
  core->query(qrm.SerializeAsString(), pb_response);
  PB::Commands::QueryResponseMessage response;
  response.ParseFromString(pb_response);

  json::object node;
  for (const PB::Commands::QueryResponseMessage::Response &r : response.payload()) {
    node["command"] = r.command();
    node["result"] = nscapi::protobuf::functions::gbp_to_nagios_status(r.result());
    json::array lines;
    for (const PB::Commands::QueryResponseMessage::Response::Line &l : r.lines()) {
      json::object line;
      line["message"] = l.message();

      json::object perf;
      for (const PB::Common::PerformanceData &p : l.perf()) {
        json::object pdata;

        if (p.has_float_value()) {
          pdata["value"] = p.float_value().value();
          if (p.float_value().has_minimum()) pdata["minimum"] = p.float_value().minimum().value();
          if (p.float_value().has_maximum()) pdata["maximum"] = p.float_value().maximum().value();
          if (p.float_value().has_warning()) pdata["warning"] = p.float_value().warning().value();
          if (p.float_value().has_critical()) pdata["critical"] = p.float_value().critical().value();
          pdata["unit"] = p.float_value().unit();
        }
        if (p.has_string_value()) {
          pdata["value"] = p.string_value().value();
        }
        perf[p.alias()] = pdata;
      }
      line["perf"] = perf;
      lines.push_back(line);
    }
    node["lines"] = lines;
    break;
  }
  http_response.setCodeOk();
  http_response.append(json::serialize(node));
}

void query_controller::execute_query_nagios(std::string module, arg_vector args, Mongoose::StreamResponse &http_response) {
  PB::Commands::QueryRequestMessage qrm;
  PB::Commands::QueryRequestMessage::Request *payload = qrm.add_payload();

  payload->set_command(module);
  for (const Mongoose::Request::arg_vector::value_type &e : args) {
    if (e.second.empty())
      payload->add_arguments(e.first);
    else
      payload->add_arguments(e.first + "=" + e.second);
  }
  std::string pb_response, json_response;
  core->query(qrm.SerializeAsString(), pb_response);
  PB::Commands::QueryResponseMessage response;
  response.ParseFromString(pb_response);

  json::object node;
  for (const PB::Commands::QueryResponseMessage::Response &r : response.payload()) {
    node["command"] = r.command();
    node["result"] = nscapi::plugin_helper::translateReturn(r.result());
    json::array lines;
    for (const PB::Commands::QueryResponseMessage::Response::Line &l : r.lines()) {
      json::object line;
      line["message"] = l.message();
      line["perf"] = nscapi::protobuf::functions::build_performance_data(l, nscapi::protobuf::functions::no_truncation);
      lines.push_back(line);
    }
    node["lines"] = lines;
    break;
  }
  http_response.setCodeOk();
  http_response.append(json::serialize(node));
}

void query_controller::execute_query_text(std::string module, arg_vector args, Mongoose::StreamResponse &http_response) {
  PB::Commands::QueryRequestMessage qrm;
  PB::Commands::QueryRequestMessage::Request *payload = qrm.add_payload();

  payload->set_command(module);
  for (const Mongoose::Request::arg_vector::value_type &e : args) {
    if (e.second.empty())
      payload->add_arguments(e.first);
    else
      payload->add_arguments(e.first + "=" + e.second);
  }
  std::string pb_response, json_response;
  core->query(qrm.SerializeAsString(), pb_response);
  PB::Commands::QueryResponseMessage response;
  response.ParseFromString(pb_response);

  int code = 200;
  std::string reason = "Ok";
  for (const PB::Commands::QueryResponseMessage::Response &r : response.payload()) {
    if (r.result() == PB::Common::ResultCode::CRITICAL) {
      code = HTTP_SERVER_ERROR;
      reason = "Critical";
    } else if (r.result() == PB::Common::ResultCode::UNKNOWN) {
      code = 503;
      reason = "Unknown";
    } else if (r.result() == PB::Common::ResultCode::WARNING) {
      code = 202;
      reason = "Warning";
    }
    for (const PB::Commands::QueryResponseMessage::Response::Line &l : r.lines()) {
      http_response.append(l.message());
      if (l.perf_size() > 0) {
        http_response.append("|" + nscapi::protobuf::functions::build_performance_data(l, nscapi::protobuf::functions::no_truncation));
      }
      http_response.append("\n");
    }
  }
  http_response.setHeader("Content-Type", "text/plain");
  http_response.setCode(code, reason);
}
