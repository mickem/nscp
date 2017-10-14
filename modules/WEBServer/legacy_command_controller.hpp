#pragma once

#include "session_manager_interface.hpp"
#include <nscapi/nscapi_core_wrapper.hpp>

#include <Controller.h>
#include <StreamResponse.h>

#include <string>

class legacy_command_controller : public Mongoose::Controller {
	boost::shared_ptr<session_manager_interface> session;
	const nscapi::core_wrapper* core;

public:

	legacy_command_controller(boost::shared_ptr<session_manager_interface> session, nscapi::core_wrapper* core);

	void handle_query(std::string obj, Mongoose::Request &request, Mongoose::StreamResponse &response);
	void handle_exec(std::string obj, Mongoose::Request &request, Mongoose::StreamResponse &response);

	Mongoose::Response* handleRequest(Mongoose::Request &request);
	bool handles(string method, string url);
};