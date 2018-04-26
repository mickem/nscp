
#include "session_manager_interface.hpp"

#include "error_handler.hpp"

#include <Helpers.h>

#include <str/utils.hpp>

#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>

#include <string>

session_manager_interface::session_manager_interface()
	: log_data(new error_handler())
{}

std::string decode_key(std::string encoded) {
	return Mongoose::Helpers::decode_b64(encoded);
}

bool session_manager_interface::is_loggedin(std::string grant, Mongoose::Request &request, Mongoose::StreamResponse &response) {
	std::list<std::string> errors;
	if (!allowed_hosts.is_allowed(boost::asio::ip::address::from_string(request.getRemoteIp()), errors)) {
// 		BOOST_FOREACH(const std::string &e, errors) {
// 			NSC_LOG_ERROR(e);
// 		}
		//NSC_LOG_ERROR("Rejected connection from: " + request.getRemoteIp());
		response.setCode(HTTP_FORBIDDEN);
		response.append("403 You're not allowed");
		return false;
	}
	if (request.hasVariable(HTTP_HDR_AUTH) || request.hasVariable(HTTP_HDR_AUTH_LC)) {
		std::string auth = request.readHeader(HTTP_HDR_AUTH);
		if (auth.empty()) {
			auth = request.readHeader(HTTP_HDR_AUTH_LC);
		}
		if (boost::algorithm::starts_with(auth, "Basic ")) {
			str::utils::token token = str::utils::split2(decode_key(auth.substr(6)), ":");
			if (!validate_user(token.first, token.second)) {
				response.setCode(HTTP_FORBIDDEN);
				response.append("403 You're not allowed");
				return false;
			}
			setup_token(token.first, response);
			return can(grant, request, response);
		} else if (boost::algorithm::starts_with(auth, "Bearer ")) {
			std::string token = auth.substr(7);
			if (!tokens.validate(token)) {
				response.setCode(HTTP_FORBIDDEN);
				response.append("403 You're not allowed");
				return false;
			}
			setup_user(token, response);
			return can(grant, request, response);
		} else {
			response.setCode(HTTP_BAD_REQUEST);
			response.append("Invalid authentication scheme");
			return false;
		}
	}
	if (request.hasVariable("Password")) {
		std::string pwd = request.readHeader("Password");
		std::string fake_user = "admin";
		if (!validate_user(fake_user, pwd)) {
			response.setCode(HTTP_FORBIDDEN);
			response.append("403 You're not allowed");
			return false;
		}
		setup_token(fake_user, response);
		return can(grant, request, response);
	}
	if (request.hasVariable("password")) {
		std::string pwd = request.readHeader("password");
		std::string fake_user = "admin";
		if (!validate_user(fake_user, pwd)) {
			response.setCode(HTTP_FORBIDDEN);
			response.append("403 You're not allowed");
			return false;
		}
		setup_token(fake_user, response);
		return can(grant, request, response);
	}


	std::string token = request.readHeader("TOKEN");
	if (token.empty())
		token = request.get("__TOKEN", "");
	if (!token.empty()) {
		if (!tokens.validate(token)) {
			response.setCode(HTTP_FORBIDDEN);
			response.append("403 You're not allowed");
			return false;
		}
		return can(grant, request, response);
	}
	response.setCode(HTTP_FORBIDDEN);
	response.append("403 Please login first");
	return false;
}


std::list<std::string> session_manager_interface::boot() {
	std::list<std::string> errors;
	allowed_hosts.refresh(errors);
	return errors;
}

bool session_manager_interface::validate_user(const std::string user, const std::string &password) {
	if (password.empty()) {
		return false;
	}
	if (users.find(user) == users.end()) {
		return false;
	}
	return users[user] == password;
}

void session_manager_interface::setup_token(std::string &user, Mongoose::StreamResponse & response) {
	response.setCookie("token", tokens.generate(user));
	response.setCookie("uid", user);
}


void session_manager_interface::setup_user(std::string &token, Mongoose::StreamResponse & response) {
	response.setCookie("token", token);
	response.setCookie("uid", tokens.get(token));
}

void session_manager_interface::get_user(const Mongoose::StreamResponse & response, std::string &user, std::string &key) const {
	user = response.getCookie("uid");
	key = response.getCookie("token");
}

bool session_manager_interface::can(std::string grant, Mongoose::Request & request, Mongoose::StreamResponse & response) {
	std::string uid = response.getCookie("uid");
	if (uid.empty()) {
		response.setCode(HTTP_FORBIDDEN);
		response.append("403 You're not allowed");
		return false;
	}
	if (!tokens.can(uid, grant)) {
		response.setCode(HTTP_FORBIDDEN);
		response.append("403 You're not allowed");
		return false;
	}
	return true;
}

void session_manager_interface::add_user(std::string user, std::string role, std::string password) {
	tokens.add_user(user, role);
	users[user] = password;
}

bool session_manager_interface::has_user(std::string user) const {
	return users.find(user) != users.end();
}


void session_manager_interface::add_grant(std::string role, std::string grant) {
	tokens.add_grant(role, grant);
}

std::string session_manager_interface::get_metrics() {
	return metrics_store.get();
}
std::string session_manager_interface::get_metrics_v2() {
	return metrics_store.get_list();
}
void session_manager_interface::set_metrics(std::string metrics, std::string metrics_list) {
	metrics_store.set(metrics);
	metrics_store.set_list(metrics_list);
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

std::string session_manager_interface::generate_token(std::string user) {
	return tokens.generate(user);
}
