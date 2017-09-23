
#include "session_manager_interface.hpp"

#include "error_handler.hpp"

#include <boost/asio.hpp>



session_manager_interface::session_manager_interface()
	: log_data(new error_handler())
	, password_(token_store::generate_token(24))
{}

bool session_manager_interface::is_loggedin(Mongoose::Request &request, Mongoose::StreamResponse &response, bool respond /*= true*/) {
	std::list<std::string> errors;
	if (!allowed_hosts.is_allowed(boost::asio::ip::address::from_string(request.getRemoteIp()), errors)) {
// 		BOOST_FOREACH(const std::string &e, errors) {
// 			NSC_LOG_ERROR(e);
// 		}
		//NSC_LOG_ERROR("Rejected connection from: " + request.getRemoteIp());
		response.setCode(HTTP_FORBIDDEN);
		response.append("403 Your not allowed");
		return false;
	}


	std::string token = request.readHeader("TOKEN");
	if (token.empty())
		token = request.get("__TOKEN", "");
	bool auth = false;
	std::string password = request.readHeader("password");
	if (token.empty()) {
		if (password.empty())
			password = request.get("password", "");
		auth = !password.empty() && password == password_;
	} else {
		auth = tokens.validate(token);
	}
	if (!auth) {
// 		NSC_LOG_ERROR("Invalid password/token from: " + request.getRemoteIp() + ": " + password);
		if (respond) {
			response.setCode(HTTP_FORBIDDEN);
			response.append("403 Please login first");
		}
		return false;
	}
	return true;
}


std::list<std::string> session_manager_interface::boot() {
	std::list<std::string> errors;
	allowed_hosts.refresh(errors);
	return errors;
}

bool session_manager_interface::validate_password(string password) {
	if (password_.empty()) {
		return false;
	}
	return password == password_;
}

std::string session_manager_interface::get_metrics() {
	return metrics_store.get();
}
void session_manager_interface::set_metrics(std::string metrics) {
	metrics_store.set(metrics);
}

void session_manager_interface::add_log_message(bool is_error, error_handler_interface::log_entry entry) {
	log_data->add_message(is_error, entry);
}
error_handler_interface* session_manager_interface::get_log_data() {
	return log_data;
}
void session_manager_interface::reset_log() {
	log_data->reset();
}


void session_manager_interface::set_allowed_hosts(std::string host) {
	allowed_hosts.set_source(host);
}
void session_manager_interface::set_allowed_hosts_cache(bool value) {
	allowed_hosts.cached = value;
}

void session_manager_interface::set_password(std::string password) {
	password_ = password;
}

bool session_manager_interface::is_allowed(std::string ip) {
	std::list<std::string> errors;
	return allowed_hosts.is_allowed(boost::asio::ip::address::from_string(ip), errors);
}

bool session_manager_interface::validate_token(std::string token) {
	return tokens.validate(token);
}

void session_manager_interface::revoke_token(std::string token) {
	tokens.revoke(token);
}

std::string session_manager_interface::generate_token() {
	return tokens.generate();
}
