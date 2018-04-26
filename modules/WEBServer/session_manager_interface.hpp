#pragma once

#include "error_handler_interface.hpp"
#include "metrics_handler.hpp"
#include "token_store.hpp"
#include "metrics_handler.hpp"

#include <StreamResponse.h>
#include <Request.h>

#include <socket/socket_helpers.hpp>

#include <string>
#include <list>

struct session_manager_interface {

private:
	error_handler_interface* log_data;

	metrics_handler metrics_store;
	token_store tokens;
	socket_helpers::allowed_hosts_manager allowed_hosts;
	boost::unordered_map<std::string, std::string> users;
public:
	session_manager_interface();

	bool is_loggedin(std::string grant, Mongoose::Request &request, Mongoose::StreamResponse &response);

	bool is_allowed(std::string ip);

	bool validate_token(std::string token);
	void revoke_token(std::string token);
	std::string generate_token(std::string user);

	std::string get_metrics();
	std::string get_metrics_v2();
	void set_metrics(std::string metrics, std::string metrics_list);

	void add_log_message(bool is_error, error_handler_interface::log_entry entry);
	error_handler_interface* get_log_data();
	void reset_log();

	void set_allowed_hosts(std::string host);
	void set_allowed_hosts_cache(bool value);

	std::list<std::string> boot();
	bool validate_user(const std::string user, const std::string &password);
	void setup_token(std::string &user, Mongoose::StreamResponse & response);
	void setup_user(std::string &token, Mongoose::StreamResponse & response);
	bool can(std::string grant, Mongoose::Request & request, Mongoose::StreamResponse & response);
	void get_user(const Mongoose::StreamResponse & response, std::string &user, std::string &key) const;
	void add_user(std::string user, std::string role, std::string password);
	bool has_user(std::string user) const;
	void add_grant(std::string role, std::string grant);
};
