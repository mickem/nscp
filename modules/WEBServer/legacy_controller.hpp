#pragma once

#include "session_manager_interface.hpp"

#include <client/simple_client.hpp>

#include <nscapi/nscapi_core_wrapper.hpp>

#include <MatchController.h>
#include <StreamResponse.h>

#include <boost/thread/shared_mutex.hpp>

#include <string>


class legacy_controller : public Mongoose::MatchController {
	boost::shared_ptr<session_manager_interface> session;
	const nscapi::core_wrapper* core;
	const unsigned int plugin_id;
	boost::shared_ptr<client::cli_client> client;

	std::string status;
	boost::shared_mutex mutex_;

public:

	legacy_controller(boost::shared_ptr<session_manager_interface> session, nscapi::core_wrapper* core, unsigned int plugin_id, boost::shared_ptr<client::cli_client> client);
	std::string get_status();
	bool set_status(std::string status_);
	void console_exec(Mongoose::Request &request, Mongoose::StreamResponse &response);
	void registry_inventory(Mongoose::Request &request, Mongoose::StreamResponse &response);
	void registry_control_module_load(Mongoose::Request &request, Mongoose::StreamResponse &response);
	void registry_control_module_unload(Mongoose::Request &request, Mongoose::StreamResponse &response);
	void registry_inventory_modules(Mongoose::Request &request, Mongoose::StreamResponse &response);
	void settings_inventory(Mongoose::Request &request, Mongoose::StreamResponse &response);
	void settings_query_json(Mongoose::Request &request, Mongoose::StreamResponse &response);
	void settings_query_pb(Mongoose::Request &request, Mongoose::StreamResponse &response);
	void run_query_pb(Mongoose::Request &request, Mongoose::StreamResponse &response);
	void run_exec_pb(Mongoose::Request &request, Mongoose::StreamResponse &response);
	void settings_status(Mongoose::Request &request, Mongoose::StreamResponse &response);

	void auth_token(Mongoose::Request &request, Mongoose::StreamResponse &response);
	void auth_logout(Mongoose::Request &request, Mongoose::StreamResponse &response);
	void redirect_index(Mongoose::Request&, Mongoose::StreamResponse &response);

	void log_status(Mongoose::Request &request, Mongoose::StreamResponse &response);
	void log_messages(Mongoose::Request &request, Mongoose::StreamResponse &response);
	void get_metrics(Mongoose::Request &request, Mongoose::StreamResponse &response);
	void log_reset(Mongoose::Request &request, Mongoose::StreamResponse &response);
	void reload(Mongoose::Request &request, Mongoose::StreamResponse &response);
	void alive(Mongoose::Request &request, Mongoose::StreamResponse &response);
};
