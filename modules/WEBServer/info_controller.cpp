#include "info_controller.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/json.hpp>
#include <boost/regex.hpp>
#include <utility>

#include "helpers.hpp"

namespace json = boost::json;

info_controller::info_controller(const int version, boost::shared_ptr<session_manager_interface> session, const nscapi::core_wrapper *core,
                                 unsigned int plugin_id)
    : RegexpController(version == 1 ? "/api/v1/info" : "/api/v2/info"), session(std::move(session)), core(core), plugin_id(plugin_id) {
  addRoute("GET", "/?$", this, &info_controller::get_info);
  addRoute("GET", "/version/?$", this, &info_controller::get_version);
}

void info_controller::get_info(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_loggedin("info.get", request, response)) return;

  json::object root;
  root["name"] = core->getApplicationName();
  root["version"] = core->getApplicationVersionString();
  root["version_url"] = request.get_host() + "/api/v1/info/version";
  response.append(json::serialize(root));
}

void info_controller::get_version(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_loggedin("info.get.version", request, response)) return;

  json::object root;
  root["version"] = core->getApplicationVersionString();
  response.append(json::serialize(root));
}
