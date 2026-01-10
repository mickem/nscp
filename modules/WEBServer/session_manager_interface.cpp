
#include "session_manager_interface.hpp"

#include <Helpers.h>

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_plugin_wrapper.hpp>
#include <str/utils.hpp>
#include <string>

#include "error_handler.hpp"

#define NOT_ALLOWED "403 You're not allowed"

inline bool is_basic_auth(const std::string &auth) { return boost::algorithm::starts_with(auth, "Basic "); }
inline bool is_bearer_auth(const std::string &auth) { return boost::algorithm::starts_with(auth, "Bearer "); }
inline bool has_auth_header(Mongoose::Request &request) { return request.hasVariable(HTTP_HDR_AUTH) || request.hasVariable(HTTP_HDR_AUTH_LC); }
inline std::string find_token(Mongoose::Request &request) {
  auto header_token = request.readHeader("TOKEN");
  if (!header_token.empty()) {
    return header_token;
  }
  std::string x_token = request.readHeader("X-Auth-Token");
  if (!x_token.empty()) {
    return x_token;
  }
  std::string request_token = request.get("TOKEN");
  if (!request_token.empty()) {
    return request_token;
  }
  return request.get("__TOKEN", "");
}
inline std::string get_auth_header(Mongoose::Request &request) {
  std::string auth = request.readHeader(HTTP_HDR_AUTH);
  if (!auth.empty()) {
    return auth;
  }
  return request.readHeader(HTTP_HDR_AUTH_LC);
}

session_manager_interface::session_manager_interface() : log_data(new error_handler()) {}

std::string decode_key(const std::string &encoded) { return Mongoose::Helpers::decode_b64(encoded); }

bool session_manager_interface::process_auth_header(const std::string &grant, Mongoose::Request &request, Mongoose::StreamResponse &response) {
  const std::string auth = get_auth_header(request);
  if (is_basic_auth(auth)) {
    const str::utils::token user_and_password = str::utils::split2(decode_key(auth.substr(6)), ":");
    if (!users.validate_user(user_and_password.first, user_and_password.second)) {
      NSC_LOG_ERROR("Invalid password for " + request.getRemoteIp() + " with user " + user_and_password.first);
      response.setCodeForbidden(NOT_ALLOWED);
      return false;
    }
    store_user_in_response(user_and_password.first, response);
    return can(grant, response);
  }
  if (is_bearer_auth(auth)) {
    const std::string token = auth.substr(7);
    if (!tokens.is_valid(token)) {
      NSC_LOG_ERROR("Invalid bearer token for " + request.getRemoteIp());
      response.setCodeForbidden(NOT_ALLOWED);
      return false;
    }
    store_token_in_response(token, response);
    return can(grant, response);
  }
  NSC_LOG_ERROR("Unknown authentication scheme for " + request.getRemoteIp());
  response.setCodeForbidden("Invalid authentication scheme");
  return false;
}

bool session_manager_interface::is_logged_in(const std::string &grant, Mongoose::Request &request, Mongoose::StreamResponse &response) {
  std::list<std::string> errors;
  if (!allowed_hosts.is_allowed(boost::asio::ip::address::from_string(request.getRemoteIp()), errors)) {
    const std::string error = str::utils::joinEx(errors, ", ");
    NSC_LOG_ERROR("Rejected connection from: " + request.getRemoteIp() + " due to " + error);
    response.setCodeForbidden(NOT_ALLOWED);
    return false;
  }
  if (has_auth_header(request)) {
    return process_auth_header(grant, request, response);
  }
  const std::string token = find_token(request);
  if (token.empty()) {
    NSC_LOG_ERROR("Rejected connection from: " + request.getRemoteIp() + " due to MISSING TOKEN");
    response.setCodeForbidden(NOT_ALLOWED);
    return false;
  }
  if (!tokens.is_valid(token)) {
    NSC_LOG_ERROR("Rejected connection from: " + request.getRemoteIp() + " due to invalid token");
    response.setCodeForbidden(NOT_ALLOWED);
    return false;
  }
  store_token_in_response(token, response);
  if (!can(grant, response)) {
    NSC_LOG_ERROR("Rejected connection from: " + request.getRemoteIp() + " due to insufficient permissions");
    response.setCodeForbidden(NOT_ALLOWED);
  }
  return true;
}

std::list<std::string> session_manager_interface::boot() {
  std::list<std::string> errors;
  allowed_hosts.refresh(errors);
  return errors;
}

void session_manager_interface::store_user_in_response(const std::string &user, Mongoose::StreamResponse &response) {
  response.setCookie("token", tokens.generate_for(user));
  response.setCookie("uid", user);
}

void session_manager_interface::store_token_in_response(const std::string &token, Mongoose::StreamResponse &response) const {
  response.setCookie("token", token);
  response.setCookie("uid", tokens.get_user(token));
}

void session_manager_interface::get_user_from_response(const Mongoose::StreamResponse &response, std::string &user, std::string &key) {
  user = response.getCookie("uid");
  key = response.getCookie("token");
}

bool session_manager_interface::can(const std::string &grant, Mongoose::StreamResponse &response) {
  const std::string uid = response.getCookie("uid");
  if (uid.empty()) {
    if (tokens.can("anonymous", grant)) {
      return true;
    }
    response.setCodeForbidden(NOT_ALLOWED);
    return false;
  }
  if (!tokens.can(uid, grant)) {
    response.setCodeForbidden(NOT_ALLOWED);
    return false;
  }
  return true;
}

void session_manager_interface::add_user(const std::string &user, const std::string &role, const std::string &password) {
  tokens.add_user(user, role);
  users.add_user(user, password);
}

bool session_manager_interface::validate_user(const std::string &user, const std::string &password) { return users.validate_user(user, password); }

bool session_manager_interface::has_user(const std::string &user) const { return users.has_user(user); }

void session_manager_interface::add_grant(const std::string &role, const std::string &grant) { tokens.add_grant(role, grant); }

std::string session_manager_interface::get_metrics() { return metrics_store.get(); }
std::string session_manager_interface::get_metrics_v2() { return metrics_store.get_list(); }
std::string session_manager_interface::get_open_metrics() {
  std::string metrics;
  for (const std::string &m : metrics_store.get_openmetrics()) {
    metrics += m + "\n";
  }
  return metrics;
}
void session_manager_interface::set_metrics(const std::string &metrics, const std::string &metrics_list, std::list<std::string> open_metrics) {
  metrics_store.set(metrics);
  metrics_store.set_list(metrics_list);
  metrics_store.set_openmetrics(open_metrics);
}

void session_manager_interface::add_log_message(const bool is_error, const error_handler_interface::log_entry &entry) const {
  log_data->add_message(is_error, entry);
}
error_handler_interface *session_manager_interface::get_log_data() const { return log_data; }
void session_manager_interface::reset_log() const { log_data->reset(); }

void session_manager_interface::set_allowed_hosts(const std::string &host) { allowed_hosts.set_source(host); }
void session_manager_interface::set_allowed_hosts_cache(const bool value) { allowed_hosts.cached = value; }

bool session_manager_interface::is_allowed(const std::string &ip) {
  std::list<std::string> errors;
  return allowed_hosts.is_allowed(boost::asio::ip::address::from_string(ip), errors);
}

bool session_manager_interface::validate_token(const std::string &token) { return tokens.is_valid(token); }

void session_manager_interface::revoke_token(const std::string &token) { tokens.revoke(token); }

std::string session_manager_interface::generate_token(const std::string &user) { return tokens.generate_for(user); }
