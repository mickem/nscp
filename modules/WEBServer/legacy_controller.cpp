#include "legacy_controller.hpp"

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

legacy_controller::legacy_controller(boost::shared_ptr<session_manager_interface> session, nscapi::core_wrapper *core, unsigned int plugin_id,
                                     boost::shared_ptr<client::cli_client> client)
    : session(session), core(core), plugin_id(plugin_id), client(client), status("ok") {
  addRoute("POST", "/query.pb", this, &legacy_controller::run_query_pb);
  addRoute("POST", "/settings/query.pb", this, &legacy_controller::settings_query_pb);
  addRoute("GET", "/log/status", this, &legacy_controller::log_status);
  addRoute("GET", "/log/reset", this, &legacy_controller::log_reset);
  addRoute("GET", "/log/messages", this, &legacy_controller::log_messages);
  addRoute("GET", "/auth/token", this, &legacy_controller::auth_token);
  addRoute("GET", "/auth/logout", this, &legacy_controller::auth_logout);
  addRoute("POST", "/auth/token", this, &legacy_controller::auth_token);
  addRoute("POST", "/auth/logout", this, &legacy_controller::auth_logout);
  addRoute("GET", "/core/reload", this, &legacy_controller::reload);
  addRoute("GET", "/core/isalive", this, &legacy_controller::alive);
  addRoute("GET", "/console/exec", this, &legacy_controller::console_exec);
  addRoute("GET", "/metrics", this, &legacy_controller::get_metrics);
}

std::string legacy_controller::get_status() {
  boost::shared_lock<boost::shared_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(1));
  if (!lock.owns_lock()) return "unknown";
  return status;
}
bool legacy_controller::set_status(std::string status_) {
  boost::shared_lock<boost::shared_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(1));
  if (!lock.owns_lock()) return false;
  status = status_;
  return true;
}

void legacy_controller::console_exec(Mongoose::Request &request, Mongoose::StreamResponse &response) {
  if (!session->is_loggedin("legacy", request, response)) return;
  std::string command = request.get("command", "help");

  client->handle_command(command);
  response.append("{\"status\" : \"ok\"}");
}

void legacy_controller::settings_query_pb(Mongoose::Request &request, Mongoose::StreamResponse &response) {
  if (!session->is_loggedin("legacy", request, response)) return;
  std::string response_pb;
  if (!core->settings_query(request.getData(), response_pb)) {
    response.setCodeServerError("500 Query failed");
    return;
  }
  response.append(response_pb);
}
void legacy_controller::run_query_pb(Mongoose::Request &request, Mongoose::StreamResponse &response) {
  if (!session->is_loggedin("legacy", request, response)) return;
  std::string response_pb;
  if (!core->query(request.getData(), response_pb)) {
    response.setCodeServerError("500 Query failed");
    return;
  }
  response.append(response_pb);
}
void legacy_controller::run_exec_pb(Mongoose::Request &request, Mongoose::StreamResponse &response) {
  if (!session->is_loggedin("legacy", request, response)) return;
  std::string response_pb;
  if (!core->exec_command("*", request.getData(), response_pb)) return;
  response.append(response_pb);
}

void legacy_controller::auth_token(Mongoose::Request &request, Mongoose::StreamResponse &response) {
  if (!session->is_allowed(request.getRemoteIp())) {
    response.setCodeForbidden("403 You're not allowed");
    return;
  }

  if (session->validate_user("admin", request.get("password"))) {
    std::string token = session->generate_token("admin");
    response.setHeader("__TOKEN", token);
    response.append("{ \"status\" : \"ok\", \"auth token\": \"" + token + "\" }");
  } else {
    response.setCodeForbidden("403 Invalid password");
  }
}
void legacy_controller::auth_logout(Mongoose::Request &request, Mongoose::StreamResponse &response) {
  std::string token = request.get("token");
  session->revoke_token(token);
  response.setHeader("__TOKEN", "");
  response.append("{ \"status\" : \"ok\", \"auth token\": \"\" }");
}

void legacy_controller::log_status(Mongoose::Request &request, Mongoose::StreamResponse &response) {
  if (!session->is_loggedin("legacy", request, response)) return;
  error_handler_interface::status status = session->get_log_data()->get_status();
  std::string tmp = status.last_error;
  boost::replace_all(tmp, "\\", "/");
  response.append("{ \"status\" : { \"count\" : " + str::xtos(status.error_count) + ", \"error\" : \"" + tmp + "\"} }");
}
void legacy_controller::log_messages(Mongoose::Request &request, Mongoose::StreamResponse &response) {
  if (!session->is_loggedin("legacy", request, response)) return;
  json_spirit::Object root, log;
  json_spirit::Array data;

  std::string str_position = request.get("pos", "0");
  std::size_t pos = str::stox<std::size_t>(str_position);
  std::size_t ipp = 100000;
  std::size_t count = 0;
  std::list<std::string> levels;
  for (const error_handler_interface::log_entry &e : session->get_log_data()->get_messages(levels, pos, ipp, count)) {
    json_spirit::Object node;
    node.insert(json_spirit::Object::value_type("file", e.file));
    node.insert(json_spirit::Object::value_type("line", e.line));
    node.insert(json_spirit::Object::value_type("type", e.type));
    node.insert(json_spirit::Object::value_type("date", e.date));
    node.insert(json_spirit::Object::value_type("message", e.message));
    data.push_back(node);
  }
  log.insert(json_spirit::Object::value_type("data", data));
  log.insert(json_spirit::Object::value_type("pos", pos));
  root.insert(json_spirit::Object::value_type("log", log));
  response.append(json_spirit::write(root));
}
void legacy_controller::get_metrics(Mongoose::Request &request, Mongoose::StreamResponse &response) {
  if (!session->is_loggedin("legacy", request, response)) return;
  response.append(session->get_metrics());
}
void legacy_controller::log_reset(Mongoose::Request &request, Mongoose::StreamResponse &response) {
  if (!session->is_loggedin("legacy", request, response)) return;
  session->reset_log();
  response.append("{\"status\" : \"ok\"}");
}
void legacy_controller::reload(Mongoose::Request &request, Mongoose::StreamResponse &response) {
  if (!session->is_loggedin("legacy", request, response)) return;
  core->reload("delayed,service");
  set_status("reload");
  response.append("{\"status\" : \"reload\"}");
}
void legacy_controller::alive(Mongoose::Request &request, Mongoose::StreamResponse &response) { response.append("{\"status\" : \"" + get_status() + "\"}"); }