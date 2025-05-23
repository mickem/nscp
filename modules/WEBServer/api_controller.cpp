#include "api_controller.hpp"

#include <boost/json.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>

namespace json = boost::json;

api_controller::api_controller(const boost::shared_ptr<session_manager_interface> &session) : RegexpController("/api"), session(session) {
  addRoute("GET", "/?$", this, &api_controller::get_versions);
  addRoute("GET", "/v1/?$", this, &api_controller::get_eps);
  addRoute("GET", "/v2/?$", this, &api_controller::get_eps);
}

void api_controller::get_versions(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_loggedin("public", request, response)) return;

  const std::string host = request.get_host();

  json::object root;
  root["current_api"] = host + "/api/v2";
  root["legacy_api"] = host + "/api/v1";
  root["beta_api"] = host + "/api/v2";
  response.append(json::serialize(root));
}

void api_controller::get_eps(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_loggedin("public", request, response)) return;

  const std::string host = request.get_host();

  json::object root;
  root["scripts_url"] = host + "/api/v2/scripts";
  root["modules_url"] = host + "/api/v2/modules";
  root["queries_url"] = host + "/api/v2/queries";
  root["settings_url"] = host + "/api/v2/settings";
  root["logs_url"] = host + "/api/v2/logs";
  root["info_url"] = host + "/api/v2/info";
  response.append(json::serialize(root));
}
