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

#include <json_spirit.h>

#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/nscapi_protobuf_nagios.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/macros.hpp>

#include <str/utils.hpp>
#include <str/format.hpp>

#include <Client.hpp>

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

#define HTTP_HDR_AUTH "Authorization"
#define HTTP_HDR_AUTH_BASIC "Basic "

namespace po = boost::program_options;
namespace sh = nscapi::settings_helper;
namespace bai = boost::asio::ip;


bool Op5Client::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {
	try {
		typedef std::map<std::string, std::string> def_check_type;
		def_check_type default_checks;
		default_checks["CPU Load"] = "check_cpu";
		default_checks["Memory Usage"] = "check_memory";
 		default_checks["Disk Read Average Latency"] = "check_pdh \"counter=\\\\LogicalDisk(*)\\\\Avg. Disk sec/Read\" show-all \"crit=value > 25\" \"warn=value > 20\"";
 		default_checks["Disk Write Average Latency"] = "check_pdh \"counter=\\\\LogicalDisk(*)\\\\Avg. Disk sec/Write\" show-all \"crit=value > 25\" \"warn=value > 20\"";
		default_checks["Disk Usage"] = "check_drivesize";

		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias("op5", alias);
		std::string interval;
		bool defChecks = true;
		op5_config config;


		settings.alias().add_path_to_settings()
			("Op5 Configuration", "Section for the Op5 server")

			("checks", sh::string_map_path(&config.checks),
				"Op5 passive Commands", "",
				"Passive commands", "Passive commands")
			;

		settings.alias().add_key_to_settings()
			("hostname", sh::string_key(&config.hostname, "auto"),
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

			("server", sh::string_key(&config.url, ""),
				"Op5 base url", "The op5 base url i.e. the url of the Op5 monitor REST API for instance https://monitor.mycompany.com")
			("user", sh::string_key(&config.username, ""),
				"Op5 user", "The user to authenticate as")
			("password", sh::string_key(&config.password, ""),
				"Op5 password", "The password for the user to authenticate as")

			("interval", sh::string_key(&interval, "5m"),
			"Check interval", "How often to submit passive check results you can use an optional suffix to denote time (s, m, h)")

			("remove", sh::bool_key(&config.deregister, false),
			"Remove checks on exit", "If we should remove all checks when NSClient++ shuts down (for truly elastic scenarios)")

			("default checks", sh::bool_key(&defChecks, true),
			"Install default checks", "Set to false to disable default checks")

			("hostgroups", sh::string_key(&config.hostgroups, ""),
			"Host groups", "A coma separated list of host groups to add to this host when registering it in monitor")

			("contactgroups", sh::string_key(&config.contactgroups, ""),
			"Contact groups", "A coma separated list of contact groups to add to this host when registering it in monitor")

			;

		settings.register_all();
		settings.notify();

		if (defChecks) {
			BOOST_FOREACH(const def_check_type::value_type &v, default_checks) {
				if (config.checks.find(v.first) == config.checks.end()) {
					config.checks[v.first] = v.second;
				}
			}
		}

		nscapi::core_helper core(get_core(), get_id());
		core.register_channel(channel_);

		if (config.hostname == "auto") {
			config.hostname = boost::asio::ip::host_name();
		} else if (config.hostname == "auto-lc") {
			config.hostname = boost::asio::ip::host_name();
			std::transform(config.hostname.begin(), config.hostname.end(), config.hostname.begin(), ::tolower);
		} else if (config.hostname == "auto-uc") {
			config.hostname = boost::asio::ip::host_name();
			std::transform(config.hostname.begin(), config.hostname.end(), config.hostname.begin(), ::toupper);
		} else {
			str::utils::token dn = str::utils::getToken(boost::asio::ip::host_name(), '.');

			try {
				boost::asio::io_service svc;
				bai::tcp::resolver resolver(svc);
				bai::tcp::resolver::query query(boost::asio::ip::host_name(), "");
				bai::tcp::resolver::iterator iter = resolver.resolve(query), end;

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

			str::utils::replace(config.hostname, "${host}", dn.first);
			str::utils::replace(config.hostname, "${domain}", dn.second);
			std::transform(dn.first.begin(), dn.first.end(), dn.first.begin(), ::toupper);
			std::transform(dn.second.begin(), dn.second.end(), dn.second.begin(), ::toupper);
			str::utils::replace(config.hostname, "${host_uc}", dn.first);
			str::utils::replace(config.hostname, "${domain_uc}", dn.second);
			std::transform(dn.first.begin(), dn.first.end(), dn.first.begin(), ::tolower);
			std::transform(dn.second.begin(), dn.second.end(), dn.second.begin(), ::tolower);
			str::utils::replace(config.hostname, "${host_lc}", dn.first);
			str::utils::replace(config.hostname, "${domain_lc}", dn.second);
		}

		if (mode == NSCAPI::normalStart) {
			config.interval = str::format::stox_as_time_sec<unsigned long long>(interval, "s");
			client.reset(new op5_client(get_core(), get_id(), config));
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

/**
 * Unload (terminate) module.
 * Attempt to stop the background processing thread.
 * @return true if successfully, false if not (if not things might be bad)
 */
bool Op5Client::unloadModule() {
	if (client) {
		client->stop();
	}
	return true;
}

bool Op5Client::commandLineExec(const int target_mode, const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response, const Plugin::ExecuteRequestMessage &request_message) {
	std::string command = request.command();
	if (command == "op5" && request.arguments_size() > 0)
		command = request.arguments(0);
	else if (target_mode == NSCAPI::target_module && request.arguments_size() > 0)
		command = request.arguments(0);
	else if (command.empty() && target_mode == NSCAPI::target_module)
		command = "help";
	if (command == "install")
		return cli_install(request, response);
	if (command == "add" || command == "add-check")
		return cli_add(request, response);
	else if (target_mode == NSCAPI::target_module) {
		nscapi::protobuf::functions::set_response_bad(*response, "Usage: nscp op5 [install|add|add-check] --help");
		return true;
	}
	return false;
}



bool Op5Client::cli_install(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response) {
	namespace po = boost::program_options;
	namespace pf = nscapi::protobuf::functions;
	po::variables_map vm;
	po::options_description desc;
	std::string user, password, server, hostgroups, contactgroups, interval;

	desc.add_options()
		("help", "Show help.")

		("user", po::value<std::string>(&user),
		"The username to login as")

		("password", po::value<std::string>(&password),
		"The password to login with")

		("server", po::value<std::string>(&server),
		"The base url of the monitor server usually https://<IP|HOST> of the monitoring server")

		("hostgroups", po::value<std::string>(&hostgroups),
		"A number of hostgroups to add to the server in op5.")

		("contactgroups", po::value<std::string>(&contactgroups),
		"A number of hostgroups to add to the server in op5.")

		("interval", po::value<std::string>(&interval),
		"Time in between sending statuses can use an optional time prefix (s, m, h, ...).")

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

		const std::string path = "/settings/op5";

		pf::settings_query q(get_id());
		q.get(path, "user", "");
		q.get(path, "password", "");
		q.get(path, "server", "");
		q.get(path, "hostgroups", "");
		q.get(path, "contactgroups", "");
		q.get(path, "interval", "");

		get_core()->settings_query(q.request(), q.response());
		if (!q.validate_response()) {
			nscapi::protobuf::functions::set_response_bad(*response, q.get_response_error());
			return true;
		}
		bool old = false;
		BOOST_FOREACH(const pf::settings_query::key_values &val, q.get_query_key_response()) {
			old = true;
			if (val.matches(path, "user") && user.empty())
				user = val.get_string();
			else if (val.matches(path, "password") && password.empty())
				password = val.get_string();
			else if (val.matches(path, "server") && server.empty())
				server = val.get_string();
			else if (val.matches(path, "hostgroups") && hostgroups.empty())
				hostgroups = val.get_string();
			else if (val.matches(path, "contactgroups") && contactgroups.empty())
				contactgroups = val.get_string();
			else if (val.matches(path, "interval") && interval.empty())
				interval = val.get_string();
		}

		std::stringstream result;
		nscapi::protobuf::functions::settings_query s(get_id());
		result << "Sending status every " << str::format::stox_as_time_sec<unsigned long long>(interval, "s") << " seconds to " << server << " as " << user << " identified by " << password << std::endl;
		s.set(path, "server", server);
		s.set(path, "user", user);
		s.set(path, "password", password);
		s.set(path, "interval", interval);
		if (!hostgroups.empty()) {
			result << "The following hostgroups will be added: " << hostgroups << std::endl;
			s.set(path, "hostgroups", hostgroups);
		}
		if (!contactgroups.empty()) {
			result << "The following contactgroups will be added: " << contactgroups << std::endl;
			s.set(path, "contactgroups", contactgroups);
		}
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



bool Op5Client::cli_add(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response) {
	namespace po = boost::program_options;
	namespace pf = nscapi::protobuf::functions;
	po::variables_map vm;
	po::options_description desc;
	std::string alias, command;

	desc.add_options()
		("help", "Show help.")

		("alias", po::value<std::string>(&alias),
		"The alias (service name) of the check")

		("command", po::value<std::string>(&command),
		"The command to execute in NSClient++")

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

		const std::string path = "/settings/op5/checks";

		pf::settings_query q(get_id());
		q.get(path, alias, command);

		get_core()->settings_query(q.request(), q.response());
		if (!q.validate_response()) {
			nscapi::protobuf::functions::set_response_bad(*response, q.get_response_error());
			return true;
		}
		bool old = false;
		BOOST_FOREACH(const pf::settings_query::key_values &val, q.get_query_key_response()) {
			old = true;
			if (val.matches(path, alias) && command.empty())
				command = val.get_string();
		}

		std::stringstream result;
		nscapi::protobuf::functions::settings_query s(get_id());
		result << "Adding check " << alias << " as " << command << std::endl;
		s.set(path, alias, command);
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

void Op5Client::handleNotification(const std::string &, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage *response_message) {

	if (!client) {
		nscapi::protobuf::functions::set_response_bad(*response_message->add_payload(), "Invalid op5 configuration");
		return;
	}
	BOOST_FOREACH(const ::Plugin::QueryResponseMessage_Response &p, request_message.payload()) {
		std::string msg = nscapi::protobuf::functions::query_data_to_nagios_string(p, nscapi::protobuf::functions::no_truncation);
		std::string alias = p.alias();
		if (alias.empty())
			alias = p.command();
		int result = nscapi::protobuf::functions::gbp_to_nagios_status(p.result());
		std::string status;
		if (!client->send_a_check(alias, result, msg, status)) {
			nscapi::protobuf::functions::set_response_bad(*response_message->add_payload(), status);
		} else {
			nscapi::protobuf::functions::set_response_good(*response_message->add_payload(), status);
		}

	}
}
