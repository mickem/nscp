// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "query_controller.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/json.hpp>
#include <boost/regex.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper.hpp>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/functions_convert.hpp>
#include <nscapi/protobuf/functions_copy.hpp>
#include <nscapi/protobuf/functions_exec.hpp>
#include <nscapi/protobuf/functions_perfdata.hpp>
#include <nscapi/protobuf/functions_submit.hpp>
#include <nscapi/protobuf/registry.hpp>
#include <str/xtos.hpp>

#include "helpers.hpp"

namespace json = boost::json;

query_controller::query_controller(const int version, const std::shared_ptr<session_manager_interface> &session, const nscapi::core_wrapper *core,
                                   unsigned int plugin_id)
    : RegexpController(version == 1 ? "/api/v1/queries" : "/api/v2/queries"), session(session), core(core), plugin_id(plugin_id) {
  addRoute("GET", "/?$", this, &query_controller::get_queries);
  addRoute("GET", "/([^/]+)/?$", this, &query_controller::get_query);
  addRoute("GET", "/([^/]+)/help/?$", this, &query_controller::get_query_help);
  addRoute("GET", "/([^/]+)/commands/([^/]*)/?$", this, &query_controller::query_command);
}

void query_controller::get_queries(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("queries.list", request, response)) return;

  PB::Registry::RegistryRequestMessage rrm;
  PB::Registry::RegistryRequestMessage::Request *payload = rrm.add_payload();
  // Never fetch_all here, and the `all` query parameter is deliberately
  // ignored. For a QUERY inventory that flag makes the core run *every*
  // registered command with `help-pb` to collect its parameters
  // (registry_query_handler::inventory_queries), and this endpoint does not
  // emit parameters at all - the JSON below is byte-identical either way.
  //
  // It used to default to true, so listing the queries executed every check on
  // the box: measured at 6.4s against 0.15s with a dozen modules loaded (92
  // commands), and it held a WEB server thread for all of it. That is what made
  // opening the Queries page - or loading a module, after which the UI
  // refreshes the list - freeze the whole web UI, static assets included.
  //
  // A caller that wants a command's parameters asks for that one command:
  // /queries/<name>/help, which fetches exactly it.
  payload->mutable_inventory()->set_fetch_all(false);
  payload->mutable_inventory()->add_type(PB::Registry::ItemType::QUERY);
  std::string str_response;
  core->registry_query(rrm.SerializeAsString(), str_response);

  PB::Registry::RegistryResponseMessage pb_response;
  pb_response.ParseFromString(str_response);
  json::array root;

  for (const PB::Registry::RegistryResponseMessage::Response &r : pb_response.payload()) {
    for (const PB::Registry::RegistryResponseMessage::Response::Inventory &i : r.inventory()) {
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
    return;
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

  for (const PB::Registry::RegistryResponseMessage::Response &r : pb_response.payload()) {
    for (const PB::Registry::RegistryResponseMessage::Response::Inventory &i : r.inventory()) {
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

namespace {
// The command an alias stands for, dug out of its description - the registry
// records it nowhere else. Mirrors alias_target() in client/simple_client.cpp;
// the two must agree, or the web prompt would offer a different keyword list
// from the one the interactive console offers for the same name.
std::string alias_target(const std::string &description) {
  static const char *prefixes[] = {"Alias for: ", "Alternative name for: "};
  for (const char *prefix : prefixes) {
    const std::size_t len = std::string(prefix).size();
    if (description.compare(0, len, prefix) == 0) return boost::algorithm::trim_copy(description.substr(len));
  }
  return "";
}

// `help-pb` for one command: the parameters it accepts and the filter
// keywords it offers. `found` distinguishes "no such command" from "a command
// that declares nothing", which the caller reports differently.
bool fetch_help(const nscapi::core_wrapper *core, const std::string &command, PB::Registry::ParameterDetails &details, std::string &description) {
  PB::Registry::RegistryRequestMessage rrm;
  PB::Registry::RegistryRequestMessage::Request *payload = rrm.add_payload();
  payload->mutable_inventory()->set_name(command);
  // With a name set this is what makes the core ask the module for its help,
  // which is where the parameters and the filter keywords come from.
  payload->mutable_inventory()->set_fetch_all(true);
  payload->mutable_inventory()->add_type(PB::Registry::ItemType::QUERY);
  std::string str_response;
  core->registry_query(rrm.SerializeAsString(), str_response);

  PB::Registry::RegistryResponseMessage pb_response;
  pb_response.ParseFromString(str_response);
  bool found = false;
  for (const PB::Registry::RegistryResponseMessage::Response &r : pb_response.payload()) {
    for (const PB::Registry::RegistryResponseMessage::Response::Inventory &i : r.inventory()) {
      if (i.name() != command) continue;
      found = true;
      details = i.parameters();
      description = i.info().description();
    }
  }
  return found;
}

const char *content_type_name(const PB::Common::DataType type) {
  switch (type) {
    case PB::Common::INT:
      return "int";
    case PB::Common::STRING:
      return "string";
    case PB::Common::FLOAT:
      return "float";
    case PB::Common::BOOL:
      return "bool";
    case PB::Common::LIST:
      return "list";
    default:
      return "";
  }
}
}  // namespace

// The vocabulary of one query: every option it takes and every filter keyword
// it offers, with the descriptions the module wrote for them. This is what the
// interactive console reads to highlight and complete a command line
// (`desc`/`keywords` in client/simple_client.cpp); the web UI reads it for the
// same reason, so a filter can be written against what the check actually
// offers rather than against memory.
void query_controller::get_query_help(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("queries.get", request, response)) return;

  if (!validate_arguments(1, what, response)) return;
  const std::string command = what.str(1);

  PB::Registry::ParameterDetails details;
  std::string description;
  if (!fetch_help(core, command, details, description)) {
    response.setCodeNotFound("Query not found");
    return;
  }

  // An alias declares no keywords of its own - its filter expressions are
  // written in the keywords of the command it stands for, so that is the list
  // worth answering with. A single hop: the registry never produces an alias
  // of an alias.
  std::string keyword_source = command;
  if (details.fields_size() == 0) {
    const std::string target = alias_target(description);
    if (!target.empty()) {
      const std::string target_command = target.substr(0, target.find(' '));
      PB::Registry::ParameterDetails target_details;
      std::string ignored;
      if (fetch_help(core, target_command, target_details, ignored) && target_details.fields_size() > 0) {
        keyword_source = target_command;
        details.mutable_fields()->CopyFrom(target_details.fields());
        // An alias takes the target's options too, and declares none itself.
        if (details.parameter_size() == 0) details.mutable_parameter()->CopyFrom(target_details.parameter());
      }
    }
  }

  json::object node;
  node["name"] = command;
  node["keyword_source"] = keyword_source;
  json::array parameters;
  for (const PB::Registry::ParameterDetail &p : details.parameter()) {
    json::object item;
    item["name"] = p.name();
    item["default_value"] = p.default_value();
    item["required"] = p.required();
    item["repeatable"] = p.repeatable();
    item["content_type"] = content_type_name(p.content_type());
    item["short_description"] = p.short_description();
    item["long_description"] = p.long_description();
    parameters.push_back(item);
  }
  node["parameters"] = parameters;
  json::array fields;
  for (const PB::Registry::FieldDetail &f : details.fields()) {
    json::object item;
    // The registry spells a filter function with a trailing "()" and a
    // variable without it; the name is passed on exactly as registered so the
    // client can tell the two apart the same way the console does.
    item["name"] = f.name();
    item["short_description"] = f.short_description();
    item["long_description"] = f.long_description();
    fields.push_back(item);
  }
  node["fields"] = fields;
  response.setCodeOk();
  response.append(json::serialize(node));
}

void query_controller::query_command(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  // Two grants open this endpoint:
  //   queries.execute         - run a query with or without arguments.
  //   queries.execute.noargs  - run a query only when the request carries no
  //                             arguments at all. This is the REST twin of
  //                             the NRPE server's `allow arguments = false`:
  //                             the caller may run the commands the agent
  //                             defines, but cannot shape what they do.
  // They are disjoint in the grant tree (neither implies the other), so a
  // role granting only `queries.execute.noargs` never widens into the full
  // privilege; the `*` wildcard of `full` still confers both.
  // is_logged_in reports which of the two let the request in, so deciding
  // whether arguments are allowed below costs no second walk of the grant tree.
  std::string granted;
  if (!session->is_logged_in(session_manager_interface::grant_options{"queries.execute", "queries.execute.noargs"}, request, response, &granted)) return;

  if (what.size() != 3) {
    response.setCodeNotFound("Invalid request");
    return;
  }
  const std::string module = what.str(1);
  const std::string command = what.str(2);

  const bool nagios_output = command == "execute_nagios";
  if (command != "execute" && !nagios_output) {
    response.setCodeNotFound("unknown command: " + command);
    return;
  }

  // Below the dispatch check on purpose: a request naming a command that does
  // not exist is a 404 for every caller, argument-less or not.
  const arg_vector args = request.getVariablesVector();
  // Every query-string parameter counts, a session token passed the legacy way
  // as `?TOKEN=` by an allowlisted client included: it is forwarded to the
  // check as an argument like any other, so exempting it would hand the
  // restricted role exactly the argument smuggling this grant forbids. A
  // no-arguments caller must authenticate with a header.
  if (!args.empty() && granted == "queries.execute.noargs") {
    std::string user, token;
    session_manager_interface::get_user_from_response(response, user, token);
    // Warning rather than error: this is a misconfigured client, not an agent
    // fault, and a poller that keeps sending arguments would otherwise write an
    // ERROR line per check per interval for as long as it is left alone. The
    // user is empty when anonymous access is enabled and the anonymous role
    // carries the grant.
    const std::string who = user.empty() ? std::string("anonymous caller") : "user '" + user + "'";
    NSC_LOG_WARNING("Refused query " + module + " from " + request.getRemoteIp() + ": " + who +
                    " holds 'queries.execute.noargs', which does not allow arguments.");
    response.setCodeForbidden("403 Arguments are not allowed for this user");
    return;
  }

  if (request.readHeader("Accept") == "text/plain") {
    execute_query_text(module, args, response);
  } else if (nagios_output) {
    execute_query_nagios(module, args, response);
  } else {
    execute_query(module, args, response);
  }
}

// Stamp the calling identity onto a QueryRequestMessage so the core
// permission layer (service/permissions.hpp, decision point in
// service/plugins/plugin_manager.cpp::execute_query) can see who's
// asking. plugin_id identifies WEBServer; the user from the HTTP
// response cookie is the authenticated principal. Empty user leaves the
// principal unset, which is fine - the policy treats it as
// "WEBServer with no named principal".
static void stamp_identity(PB::Commands::QueryRequestMessage &qrm, unsigned int plugin_id, const std::string &user) {
  auto *meta_plugin = qrm.mutable_header()->add_metadata();
  meta_plugin->set_key("nscp.caller_plugin_id");
  meta_plugin->set_value(std::to_string(plugin_id));
  if (!user.empty()) {
    auto *meta_user = qrm.mutable_header()->add_metadata();
    meta_user->set_key("nscp.principal");
    meta_user->set_value(user);
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
  std::string user, token;
  session_manager_interface::get_user_from_response(http_response, user, token);
  stamp_identity(qrm, plugin_id, user);
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
          const auto &fv = p.float_value();
          pdata["value"] = fv.value();
          if (fv.has_minimum()) pdata["minimum"] = fv.minimum().value();
          if (fv.has_maximum()) pdata["maximum"] = fv.maximum().value();
          // Threshold fields carry Nagios range syntax when the original
          // input used it (e.g. "4:5") - prefer that string over the
          // numeric lower bound so the API faithfully reports what the
          // upstream plugin emitted (issue #748). JSON consumers see
          // `number` for plain thresholds and `string` for range syntax;
          // the api.ts type mirrors that union.
          if (!fv.warning_range().empty())
            pdata["warning"] = fv.warning_range();
          else if (fv.has_warning())
            pdata["warning"] = fv.warning().value();
          if (!fv.critical_range().empty())
            pdata["critical"] = fv.critical_range();
          else if (fv.has_critical())
            pdata["critical"] = fv.critical().value();
          pdata["unit"] = fv.unit();
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
  std::string user, token;
  session_manager_interface::get_user_from_response(http_response, user, token);
  stamp_identity(qrm, plugin_id, user);
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
  std::string user, token;
  session_manager_interface::get_user_from_response(http_response, user, token);
  stamp_identity(qrm, plugin_id, user);
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
