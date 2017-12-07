/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Op5Client.h"
#include "op5_handler.hpp"

#include <json_spirit.h>

#include <helpers.h>

#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/nscapi_protobuf_nagios.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/macros.hpp>

#include <str/utils.hpp>
#include <str/format.hpp>

#include <boost/algorithm/string/replace.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/asio.hpp>
#include <boost/range/iterator_range.hpp>
/**
 * Default c-tor
 * @return
 */
Op5Client::Op5Client() 
{}

/**
 * Default d-tor
 * @return
 */
Op5Client::~Op5Client() {}

#include <Client.hpp>


#define HTTP_HDR_AUTH "Authorization"
#define HTTP_HDR_AUTH_BASIC "Basic "


bool is_200(const boost::shared_ptr<Mongoose::Response> &response) {
	if (!response) {
		return false;
	}
	return response->get_response_code() >= 200 && response->get_response_code() <= 299;
}

bool is_404(const boost::shared_ptr<Mongoose::Response> &response) {
	if (!response) {
		return false;
	}
	return response->get_response_code() == 404;
}

std::string get_error(const boost::shared_ptr<Mongoose::Response> &response) {
	if (!response) {
		return "Failed to connect to host";
	}
	return str::xtos(response->get_response_code()) + ": " + response->getBody();
}


std::string get_my_ip() {
	boost::asio::io_service io_service;
	boost::asio::ip::tcp::resolver resolver(io_service);

	std::string h = boost::asio::ip::host_name();
	boost::optional<std::string> firstv4;
	boost::optional<std::string> firstv6;

	tcp::resolver::iterator endpoint_iterator = resolver.resolve(boost::asio::ip::tcp::resolver::query(h, ""));
	tcp::resolver::iterator end;

	for (; endpoint_iterator != end; endpoint_iterator++) {
		if (endpoint_iterator->endpoint().address().is_v6()) {
			if (!firstv6) {
				firstv6 = endpoint_iterator->endpoint().address().to_string();
			}
		} else {
			if (!firstv4) {
				firstv4 = endpoint_iterator->endpoint().address().to_string();
			}
		}
	}
	if (firstv4) {
		return *firstv4;
	} else if (firstv6) {
		return *firstv6;
	}
	return h;
}

std::map<std::string, std::string> make_header(std::string user, std::string password) {
	std::map<std::string, std::string> hdr;
	std::string uid = user + ":" + password;
	hdr[HTTP_HDR_AUTH] = std::string(HTTP_HDR_AUTH_BASIC) + Mongoose::Helpers::encode_b64(uid);
	hdr["Accept"] = "application/json";
	hdr["Content-type"] = "application/json";
	return hdr;
}

bool Op5Client::has_host(std::string host) {
	Mongoose::Client query(op5_url + "/api/filter/query?query=[hosts]%20name=\"" + host + "\"");
	boost::shared_ptr<Mongoose::Response> response = query.fetch("GET", make_header(op5_username, op5_password), "");
	if (!is_200(response)) {
		NSC_LOG_ERROR("Failed to check host: " + host + ": " + get_error(response));
		return false;
	}

	try {
		json_spirit::Value root;
		std::string data = response->getBody();
		json_spirit::read_or_throw(data, root);
		return root.getArray().size() > 0;
	} catch (const json_spirit::ParseError &e) {
		NSC_LOG_ERROR("Failed to parse reponse: " + response->getBody());
		return false;
	}
	return false;
}

std::pair<bool, bool> Op5Client::has_service(std::string service, std::string host, std::string &hosts_string) {
	Mongoose::Client query(op5_url + "/api/config/service/" + service);
	boost::shared_ptr<Mongoose::Response> response = query.fetch("GET", make_header(op5_username, op5_password), "");

	if (is_404(response)) {
		return std::pair<bool, bool>(false, false);
	}
	if (!is_200(response)) {
		NSC_LOG_ERROR("Failed to check host: " + service + ": " + get_error(response));
		return std::pair<bool, bool>(false, false);
	}

	try {
		json_spirit::Value root;
		std::string data = response->getBody();
		json_spirit::read_or_throw(data, root);
		std::vector<std::string> hosts;
		hosts_string = root.getString("host_name");
		boost::split(hosts, hosts_string, boost::is_any_of(", "), boost::token_compress_on);
		if (std::find(hosts.begin(), hosts.end(), host) == hosts.end()) {
			return std::pair<bool, bool>(true, false);
		} else {
			return std::pair<bool, bool>(true, true);
		}
	} catch (const json_spirit::ParseError &e) {
		NSC_LOG_ERROR("Failed to parse reponse: " + response->getBody());
		return std::pair<bool, bool>(false, false);
	}
	return std::pair<bool, bool>(false, false);
}

bool Op5Client::add_host(std::string host) {

	json_spirit::Object req;
	req["alias"] = host;
	req["host_name"] = host;
	req["address"] = get_my_ip();
	req["active_checks_enabled"] = 0;
	if (!hostgroups_.empty()) {
		req["hostgroups"] = hostgroups_;
	}
	if (!contactgroups_.empty()) {
		req["contact_groups"] = contactgroups_;
	}


	Mongoose::Client query(op5_url + "/api/config/host");
	boost::shared_ptr<Mongoose::Response> response = query.fetch("POST", make_header(op5_username, op5_password), json_spirit::write(req));

	if (!is_200(response)) {
		NSC_LOG_ERROR("Failed to add host: " + host + ": " + get_error(response));
		return false;
	}
	return true;
}

bool Op5Client::remove_host(std::string host) {

	Mongoose::Client query(op5_url + "/api/config/host/" + host);
	boost::shared_ptr<Mongoose::Response> response = query.fetch("DELETE", make_header(op5_username, op5_password), "");

	if (!is_200(response)) {
		NSC_LOG_ERROR("Failed to delete host: " + host + ": " + get_error(response));
		return false;
	}
	return true;
}

bool Op5Client::save_config() {
	Mongoose::Client query(op5_url + "/api/config/change");
	boost::shared_ptr<Mongoose::Response> response = query.fetch("POST", make_header(op5_username, op5_password), "");

	if (!is_200(response)) {
		NSC_LOG_ERROR("Failed to save configuration: " + get_error(response));
		return false;
	}
	return true;
}

bool Op5Client::send_host_check(std::string host, int status_code, std::string msg, std::string &status, bool create_if_missing) {

	json_spirit::Object req;
	req["host_name"] = host;
	req["status_code"] = status_code;
	req["plugin_output"] = msg;

	Mongoose::Client query(op5_url + "/api/command/PROCESS_HOST_CHECK_RESULT");
	boost::shared_ptr<Mongoose::Response> response = query.fetch("POST", make_header(op5_username, op5_password), json_spirit::write(req));

	if (!is_200(response)) {
		status = "Failed to submit host check to " + host + ": " + get_error(response);
		return false;
	}
	status = "Submitted host status to " + host;
	return true;
}

bool Op5Client::send_service_check(std::string host, std::string service, int status_code, std::string msg, std::string &status, bool create_if_missing) {

	json_spirit::Object req;
	req["host_name"] = host;
	req["service_description"] = service;
	req["status_code"] = status_code;
	req["plugin_output"] = msg;

	Mongoose::Client query(op5_url + "/api/command/PROCESS_SERVICE_CHECK_RESULT");
	boost::shared_ptr<Mongoose::Response> response = query.fetch("POST", make_header(op5_username, op5_password), json_spirit::write(req));

	if (is_404(response)) {
		if (create_if_missing) {
			add_service(hostname_, service);
			save_config();
			return send_service_check(host, service, status_code, msg, status, false);
		}
		status = "Service " + service + " does not exist on " + host;
		return false;
	}
	if (!is_200(response)) {
		status = "Failed to submit " + service + " result: " + host + ": " + get_error(response);
		return false;
	}
	status = "Submitted " + service + " to " + host;
	return true;
}

bool Op5Client::add_service(std::string host, std::string service) {

	json_spirit::Object req;
	req["service_description"] = service;
	req["host_name"] = host;
	req["check_command"] = "check-host-alive";
	req["active_checks_enabled"] = 0;
	req["freshness_threshold"] = 600;

	Mongoose::Client query(op5_url + "/api/config/service");
	boost::shared_ptr<Mongoose::Response> response = query.fetch("POST", make_header(op5_username, op5_password), json_spirit::write(req));

	if (!is_200(response)) {
		NSC_LOG_ERROR("Failed to add service " + service + " to " + host + ": " + get_error(response));
		return false;
	}
	return true;
}

bool Op5Client::add_host_to_service(std::string service, std::string host, std::string &hosts_string) {
	json_spirit::Object req;
	if (hosts_string.length() > 0) {
		hosts_string += "," + host;
	} else {
		hosts_string = host;
	}
	req["host_name"] = host;

	Mongoose::Client query(op5_url + "/api/config/service/" + service);
	boost::shared_ptr<Mongoose::Response> response = query.fetch("PATCH", make_header(op5_username, op5_password), json_spirit::write(req));

	if (!is_200(response)) {
		NSC_LOG_ERROR("Failed to add service " + service + " to " + host + ": " + get_error(response));
		return false;
	}
	return true;
}


bool Op5Client::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {
	try {
		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias("op5", alias);
// 		client_.set_path(settings.alias().get_settings_path("commands"));
		std::string interval;

		settings.alias().add_path_to_settings()
			("Op5 Configuration", "Section for the Op5 server")

			("commands", sh::fun_values_path(boost::bind(&Op5Client::add_command, this, _1, _2)),
				"Op5 passive Commands", "",
				"Passive commands", "Passive commands")
			;

		settings.alias().add_key_to_settings()
			("hostname", sh::string_key(&hostname_, "auto"),
				"HOSTNAME", "The host name of this monitored computer.\nSet this to auto (default) to use the windows name of the computer.\n\n"
				"auto\tHostname\n"
				"${host}\tHostname\n"
				"${host_lc}\nHostname in lowercase\n"
				"${host_uc}\tHostname in uppercase\n"
				"${domain}\tDomainname\n"
				"${domain_lc}\tDomainname in lowercase\n"
				"${domain_uc}\tDomainname in uppercase\n"
				)

			("channel", sh::string_key(&channel_, "op5"),
				"CHANNEL", "The channel to listen to.")

			("server", sh::string_key(&op5_url, ""),
				"Op5 base url", "The op5 base url i.e. the url of the Op5 monitor REST API for instance https://monitor.mycompany.com")
			("user", sh::string_key(&op5_username, ""),
				"Op5 user", "The user to authenticate as")
			("password", sh::string_key(&op5_password, ""),
				"Op5 password", "The password for the user to authenticate as")

			("interval", sh::string_key(&interval, "5m"),
			"Check interval", "How often to submit passive check results you can use an optional suffix to denote time (s, m, h)")

			("remove", sh::bool_key(&deregister, "false"),
			"Remove checks on exit", "If we should remove all checks when NSClient++ shuts down (for truly elastic scenarios)")

			("hostgroups", sh::string_key(&hostgroups_, ""),
			"Host groups", "A coma separated list of host groups to add to this host when registering it in monitor")

			("contactgroups", sh::string_key(&contactgroups_, ""),
			"Contact groups", "A coma separated list of contact groups to add to this host when registering it in monitor")

			;

		settings.register_all();
		settings.notify();

		//client_.finalize(get_settings_proxy());

		nscapi::core_helper core(get_core(), get_id());
		core.register_channel(channel_);

		if (hostname_ == "auto") {
			hostname_ = boost::asio::ip::host_name();
		} else if (hostname_ == "auto-lc") {
			hostname_ = boost::asio::ip::host_name();
			std::transform(hostname_.begin(), hostname_.end(), hostname_.begin(), ::tolower);
		} else if (hostname_ == "auto-uc") {
			hostname_ = boost::asio::ip::host_name();
			std::transform(hostname_.begin(), hostname_.end(), hostname_.begin(), ::toupper);
		} else {
			str::utils::token dn = str::utils::getToken(boost::asio::ip::host_name(), '.');

			try {
				boost::asio::io_service svc;
				boost::asio::ip::tcp::resolver resolver(svc);
				boost::asio::ip::tcp::resolver::query query(boost::asio::ip::host_name(), "");
				boost::asio::ip::tcp::resolver::iterator iter = resolver.resolve(query), end;

				std::string s;
				while (iter != end) {
					s += iter->host_name();
					s += " - ";
					s += iter->endpoint().address().to_string();
					iter++;
				}
			} catch (const std::exception& e) {
				NSC_LOG_ERROR_EXR("Failed to resolve: ", e);
			}

			str::utils::replace(hostname_, "${host}", dn.first);
			str::utils::replace(hostname_, "${domain}", dn.second);
			std::transform(dn.first.begin(), dn.first.end(), dn.first.begin(), ::toupper);
			std::transform(dn.second.begin(), dn.second.end(), dn.second.begin(), ::toupper);
			str::utils::replace(hostname_, "${host_uc}", dn.first);
			str::utils::replace(hostname_, "${domain_uc}", dn.second);
			std::transform(dn.first.begin(), dn.first.end(), dn.first.begin(), ::tolower);
			std::transform(dn.second.begin(), dn.second.end(), dn.second.begin(), ::tolower);
			str::utils::replace(hostname_, "${host_lc}", dn.first);
			str::utils::replace(hostname_, "${domain_lc}", dn.second);
		}
		//client_.set_sender(hostname_);


		if (mode == NSCAPI::normalStart) {
			NSC_DEBUG_MSG("Registring host " + hostname_ + " with op5");
			register_host(hostname_);


			Plugin::ExecuteRequestMessage rm;
			Plugin::ExecuteRequestMessage::Request *payload = rm.add_payload();

			payload->set_command("add");
			payload->add_arguments("--interval");
			payload->add_arguments("5m");
			payload->add_arguments("--command");
			payload->add_arguments("check_cpu");
			payload->add_arguments("--alias");
			payload->add_arguments("host_check");

			std::string pb_response;
			get_core()->exec_command("Scheduler", rm.SerializeAsString(), pb_response);
			Plugin::ExecuteResponseMessage resp;
			resp.ParseFromString(pb_response);

		}

	} catch (nsclient::nsclient_exception &e) {
		NSC_LOG_ERROR_EXR("NSClient API exception: ", e);
		return false;
	} catch (std::exception &e) {
		NSC_LOG_ERROR_EXR("NSClient API exception: ", e);
		return false;
	} catch (...) {
		NSC_LOG_ERROR_EX("NSClient API exception: ");
		return false;
	}
	return true;
}

void Op5Client::register_host(std::string host) {
	if (!has_host(host)) {
		NSC_DEBUG_MSG("Adding host");
		add_host(host);
		NSC_DEBUG_MSG("Saving config");
		save_config();
	}
}

void Op5Client::deregister_host(std::string host) {
	if (has_host(host)) {
		NSC_DEBUG_MSG("Adding host");
		remove_host(host);
		NSC_DEBUG_MSG("Saving config");
		save_config();
	}
}

void Op5Client::add_command(std::string key, std::string arg) {
	try {
// 		client_.add_target(get_settings_proxy(), key, arg);
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to add target: " + key, e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to add target: " + key);
	}
}

/**
 * Unload (terminate) module.
 * Attempt to stop the background processing thread.
 * @return true if successfully, false if not (if not things might be bad)
 */
bool Op5Client::unloadModule() {
	if (deregister) {
		deregister_host(hostname_);
	}
	return true;
}

bool Op5Client::commandLineExec(const int target_mode, const Plugin::ExecuteRequestMessage &request, Plugin::ExecuteResponseMessage &response) {
	return true;
}

void Op5Client::handleNotification(const std::string &, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage *response_message) {

	BOOST_FOREACH(const ::Plugin::QueryResponseMessage_Response &p, request_message.payload()) {
		std::string msg = nscapi::protobuf::functions::query_data_to_nagios_string(p, nscapi::protobuf::functions::no_truncation);
		std::string alias = p.alias();
		if (alias.empty())
			alias = p.command();
		int result = nscapi::protobuf::functions::gbp_to_nagios_status(p.result());
		if (alias == "host_check") {
			std::string status;
			if (!send_host_check(hostname_, result, msg, status)) {
				nscapi::protobuf::functions::set_response_bad(*response_message->add_payload(), status);
			} else {
				nscapi::protobuf::functions::set_response_good(*response_message->add_payload(), status);
			}
		} else {
			std::string status;
			if (!send_service_check(hostname_, alias, result, msg, status)) {
				nscapi::protobuf::functions::set_response_bad(*response_message->add_payload(), status);
			} else {
				nscapi::protobuf::functions::set_response_good(*response_message->add_payload(), status);
			}
		}
	}
}
