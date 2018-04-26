#include "login_controller.hpp"

#include <json_spirit.h>

login_controller::login_controller(const int version, boost::shared_ptr<session_manager_interface> session)
	: RegexpController(version==1?"/api/v1/login":"/api/v2/login")
	, session(session) {
	addRoute("GET", "/?$", this, &login_controller::is_loggedin);
}

void login_controller::is_loggedin(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
	if (!session->is_loggedin("login.get", request, response))
		return;

	std::string user, key;
	session->get_user(response, user, key);
	json_spirit::Object root;
	root["user"] = user;
	root["key"] = key;
	response.append(json_spirit::write(root));
}


