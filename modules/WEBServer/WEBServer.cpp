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

#include "WEBServer.h"

#include "web_cli_handler.hpp"
#include "token_store.hpp"

#include "static_controller.hpp"
#include "modules_controller.hpp"
#include "query_controller.hpp"
#include "scripts_controller.hpp"
#include "legacy_command_controller.hpp"
#include "legacy_controller.hpp"
#include "api_controller.hpp"
#include "log_controller.hpp"
#include "info_controller.hpp"
#include "settings_controller.hpp"
#include "login_controller.hpp"
#include "metrics_controller.hpp"

#include "error_handler.hpp"

#include <nscapi/nscapi_protobuf.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/nscapi_common_options.hpp>

#include <str/xtos.hpp>
#include <str/format.hpp>

#include <json_spirit.h>

#include <boost/program_options.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/unordered_set.hpp>
#include <boost/foreach.hpp>

#include <iostream>
#include <fstream>

namespace sh = nscapi::settings_helper;

using namespace std;
using namespace Mongoose;

WEBServer::WEBServer()
	: session(new session_manager_interface())
{
}
WEBServer::~WEBServer() {}


bool WEBServer::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {

	log_handler.reset(new error_handler());
	client.reset(new client::cli_client(client::cli_handler_ptr(new web_cli_handler(log_handler, get_core(), get_id()))));

	sh::settings_registry settings(get_settings_proxy());
	settings.set_alias("WEB", alias, "server");

	std::string port;
	std::string certificate;
	std::string admin_password;
	int threads;

	role_map roles;

	std::string role_path = settings.alias().get_settings_path("roles");
	std::string user_path = settings.alias().get_settings_path("users");

	users_.set_path(settings.alias().get_settings_path("users"));

	settings.alias().add_path_to_settings()
		("Web server", "Section for WEB (WEBServer.dll) (check_WEB) protocol options.")

		("users", sh::fun_values_path(boost::bind(&WEBServer::add_user, this, _1, _2)),
		"Web server users", "Users which can access the REST API",
		"REST USER", "")

		("roles", sh::string_map_path(&roles)
		, "Web server roles", "A list of roles and with coma separated list of access rights.")

		;
	settings.alias().add_key_to_settings()
		("port", sh::string_key(&port, "8443"),
		"Server port", "Port to use for WEB server.")

		("threads", sh::int_key(&threads, 10),
		"Server threads", "The number of threads in the sever response pool.")
		;
	settings.alias().add_key_to_settings()
		("certificate", sh::string_key(&certificate, "${certificate-path}/certificate.pem"),
			"TLS Certificate", "Ssl certificate to use for the ssl server")
		;

	settings.alias().add_parent("/settings/default").add_key_to_settings()

		("allowed hosts", nscapi::settings_helper::string_fun_key(boost::bind(&session_manager_interface::set_allowed_hosts, session, _1), "127.0.0.1"),
			"Allowed hosts", "A comma separated list of allowed hosts. You can use netmasks (/ syntax) or * to create ranges.")

		("cache allowed hosts", nscapi::settings_helper::bool_fun_key(boost::bind(&session_manager_interface::set_allowed_hosts_cache, session, _1), true),
			"Cache list of allowed hosts", "If host names (DNS entries) should be cached, improves speed and security somewhat but won't allow you to have dynamic IPs for your Nagios server.")

		("password", nscapi::settings_helper::string_key(&admin_password),
			DEFAULT_PASSWORD_NAME, DEFAULT_PASSWORD_DESC)

		;


	settings.register_all();
	settings.notify();
	certificate = get_core()->expand_path(certificate);

	users_.add_samples(get_settings_proxy());

	ensure_role(roles, settings, role_path, "legacy", "legacy", "legacy API");
	ensure_role(roles, settings, role_path, "full", "*", "Full access");
	ensure_role(roles, settings, role_path, "client", "public,info.get,info.get.version,queries.list,queries.get,queries.execute,login.get,modules.list", "read only");
	ensure_role(roles, settings, role_path, "view", "*", "Full access");

	ensure_user(settings, user_path, "admin", "full", admin_password, "Administrator");

	if (mode == NSCAPI::normalStart) {
		std::list<std::string> errors = session->boot();

		BOOST_FOREACH(const web_server::user_config_instance &o, users_.get_object_list()) {
			session->add_user(o->get_alias(), o->role, o->password);
		}
		BOOST_FOREACH(const role_map::value_type &v, roles) {
			session->add_grant(v.first, v.second);
		}

		socket_helpers::validate_certificate(certificate, errors);
		NSC_LOG_ERROR_LISTS(errors);
		std::string path = get_core()->expand_path("${web-path}");
		if (!boost::filesystem::is_regular_file(certificate) && port == "8443")
			port = "8080";
		if (boost::ends_with(port, "s")) {
			port = port.substr(0, port.length() - 1);
		}

		session->add_user("admin", "full", admin_password);

		server.reset(Mongoose::Server::make_server(port));
		if (!boost::filesystem::is_regular_file(certificate)) {
			NSC_LOG_ERROR("Certificate not found (disabling SSL): " + certificate);
		} else {
			NSC_DEBUG_MSG("Using certificate: " + certificate);
			server->setSsl(certificate.c_str());
		}

		server->registerController(new StaticController(session, path));

		server->registerController(new modules_controller(2, session, get_core(), get_id()));
		server->registerController(new query_controller(2, session, get_core(), get_id()));
		server->registerController(new scripts_controller(2, session, get_core(), get_id()));
		server->registerController(new log_controller(2, session, get_core(), get_id()));
		server->registerController(new info_controller(2, session, get_core(), get_id()));
		server->registerController(new settings_controller(2, session, get_core(), get_id()));
		server->registerController(new login_controller(2, session));
		server->registerController(new metrics_controller(2, session, get_core(), get_id()));

		server->registerController(new modules_controller(1, session, get_core(), get_id()));
		server->registerController(new query_controller(1, session, get_core(), get_id()));
		server->registerController(new scripts_controller(1, session, get_core(), get_id()));
		server->registerController(new log_controller(1, session, get_core(), get_id()));
		server->registerController(new info_controller(1, session, get_core(), get_id()));
		server->registerController(new settings_controller(1, session, get_core(), get_id()));
		server->registerController(new login_controller(1, session));

		server->registerController(new api_controller(session));

		server->registerController(new legacy_command_controller(session, get_core()));
		server->registerController(new legacy_controller(session, get_core(), get_id(), client));

		try {
			server->start(threads);
		} catch (const std::exception &e) {
			NSC_LOG_ERROR("Failed to start server: " + utf8::utf8_from_native(e.what()));
			return true;
		} catch (const std::string &e) {
			NSC_LOG_ERROR("Failed to start server: " + e);
			return true;
		}
		NSC_DEBUG_MSG("Loading webserver on port: " + port);
	}
	return true;
}

bool WEBServer::unloadModule() {
	try {
		if (server) {
			server->stop();
			server.reset();
		}
	} catch (...) {
		NSC_LOG_ERROR_EX("unload");
		return false;
	}
	return true;
}

void WEBServer::handleLogMessage(const Plugin::LogEntry::Entry &message) {
	using namespace boost::posix_time;
	using namespace boost::gregorian;

	error_handler_interface::log_entry entry;
	entry.line = message.line();
	entry.file = message.file();
	entry.message = message.message();
	entry.date = to_simple_string(second_clock::local_time());

	switch (message.level()) {
	case Plugin::LogEntry_Entry_Level_LOG_CRITICAL:
		entry.type = "critical";
		break;
	case Plugin::LogEntry_Entry_Level_LOG_DEBUG:
		entry.type = "debug";
		break;
	case Plugin::LogEntry_Entry_Level_LOG_ERROR:
		entry.type = "error";
		break;
	case Plugin::LogEntry_Entry_Level_LOG_INFO:
		entry.type = "info";
		break;
	case Plugin::LogEntry_Entry_Level_LOG_WARNING:
		entry.type = "warning";
		break;
	default:
		entry.type = "unknown";
	}
	session->add_log_message(message.level() == Plugin::LogEntry_Entry_Level_LOG_CRITICAL || message.level() == Plugin::LogEntry_Entry_Level_LOG_ERROR, entry);
}

bool WEBServer::commandLineExec(const int target_mode, const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response, const Plugin::ExecuteRequestMessage &request_message) {
	std::string command = request.command();
	if (command == "web" && request.arguments_size() > 0)
		command = request.arguments(0);
	else if (target_mode == NSCAPI::target_module && request.arguments_size() > 0)
		command = request.arguments(0);
	else if (command.empty() && target_mode == NSCAPI::target_module)
		command = "help";
	if (command == "install")
		return install_server(request, response);
	if (command == "add-user")
		return cli_add_user(request, response);
	if (command == "add-role")
		return cli_add_role(request, response);
	else if (command == "password")
		return password(request, response);
	else if (target_mode == NSCAPI::target_module) {
		nscapi::protobuf::functions::set_response_bad(*response, "Usage: nscp web [install|password|add-user|add-role] --help");
		return true;
	}
	return false;
}

bool WEBServer::cli_add_user(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response) {
	namespace po = boost::program_options;
	namespace pf = nscapi::protobuf::functions;
	po::variables_map vm;
	po::options_description desc;
	std::string user, password, role;

	desc.add_options()
		("help", "Show help.")

		("user", po::value<std::string>(&user),
		"The username to login as")

		("password", po::value<std::string>(&password),
		"The password to login with")

		("role", po::value<std::string>(&role),
		"The role to grant to the user")

		;

	try {
		nscapi::program_options::basic_command_line_parser cmd(request);
		cmd.options(desc);

		po::parsed_options parsed = cmd.run();
		po::store(parsed, vm);
		po::notify(vm);

		if (vm.count("help")) {
			nscapi::protobuf::functions::set_response_good(*response, nscapi::program_options::help(desc));
			return true;
		}

		const std::string path = "/settings/WEB/server/users/" + user;

		pf::settings_query q(get_id());
		q.get(path, "password", "");
		q.get(path, "role", "");

		get_core()->settings_query(q.request(), q.response());
		if (!q.validate_response()) {
			nscapi::protobuf::functions::set_response_bad(*response, q.get_response_error());
			return true;
		}
		bool old = false;
		BOOST_FOREACH(const pf::settings_query::key_values &val, q.get_query_key_response()) {
			old = true;
			if (val.matches(path, "password") && password.empty())
				password = val.get_string();
			else if (val.matches(path, "role") && role.empty())
				role = val.get_string();
		}

		std::stringstream result;
		if (password == "") {
			result << "WARNING: No password specified using a generated password" << std::endl;
			password = token_store::generate_token(32);
		}
		if (role == "") {
			result << "WARNING: No role specified using client" << std::endl;
			role = "client";
		}

		nscapi::protobuf::functions::settings_query s(get_id());
		result << "User " << user << " authenticated by " << password << " as " << role <<std::endl;
		s.set(path, "password", password);
		s.set(path, "role", role);
		s.save();
		get_core()->settings_query(s.request(), s.response());
		if (!s.validate_response()) {
			nscapi::protobuf::functions::set_response_bad(*response, s.get_response_error());
			return true;
		}
		nscapi::protobuf::functions::set_response_good(*response, result.str());
		return true;
	} catch (const std::exception &e) {
		nscapi::program_options::invalid_syntax(desc, request.command(), "Invalid command line: " + utf8::utf8_from_native(e.what()), *response);
		return true;
	} catch (...) {
		nscapi::program_options::invalid_syntax(desc, request.command(), "Unknown exception", *response);
		return true;
	}
}

bool WEBServer::cli_add_role(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response) {
	namespace po = boost::program_options;
	namespace pf = nscapi::protobuf::functions;
	po::variables_map vm;
	po::options_description desc;
	std::string role, grant;

	desc.add_options()
		("help", "Show help.")

		("role", po::value<std::string>(&role),
		"The role to update grants for")

		("grant", po::value<std::string>(&grant),
		"The grants to give to the role")

		;

	try {
		nscapi::program_options::basic_command_line_parser cmd(request);
		cmd.options(desc);

		po::parsed_options parsed = cmd.run();
		po::store(parsed, vm);
		po::notify(vm);

		if (vm.count("help")) {
			nscapi::protobuf::functions::set_response_good(*response, nscapi::program_options::help(desc));
			return true;
		}

		if (role == "") {
			nscapi::protobuf::functions::set_response_good(*response, nscapi::program_options::help(desc));
			return true;
		}

		std::stringstream result;

		const std::string path = "/settings/WEB/server/roles";

		pf::settings_query q(get_id());
		q.get(path, role, "");

		get_core()->settings_query(q.request(), q.response());
		if (!q.validate_response()) {
			nscapi::protobuf::functions::set_response_bad(*response, q.get_response_error());
			return true;
		}
		BOOST_FOREACH(const pf::settings_query::key_values &val, q.get_query_key_response()) {
			if (val.matches(path, role) && grant.empty()) {
				grant = val.get_string();
			}
		}

		nscapi::protobuf::functions::settings_query s(get_id());
		result << "Role " << role << std::endl;
		BOOST_FOREACH(const std::string &g, str::utils::split<std::list<std::string> >(grant, ",")) {
			result << " " << g << std::endl;
		}
		s.set(path, role, grant);
		s.save();
		get_core()->settings_query(s.request(), s.response());
		if (!s.validate_response()) {
			nscapi::protobuf::functions::set_response_bad(*response, s.get_response_error());
			return true;
		}
		nscapi::protobuf::functions::set_response_good(*response, result.str());
		return true;
	} catch (const std::exception &e) {
		nscapi::program_options::invalid_syntax(desc, request.command(), "Invalid command line: " + utf8::utf8_from_native(e.what()), *response);
		return true;
	} catch (...) {
		nscapi::program_options::invalid_syntax(desc, request.command(), "Unknown exception", *response);
		return true;
	}
}
bool WEBServer::install_server(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response) {
	namespace po = boost::program_options;
	namespace pf = nscapi::protobuf::functions;
	po::variables_map vm;
	po::options_description desc;
	std::string allowed_hosts, cert, key, port, password;
	const std::string path = "/settings/WEB/server";

	pf::settings_query q(get_id());
	q.get("/settings/default", "allowed hosts", "127.0.0.1");
	q.get("/settings/default", "password", "");
	q.get(path, "certificate", "${certificate-path}/certificate.pem");
	q.get(path, "certificate key", "");
	q.get(path, "port", "8443");

	get_core()->settings_query(q.request(), q.response());
	if (!q.validate_response()) {
		nscapi::protobuf::functions::set_response_bad(*response, q.get_response_error());
		return true;
	}
	BOOST_FOREACH(const pf::settings_query::key_values &val, q.get_query_key_response()) {
		if (val.matches("/settings/default", "allowed hosts"))
			allowed_hosts = val.get_string();
		else if (val.matches("/settings/default", "password"))
			password = val.get_string();
		else if (val.matches(path, "certificate"))
			cert = val.get_string();
		else if (val.matches(path, "certificate key"))
			key = val.get_string();
		else if (val.matches(path, "port"))
			port = val.get_string();
	}

	desc.add_options()
		("help", "Show help.")

		("allowed-hosts,h", po::value<std::string>(&allowed_hosts)->default_value(allowed_hosts),
			"Set which hosts are allowed to connect")

		("certificate", po::value<std::string>(&cert)->default_value(cert),
			"Length of payload (has to be same as on the server)")

		("certificate-key", po::value<std::string>(&key)->default_value(key),
			"Client certificate to use")

		("port", po::value<std::string>(&port)->default_value(port),
		"Port to use")

		("password", po::value<std::string>(&password)->default_value(password),
		"Password to use to authenticate (if none a generated password will be set)")

		;

	try {
		nscapi::program_options::basic_command_line_parser cmd(request);
		cmd.options(desc);

		po::parsed_options parsed = cmd.run();
		po::store(parsed, vm);
		po::notify(vm);

		if (vm.count("help")) {
			nscapi::protobuf::functions::set_response_good(*response, nscapi::program_options::help(desc));
			return true;
		}
		std::stringstream result;

		if (password == "") {
			result << "WARNING: No password specified using a generated password" << std::endl;
			password = token_store::generate_token(32);
		}

		bool https = false;
		nscapi::protobuf::functions::settings_query s(get_id());
		result << "Enabling WEB access from " << allowed_hosts << std::endl;
		s.set("/settings/default", "allowed hosts", allowed_hosts);
		s.set("/modules", "WEBServer", "enabled");
		if (port.find('s') != std::string::npos) {
			https = true;
			result << "HTTP(s) is enabled using " << get_core()->expand_path(cert);
			if (!key.empty())
				result << " and " << get_core()->expand_path(key);
			result << "." << std::endl;
		}
		if (!https) {
			cert = "";
			key = "";
		}
		s.set(path, "certificate", cert);
		s.set(path, "certificate key", key);
		result << "Point your browser to https://localhost:" << boost::replace_all_copy(port, "s", "") << std::endl;
		result << "Login using this password " << password << std::endl;
		s.set("/settings/default", "password", password);
		s.set(path, "port", port);
		s.save();
		get_core()->settings_query(s.request(), s.response());
		if (!s.validate_response()) {
			nscapi::protobuf::functions::set_response_bad(*response, s.get_response_error());
			return true;
		}
		nscapi::protobuf::functions::set_response_good(*response, result.str());
		return true;
	} catch (const std::exception &e) {
		nscapi::program_options::invalid_syntax(desc, request.command(), "Invalid command line: " + utf8::utf8_from_native(e.what()), *response);
		return true;
	} catch (...) {
		nscapi::program_options::invalid_syntax(desc, request.command(), "Unknown exception", *response);
		return true;
	}
}

bool WEBServer::password(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response) {
	namespace po = boost::program_options;
	namespace pf = nscapi::protobuf::functions;
	po::variables_map vm;
	po::options_description desc;

	std::string password;
	bool display = false, setweb = false;

	desc.add_options()
		("help", "Show help.")

		("set,s", po::value<std::string>(&password),
			"Set the new password")

		("display,d", po::bool_switch(&display),
			"Display the current configured password")

		("only-web", po::bool_switch(&setweb),
			"Set the password for WebServer only (if not specified the default password is used)")

		;
	try {
		nscapi::program_options::basic_command_line_parser cmd(request);
		cmd.options(desc);

		po::parsed_options parsed = cmd.allow_unregistered().run();
		po::store(parsed, vm);
		po::notify(vm);

		if (vm.count("help")) {
			nscapi::protobuf::functions::set_response_good(*response, nscapi::program_options::help(desc));
			return true;
		}
	} catch (const std::exception &e) {
		nscapi::program_options::invalid_syntax(desc, request.command(), "Invalid command line: " + utf8::utf8_from_native(e.what()), *response);
		return true;
	}

	if (display) {
		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias("WEB", "", "server");

		settings.alias().add_parent("/settings/default").add_key_to_settings()

			("password", sh::string_key(&password),
				"PASSWORD", "Password used to authenticate against server")

			;

		settings.register_all();
		settings.notify();
		if (password.empty())
			nscapi::protobuf::functions::set_response_good(*response, "No password set you will not be able to login");
		else
			nscapi::protobuf::functions::set_response_good(*response, "Current password: " + password);
	} else if (!password.empty()) {
		nscapi::protobuf::functions::settings_query s(get_id());
		if (setweb) {
			s.set("/settings/default", "password", password);
			s.set("/settings/WEB/server", "password", "");
		} else {
			s.set("/settings/WEB/server", "password", password);
		}

		s.save();
		get_core()->settings_query(s.request(), s.response());
		if (!s.validate_response()) {
			nscapi::protobuf::functions::set_response_bad(*response, s.get_response_error());
			return true;
		}
		nscapi::protobuf::functions::set_response_good(*response, "Password updated successfully, please restart nsclient++ for changes to affect.");
	} else {
		nscapi::protobuf::functions::set_response_bad(*response, nscapi::program_options::help(desc));
	}
	return true;
}

void build_metrics(json_spirit::Object &metrics, json_spirit::Object &metrics_list, const std::string trail, const Plugin::Common::MetricsBundle & b) {
	json_spirit::Object node;
	BOOST_FOREACH(const Plugin::Common::MetricsBundle &b2, b.children()) {
		build_metrics(node, metrics_list, trail + "." + b2.key(), b2);
	}
	BOOST_FOREACH(const Plugin::Common::Metric &v, b.value()) {
		const ::Plugin::Common_AnyDataType &value = v.value();
		if (value.has_int_data()) {
			node.insert(json_spirit::Object::value_type(v.key(), v.value().int_data()));
			metrics_list.insert(json_spirit::Object::value_type(trail + "." + v.key(), v.value().int_data()));
		} else if (value.has_string_data()) {
			node.insert(json_spirit::Object::value_type(v.key(), v.value().string_data()));
			metrics_list.insert(json_spirit::Object::value_type(trail + "." + v.key(), v.value().string_data()));
		} else if (value.has_float_data()) {
			node.insert(json_spirit::Object::value_type(v.key(), v.value().float_data()));
			metrics_list.insert(json_spirit::Object::value_type(trail + "." + v.key(), v.value().float_data()));
		} else {
			node.insert(json_spirit::Object::value_type(v.key(), "TODO"));
		}
	}
	metrics.insert(json_spirit::Object::value_type(b.key(), node));
}
void WEBServer::submitMetrics(const Plugin::MetricsMessage &response) {
	json_spirit::Object metrics, metrics_list;
	BOOST_FOREACH(const Plugin::MetricsMessage::Response &p, response.payload()) {
		BOOST_FOREACH(const Plugin::Common::MetricsBundle &b, p.bundles()) {
			build_metrics(metrics, metrics_list, b.key(), b);
		}
	}
	session->set_metrics(json_spirit::write(metrics), json_spirit::write(metrics_list));
	client->push_metrics(response);

}

void WEBServer::add_user(std::string key, std::string arg) {
	try {
		users_.add(get_settings_proxy(), key, arg);
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to add user: " + key, e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to add user: " + key);
	}
}

void WEBServer::ensure_role(role_map &roles, nscapi::settings_helper::settings_registry &settings, std::string role_path, std::string role, std::string value, std::string reason) {
	if (roles.find(role) == roles.end()) {
		roles[role] = value;
		settings.register_key(role_path, role, "Role for " + reason, "Default role for " + reason, value, false);
		settings.set_static_key(role_path, role, value);
	}
}

void WEBServer::ensure_user(nscapi::settings_helper::settings_registry &settings, std::string path, std::string user, std::string role, std::string password, std::string reason) {
	if (!session->has_user(user)) {
		session->add_user(user, role, password);
		std::string the_path = path + "/" + user;
		settings.register_key(the_path, "password", "Password for " + reason, "Password name for" + reason, password, false);
		settings.set_static_key(the_path, "password", password);
		settings.register_key(the_path, "role", "Role for " + reason, "Role name for" + reason, role, false);
		settings.set_static_key(the_path, "role", role);
	}
}
