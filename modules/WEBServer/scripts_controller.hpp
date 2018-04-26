#pragma once

#include "session_manager_interface.hpp"
#include "helpers.hpp"

#include <client/simple_client.hpp>

#include <nscapi/nscapi_core_wrapper.hpp>

#include <RegexController.h>
#include <StreamResponse.h>

#include <boost/thread/shared_mutex.hpp>

#include <string>


class scripts_controller : public Mongoose::RegexpController {
	boost::shared_ptr<session_manager_interface> session;
	const nscapi::core_wrapper* core;
	const unsigned int plugin_id;

public:

	scripts_controller(const int version, boost::shared_ptr<session_manager_interface> session, nscapi::core_wrapper* core, unsigned int plugin_id);

	void get_runtimes(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
	void get_scripts(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
	void get_script(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
	void add_script(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
	void delete_script(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
};
