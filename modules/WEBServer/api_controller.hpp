#pragma once

#include "session_manager_interface.hpp"
#include "helpers.hpp"

#include <RegexController.h>
#include <StreamResponse.h>

#include <string>


class api_controller : public Mongoose::RegexpController {
	boost::shared_ptr<session_manager_interface> session;

public:

	api_controller(boost::shared_ptr<session_manager_interface> session);

	void get_versions(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
	void get_eps(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
};
