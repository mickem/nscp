#include "legacy_controller.hpp"

#include <boost/json.hpp>
#include <boost/thread/locks.hpp>
#include <client/simple_client.hpp>
#include <nscapi/macros.hpp>
#include <str/xtos.hpp>

#include "error_handler_interface.hpp"

namespace json = boost::json;

legacy_controller::legacy_controller(const std::shared_ptr<session_manager_interface> &session, const nscapi::core_wrapper *core, unsigned int plugin_id,
                                     const std::shared_ptr<client::cli_client> &client)
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
  // Raw-protobuf passthrough: forwarded verbatim. The newer
  // query_controller (v2 `/api/vX/queries/...`) is the supported way to
  // invoke checks from HTTP - it stamps identity metadata so the core
  // permission layer can attribute calls. This legacy endpoint is
  // deliberately left unstamped: callers using it should be migrated to
  // the v2 controller, and a strict default-deny policy will block this
  // path because the subject resolves to "*" (no caller module known)
  // rather than to WEBServer. That's the intended behaviour for a
  // deprecated endpoint.
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
  // The endpoint was disabled in commit 340b8db1 because it accepted the
  // password as `?password=...` — leaked into browser history, proxy logs,
  // and Referer headers. Re-enabling it unconditionally would reintroduce
  // that vector. We gate it on a User-Agent allowlist so legacy integrations
  // (default: Icinga's check_nscp_api) keep working while browsers and
  // arbitrary scrapers still get the original "Gone" rejection.
  //
  // Content-Type is set per-branch rather than once at entry: the 403 paths
  // below emit plain-text status lines via setCodeForbidden(), and labelling
  // those as application/json would mislead callers (and break clients that
  // try to JSON-parse the body, e.g. supertest in our REST suite).
  const std::string user_agent = request.readHeader("User-Agent");
  if (!session->client_allows_legacy_query_auth(user_agent)) {
    NSC_LOG_ERROR("Rejected legacy /auth/token call from " + request.getRemoteIp() + " (User-Agent: " + user_agent +
                  "): this endpoint accepted the password as a URL query parameter and has been disabled. "
                  "Use POST /api/v2/login with HTTP Basic authentication instead, or add this client's User-Agent to "
                  "[/settings/WEB/server] 'legacy query auth user agents' if you cannot upgrade it.");
    response.setCode(410, "Gone");
    response.get_headers()["Content-Type"] = "application/json";
    response.append(
        "{ \"status\" : \"error\", \"message\" : \"The /auth/token endpoint has been removed. "
        "It accepted the password as a URL query parameter, which leaked credentials into browser history, "
        "proxy logs and Referer headers. Authenticate with POST /api/v2/login using the Authorization: Basic header.\" }");
    return;
  }
  if (!session->is_allowed(request.getRemoteIp())) {
    response.setCodeForbidden("403 You're not allowed");
    return;
  }
  if (session->validate_user("admin", request.get("password"))) {
    const std::string token = session->generate_token("admin");
    response.setHeader("__TOKEN", token);
    response.get_headers()["Content-Type"] = "application/json";
    response.append("{ \"status\" : \"ok\", \"auth token\": \"" + token + "\" }");
    NSC_DEBUG_MSG("Issued legacy /auth/token for allowlisted User-Agent: " + user_agent);
  } else {
    response.setCodeForbidden("403 Invalid password");
  }
}
void legacy_controller::auth_logout(Mongoose::Request &request, Mongoose::StreamResponse &response) {
  // Same per-branch Content-Type pattern as auth_token above.
  const std::string user_agent = request.readHeader("User-Agent");
  if (!session->client_allows_legacy_query_auth(user_agent)) {
    NSC_LOG_ERROR("Rejected legacy /auth/logout call from " + request.getRemoteIp() + " (User-Agent: " + user_agent +
                  "): this endpoint accepted the session token as a URL query parameter and has been disabled. "
                  "Use DELETE /api/v2/login with the Authorization: Bearer header instead, or add this client's User-Agent to "
                  "[/settings/WEB/server] 'legacy query auth user agents' if you cannot upgrade it.");
    response.setCode(410, "Gone");
    response.get_headers()["Content-Type"] = "application/json";
    response.append(
        "{ \"status\" : \"error\", \"message\" : \"The /auth/logout endpoint has been removed. "
        "It accepted the session token as a URL query parameter, which leaked credentials into browser history, "
        "proxy logs and Referer headers. Log out via DELETE /api/v2/login with the Authorization: Bearer header.\" }");
    return;
  }
  const std::string token = request.get("token");
  session->revoke_token(token);
  response.setHeader("__TOKEN", "");
  response.get_headers()["Content-Type"] = "application/json";
  response.append("{ \"status\" : \"ok\", \"auth token\": \"\" }");
}

void legacy_controller::log_status(Mongoose::Request &request, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("legacy", request, response)) return;
  const error_handler_interface::status current_status = session->get_log_data()->get_status();
  // Use the json serializer rather than string concatenation. The previous
  // version replaced backslashes but left double-quotes intact, so a log
  // message containing `"` would break the surrounding JSON and let an
  // authenticated logs.put caller smuggle extra fields into log/status.
  json::object status_node;
  status_node["count"] = current_status.error_count;
  status_node["error"] = current_status.last_error;
  json::object root;
  root["status"] = status_node;
  response.append(json::serialize(root));
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