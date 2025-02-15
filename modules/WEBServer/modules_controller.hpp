#pragma once

#include "session_manager_interface.hpp"
#include "helpers.hpp"

#include <client/simple_client.hpp>

#include <nscapi/nscapi_core_wrapper.hpp>

#include <RegexController.h>
#include <StreamResponse.h>

#include <boost/thread/shared_mutex.hpp>

#include <string>

class modules_controller : public Mongoose::RegexpController {
	boost::shared_ptr<session_manager_interface> session;
	const nscapi::core_wrapper* core;
	const unsigned int plugin_id;
	const int version;

public:

	modules_controller(const int version, boost::shared_ptr<session_manager_interface> session, nscapi::core_wrapper* core, unsigned int plugin_id);

	void get_modules(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
	void get_module(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
	void put_module(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
	void post_module(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
	void module_command(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
	void load_module(std::string module, Mongoose::StreamResponse &response);
	void unload_module(std::string module, Mongoose::StreamResponse &response);
	void enable_module(std::string module, Mongoose::StreamResponse &response);
	void disable_module(std::string module, Mongoose::StreamResponse &response);
};

