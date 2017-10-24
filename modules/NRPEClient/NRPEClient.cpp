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

#include "NRPEClient.h"

#include <boost/filesystem.hpp>
#include <boost/make_shared.hpp>

#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>
#include <config.h>

#include "nrpe_client.hpp"
#include "nrpe_handler.hpp"

namespace sh = nscapi::settings_helper;

/**
 * Default c-tor
 * @return
 */
NRPEClient::NRPEClient() : client_("nrpe", boost::make_shared<nrpe_client::nrpe_client_handler<> >(), boost::make_shared<nrpe_handler::options_reader_impl>()) {}

/**
 * Default d-tor
 * @return
 */
NRPEClient::~NRPEClient() {}

bool NRPEClient::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode) {
	try {
		client_.clear();
		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias("NRPE", alias, "client");

		client_.set_path(settings.alias().get_settings_path("targets"));

		settings.alias().add_path_to_settings()
			("NRPE CLIENT SECTION", "Section for NRPE active/passive check module.")

			("handlers", sh::fun_values_path(boost::bind(&NRPEClient::add_command, this, _1, _2)),
				"CLIENT HANDLER SECTION", "",
				"TARGET", "For more configuration options add a dedicated section")

			("targets", sh::fun_values_path(boost::bind(&NRPEClient::add_target, this, _1, _2)),
				"REMOTE TARGET DEFINITIONS", "",
				"TARGET", "For more configuration options add a dedicated section")
			;

		settings.alias().add_key_to_settings()
			("channel", sh::string_key(&channel_, "NRPE"),
				"CHANNEL", "The channel to listen to.")

			;

		settings.register_all();
		settings.notify();

		client_.finalize(get_settings_proxy());

		nscapi::core_helper core(get_core(), get_id());
		core.register_channel(channel_);
	} catch (std::exception &e) {
		NSC_LOG_ERROR_EXR("loading", e);
		return false;
	} catch (...) {
		NSC_LOG_ERROR_EX("loading");
		return false;
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
// Settings helpers
//

void NRPEClient::add_target(std::string key, std::string arg) {
	try {
		client_.add_target(get_settings_proxy(), key, arg);
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to add target: " + key, e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to add target: " + key);
	}
}

void NRPEClient::add_command(std::string name, std::string args) {
	try {
		nscapi::core_helper core(get_core(), get_id());
		std::string key = client_.add_command(name, args);
		if (!key.empty())
			core.register_command(key.c_str(), "NRPE relay for: " + name);
	} catch (boost::program_options::validation_error &e) {
		NSC_LOG_ERROR_EXR("Failed to add command: " + name, e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to add command: " + name);
	}
}

/**
 * Unload (terminate) module.
 * Attempt to stop the background processing thread.
 * @return true if successfully, false if not (if not things might be bad)
 */
bool NRPEClient::unloadModule() {
	return true;
}

void NRPEClient::query_fallback(const Plugin::QueryRequestMessage &request_message, Plugin::QueryResponseMessage &response_message) {
	client_.do_query(request_message, response_message);
}

bool NRPEClient::commandLineExec(const int target_mode, const Plugin::ExecuteRequestMessage &request, Plugin::ExecuteResponseMessage &response) {
	BOOST_FOREACH(const Plugin::ExecuteRequestMessage::Request &payload, request.payload()) {
		if (payload.arguments_size() > 0 && payload.arguments(0) == "install") {
			Plugin::ExecuteResponseMessage::Response *rp = response.add_payload();
			return install_server(payload, rp);
		}
		if (payload.arguments_size() > 0 && payload.arguments(0) == "make-cert") {
			Plugin::ExecuteResponseMessage::Response *rp = response.add_payload();
			return make_cert(payload, rp);
		}
		if (payload.arguments_size() == 0 || payload.arguments(0) == "help") {
			Plugin::ExecuteResponseMessage::Response *rp = response.add_payload();
			nscapi::protobuf::functions::set_response_bad(*rp, "Usage: nscp nrpe [install|make-cert] --help");
			return true;
		}
	}
	if (target_mode == NSCAPI::target_module)
		return client_.do_exec(request, response, "check_nrpe");
	return false;
}

void NRPEClient::handleNotification(const std::string &, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage *response_message) {
	client_.do_submit(request_message, *response_message);
}

bool NRPEClient::install_server(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response) {
	namespace po = boost::program_options;
	namespace pf = nscapi::protobuf::functions;
	po::variables_map vm;
	po::options_description desc;
	std::string allowed_hosts, cert, key, arguments = "false", chipers, insecure = "true";
	unsigned int length = 1024;
	const std::string path = "/settings/NRPE/server";
	std::string verify = "peer-cert";
	std::string sslops = "";
	std::string port = "5666";

	pf::settings_query q(get_id());
	q.get("/settings/default", "allowed hosts", "127.0.0.1");
	q.get(path, "insecure", "false");
	q.get(path, "certificate", "${certificate-path}/certificate.pem");
	q.get(path, "certificate key", "");
	q.get(path, "allow arguments", false);
	q.get(path, "allow nasty characters", false);
	q.get(path, "allowed ciphers", "");
	q.get(path, "verify mode", verify);
	q.get(path, "ssl options", "");
	q.get(path, "port", "5666");

	get_core()->settings_query(q.request(), q.response());
	if (!q.validate_response()) {
		nscapi::protobuf::functions::set_response_bad(*response, q.get_response_error());
		return true;
	}
	std::list<pf::settings_query::key_values> values = q.get_query_key_response();
	BOOST_FOREACH(const pf::settings_query::key_values &val, values) {
		if (val.matches("/settings/default", "allowed hosts"))
			allowed_hosts = val.get_string();
		else if (val.matches(path, "certificate"))
			cert = val.get_string();
		else if (val.matches(path, "certificate key"))
			key = val.get_string();
		else if (val.matches(path, "allowed ciphers"))
			chipers = val.get_string();
		else if (val.matches(path, "insecure"))
			insecure = val.get_string();
		else if (val.matches(path, "allow arguments") && val.get_bool())
			arguments = "safe";
		else if (val.matches(path, "verify"))
			verify = val.get_string();
		else if (val.matches(path, "ssl options"))
			sslops = val.get_string();
		else if (val.matches(path, "port"))
			port = val.get_string();
	}
	BOOST_FOREACH(const pf::settings_query::key_values &val, values) {
		if (val.matches(path, "allow nasty characters")) {
			if (arguments == "safe" && val.get_bool())
				arguments = "all";
		}
	}

	std::stringstream result;
	if (chipers == "ADH")
		insecure = "true";
	if (chipers == "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH")
		insecure = "false";
	if (insecure == "false" && sslops != "no-sslv2,no-sslv3")
		result << "WARNING: Inconsistent ssl options will overwrite: " << sslops << " with no-sslv2,no-sslv3\n";
	if (insecure == "true" && sslops != "")
		result << "WARNING: Inconsistent ssl options will overwrite: " << sslops << " with \"\"\n";

	desc.add_options()
		("help", "Show help.")

		("allowed-hosts,h", po::value<std::string>(&allowed_hosts)->default_value(allowed_hosts),
			"Set which hosts are allowed to connect")

		("port", po::value<std::string>(&port)->default_value(port),
			"Set the port NRPE listens on")

		("certificate", po::value<std::string>(&cert)->default_value(cert),
			"Length of payload (has to be same as on the server)")

		("certificate-key", po::value<std::string>(&key)->default_value(key),
			"Client certificate to use")

		("insecure", po::value<std::string>(&insecure)->default_value(insecure)->implicit_value("true"),
			"Use \"old\" legacy NRPE.")

		("payload-length,l", po::value<unsigned int>(&length)->default_value(1024),
			"Length of payload (has to be same as on both the server and client)")

		("arguments", po::value<std::string>(&arguments)->default_value(arguments)->implicit_value("safe"),
			"Allow arguments. false=don't allow, safe=allow non escape chars, all=allow all arguments.")

		("verify", po::value<std::string>(&verify)->default_value(verify)->implicit_value("yes"),
			"")

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

		nscapi::protobuf::functions::settings_query s(get_id());
		result << "Enabling NRPE via SSL from: " << allowed_hosts << " on port " << port << std::endl;
		s.set("/settings/default", "allowed hosts", allowed_hosts);
		s.set(MAIN_MODULES_SECTION, "NRPEServer", "enabled");
		s.set("/settings/NRPE/server", "port", port);
		s.set("/settings/NRPE/server", "ssl", "true");
		if (insecure == "true") {
			result << "WARNING: NRPE is currently insecure." << std::endl;
			s.set("/settings/NRPE/server", "insecure", "true");
			s.set("/settings/NRPE/server", "allowed ciphers", "ADH");
			s.set("/settings/NRPE/server", "ssl options", "");
		} else {
			if (key.empty())
				result << "NRPE is currently reasonably secure using " << cert << "." << std::endl;
			else
				result << "NRPE is currently reasonably secure using " << cert << " and " << key << "." << std::endl;
			s.set("/settings/NRPE/server", "insecure", "false");
			s.set("/settings/NRPE/server", "allowed ciphers", "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");
			s.set("/settings/NRPE/server", "certificate", cert);
			s.set("/settings/NRPE/server", "certificate key", key);
			s.set("/settings/NRPE/server", "ssl options", "no-sslv2,no-sslv3");
		}
		if (arguments == "all" || arguments == "unsafe") {
			result << "UNSAFE Arguments are allowed." << std::endl;
			s.set(path, "allow arguments", "true");
			s.set(path, "allow nasty characters", "true");
		} else if (arguments == "safe" || arguments == "true") {
			result << "SAFE Arguments are allowed." << std::endl;
			s.set(path, "allow arguments", "true");
			s.set(path, "allow nasty characters", "false");
		} else {
			result << "Arguments are NOT allowed." << std::endl;
			s.set(path, "allow arguments", "false");
			s.set(path, "allow nasty characters", "false");
		}
		s.set(path, "payload length", str::xtos(length));
		if (length != 1024)
			result << "NRPE is using non standard payload length " << length << " please use same configuration in check_nrpe." << std::endl;
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

bool NRPEClient::make_cert(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response) {
	namespace po = boost::program_options;
	namespace pf = nscapi::protobuf::functions;
	po::variables_map vm;
	po::options_description desc;
	std::string cert, key;
	const std::string path = "/settings/NRPE/server";
	bool force = false;

	pf::settings_query q(get_id());
	q.get(path, "certificate", "${certificate-path}/certificate.pem");
	q.get(path, "certificate key", "");

	get_core()->settings_query(q.request(), q.response());
	if (!q.validate_response()) {
		nscapi::protobuf::functions::set_response_bad(*response, q.get_response_error());
		return true;
	}
	std::list<pf::settings_query::key_values> values = q.get_query_key_response();
	BOOST_FOREACH(const pf::settings_query::key_values &val, values) {
		if (val.matches(path, "certificate"))
			cert = val.get_string();
		else if (val.matches(path, "certificate key"))
			key = val.get_string();
	}

	desc.add_options()
		("help", "Show help.")

		("certificate", po::value<std::string>(&cert)->default_value(cert),
			"Length of payload (has to be same as on the server)")

		("certificate-key", po::value<std::string>(&key)->default_value(key),
			"Client certificate to use")

		("force", po::bool_switch(&force),
			"Overwrite existing certificates.")

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

		cert = get_core()->expand_path(cert);
		key = get_core()->expand_path(key);

		if (!force && (boost::filesystem::exists(cert) || boost::filesystem::exists(key))) {
			nscapi::protobuf::functions::set_response_bad(*response, "Certificate already exists, wont overwrite");
			return true;
		}

		socket_helpers::write_certs(cert, false);

		nscapi::protobuf::functions::set_response_good(*response, cert + " generated.");
		return true;
	} catch (const std::exception &e) {
		nscapi::program_options::invalid_syntax(desc, request.command(), "Invalid command line: " + utf8::utf8_from_native(e.what()), *response);
		return true;
	} catch (...) {
		nscapi::program_options::invalid_syntax(desc, request.command(), "Unknown exception", *response);
		return true;
	}
}

//////////////////////////////////////////////////////////////////////////
// Parser setup/Helpers
//