#include "legacy_controller.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/json.hpp>
#include <boost/thread/locks.hpp>
#include <client/simple_client.hpp>
#include <nscapi/macros.hpp>
#include <str/xtos.hpp>

#include "error_handler_interface.hpp"

namespace json = boost::json;

legacy_controller::legacy_controller(const boost::shared_ptr<session_manager_interface> &session, const nscapi::core_wrapper *core, unsigned int plugin_id,
                                     const boost::shared_ptr<client::cli_client> &client)
    : session(session), core(core), plugin_id(plugin_id), client(client), status("ok") {
  addRoute("POST", "/query.pb", this, &legacy_controller::run_query_pb);
  addRoute("POST", "/settings/query.pb", this, &legacy_controller::settings_query_pb);
  addRoute("GET", "/log/status", this, &legacy_controller::log_status);
  // State-changing endpoints must be POST so that they cannot be triggered by
  // a cross-origin <img>/<a>/<form> CSRF gadget that an authenticated admin
  // happens to render. The matching GET routes are deliberately not
  // registered.
  addRoute("POST", "/log/reset", this, &legacy_controller::log_reset);
  addRoute("GET", "/log/messages", this, &legacy_controller::log_messages);
  addRoute("GET", "/auth/token", this, &legacy_controller::auth_token);
  addRoute("GET", "/auth/logout", this, &legacy_controller::auth_logout);
  addRoute("POST", "/auth/token", this, &legacy_controller::auth_token);
  addRoute("POST", "/auth/logout", this, &legacy_controller::auth_logout);
  addRoute("POST", "/core/reload", this, &legacy_controller::reload);
  addRoute("GET", "/core/isalive", this, &legacy_controller::alive);
  // /console/exec is RCE-equivalent: it forwards arbitrary CLI commands. Keep
  // it on POST and gate it behind a dedicated grant so an operator who only
  // wants legacy read access cannot accidentally hand out a remote shell.
  addRoute("POST", "/console/exec", this, &legacy_controller::console_exec);
  addRoute("GET", "/metrics", this, &legacy_controller::get_metrics);
}

std::string legacy_controller::get_status() {
  const boost::shared_lock<boost::shared_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(1));
  if (!lock.owns_lock()) return "unknown";
  return status;
}
bool legacy_controller::set_status(std::string status_) {
  const boost::shared_lock<boost::shared_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(1));
  if (!lock.owns_lock()) return false;
  status = status_;
  return true;
}

void legacy_controller::console_exec(Mongoose::Request &request, Mongoose::StreamResponse &response) {
  // RCE-equivalent: this endpoint forwards the "command" parameter straight
  // into the CLI dispatcher. Keep the auth grant separate from the broad
  // "legacy" grant so the privilege is explicit in the role table.
  if (!session->is_logged_in("console.exec", request, response)) return;
  const std::string command = request.get("command", "help");

  client->handle_command(command);
  response.append("{\"status\" : \"ok\"}");
}

void legacy_controller::settings_query_pb(Mongoose::Request &request, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("legacy", request, response)) return;
  std::string response_pb;
  if (!core->settings_query(request.getData(), response_pb)) {
    response.setCodeServerError("500 Query failed");
    return;
  }
  response.append(response_pb);
}
void legacy_controller::run_query_pb(Mongoose::Request &request, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("legacy", request, response)) return;
  std::string response_pb;
  if (!core->query(request.getData(), response_pb)) {
    response.setCodeServerError("500 Query failed");
    return;
  }
  response.append(response_pb);
}
void legacy_controller::run_exec_pb(Mongoose::Request &request, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("legacy", request, response)) return;
  std::string response_pb;
  if (!core->exec_command("*", request.getData(), response_pb)) return;
  response.append(response_pb);
}

void legacy_controller::auth_token(Mongoose::Request &request, Mongoose::StreamResponse &response) {
  NSC_LOG_ERROR("Rejected legacy /auth/token call from " + request.getRemoteIp() +
                ": this endpoint accepted the password as a URL query parameter and has been removed. "
                "Use POST /api/v2/login with HTTP Basic authentication instead.");
  response.setCode(410, "Gone");
  response.append(
      "{ \"status\" : \"error\", \"message\" : \"The /auth/token endpoint has been removed. "
      "It accepted the password as a URL query parameter, which leaked credentials into browser history, "
      "proxy logs and Referer headers. Authenticate with POST /api/v2/login using the Authorization: Basic header.\" }");
}
void legacy_controller::auth_logout(Mongoose::Request &request, Mongoose::StreamResponse &response) {
  NSC_LOG_ERROR("Rejected legacy /auth/logout call from " + request.getRemoteIp() +
                ": this endpoint accepted the session token as a URL query parameter and has been removed. "
                "Use DELETE /api/v2/login with the Authorization: Bearer header instead.");
  response.setCode(410, "Gone");
  response.append(
      "{ \"status\" : \"error\", \"message\" : \"The /auth/logout endpoint has been removed. "
      "It accepted the session token as a URL query parameter, which leaked credentials into browser history, "
      "proxy logs and Referer headers. Log out via DELETE /api/v2/login with the Authorization: Bearer header.\" }");
}

void legacy_controller::log_status(Mongoose::Request &request, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("legacy", request, response)) return;
  const error_handler_interface::status current_status = session->get_log_data()->get_status();
  std::string tmp = current_status.last_error;
  boost::replace_all(tmp, "\\", "/");
  response.append("{ \"status\" : { \"count\" : " + str::xtos(current_status.error_count) + ", \"error\" : \"" + tmp + "\"} }");
}
void legacy_controller::log_messages(Mongoose::Request &request, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("legacy", request, response)) return;
  json::object root, log;
  json::array data;

  const std::string str_position = request.get("pos", "0");
  std::size_t pos = str::stox<std::size_t>(str_position);
  constexpr std::size_t ipp = 100000;
  std::size_t count = 0;
  const std::list<std::string> levels;
  for (const error_handler_interface::log_entry &e : session->get_log_data()->get_messages(levels, pos, ipp, count)) {
    json::object node;
    node.insert(json::object::value_type("file", e.file));
    node.insert(json::object::value_type("line", e.line));
    node.insert(json::object::value_type("type", e.type));
    node.insert(json::object::value_type("date", e.date));
    node.insert(json::object::value_type("message", e.message));
    data.push_back(node);
  }
  log.insert(json::object::value_type("data", data));
  log.insert(json::object::value_type("pos", pos));
  root.insert(json::object::value_type("log", log));
  response.append(json::serialize(root));
}
void legacy_controller::get_metrics(Mongoose::Request &request, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("legacy", request, response)) return;
  response.append(session->get_metrics());
}
void legacy_controller::log_reset(Mongoose::Request &request, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("legacy", request, response)) return;
  session->reset_log();
  response.append("{\"status\" : \"ok\"}");
}
void legacy_controller::reload(Mongoose::Request &request, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("legacy", request, response)) return;
  if (!core->reload("delayed,service")) {
    response.setCodeServerError("500 Query failed");
    return;
  }
  set_status("reload");
  response.append("{\"status\" : \"reload\"}");
}
void legacy_controller::alive(Mongoose::Request &request, Mongoose::StreamResponse &response) { response.append("{\"status\" : \"" + get_status() + "\"}"); }