#include "api_controller.hpp"

#include <json_spirit.h>

#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/regex.hpp>
#include <boost/filesystem/path.hpp>

std::string get_host(Mongoose::Request &request) {
	if (request.hasVariable("Host")) {
		std::string proto = request.is_ssl() ? "https://" : "http://";
		return proto + request.readHeader("Host");
	}
	return "";
}

api_controller::api_controller(boost::shared_ptr<session_manager_interface> session)
	: RegexpController("/api")
	, session(session)
{
	addRoute("GET", "/?$", this, &api_controller::get_versions);
	addRoute("GET", "/v1/?$", this, &api_controller::get_eps);
}

void api_controller::get_versions(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
	if (!session->is_loggedin(request, response))
		return;

	std::string host = get_host(request);

	json_spirit::Object root;
	root["current_api"] = host + "/api/v1";
	root["legacy_api"] = host + "/api/v1";
	root["beta_api"] = host + "/api/v1";
	response.append(json_spirit::write(root));
}

void api_controller::get_eps(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
	if (!session->is_loggedin(request, response))
		return;

	std::string host = get_host(request);

	json_spirit::Object root;
	root["scripts_url"] = host + "/api/v1/scripts";
	root["modules_url"] = host + "/api/v1/modules";
	root["queries_url"] = host + "/api/v1/queries";
	response.append(json_spirit::write(root));
}
