#pragma once

#include "session_manager_interface.hpp"

#include <client/simple_client.hpp>

#include <nscapi/nscapi_core_wrapper.hpp>

#include <RegexController.h>
#include <StreamResponse.h>

#include <boost/thread/shared_mutex.hpp>

#include <string>


class query_controller : public Mongoose::RegexpController {
	boost::shared_ptr<session_manager_interface> session;
	const nscapi::core_wrapper* core;
	const unsigned int plugin_id;

	typedef std::vector<std::pair<std::string, std::string> > arg_vector;

public:

	query_controller(const int version, boost::shared_ptr<session_manager_interface> session, nscapi::core_wrapper* core, unsigned int plugin_id);

	void get_queries(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
	void get_query(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
	void query_command(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
	void execute_query(std::string module, arg_vector args, Mongoose::StreamResponse &response);
	void execute_query_nagios(std::string module, arg_vector args, Mongoose::StreamResponse &response);
	void execute_query_text(std::string module, arg_vector args, Mongoose::StreamResponse &response);

};
