#include "info_controller.hpp"
#include "helpers.hpp"

#include <nscapi/nscapi_protobuf.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>

#include <json_spirit.h>

#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/regex.hpp>


info_controller::info_controller(const int version, boost::shared_ptr<session_manager_interface> session, nscapi::core_wrapper* core, unsigned int plugin_id)
  : session(session)
  , core(core)
  , plugin_id(plugin_id)
  , RegexpController(version==1?"/api/v1/info":"/api/v2/info")
{
	addRoute("GET", "/?$", this, &info_controller::get_info);
	addRoute("GET", "/version/?$", this, &info_controller::get_version);
}

void info_controller::get_info(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
	if (!session->is_loggedin("info.get", request, response))
		return;

	json_spirit::Object root;
	root["name"] = core->getApplicationName();
	root["version"] = core->getApplicationVersionString();
	root["version_url"] = request.get_host() + "/api/v1/info/version";
	response.append(json_spirit::write(root));
}


void info_controller::get_version(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
	if (!session->is_loggedin("info.get.version", request, response))
		return;

	json_spirit::Object root;
	root.insert(json_spirit::Object::value_type("version", core->getApplicationVersionString()));
	response.append(json_spirit::write(root));
}
