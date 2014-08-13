/**************************************************************************
*   Copyright (C) 2004-2007 by Michael Medin <michael@medin.name>         *
*                                                                         *
*   This code is part of NSClient++ - http://trac.nakednuns.org/nscp      *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/
#include "NRPEClient.h"

#include <time.h>
#include <strEx.h>

#include <settings/client/settings_client.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>

namespace sh = nscapi::settings_helper;

/**
 * Default c-tor
 * @return 
 */
NRPEClient::NRPEClient() {}

/**
 * Default d-tor
 * @return 
 */
NRPEClient::~NRPEClient() {}

bool NRPEClient::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode) {

	try {

		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias("NRPE", alias, "client");
		target_path = settings.alias().get_settings_path("targets");

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

		nscapi::core_helper::core_proxy core(get_core(), get_id());
		targets.add_samples(get_settings_proxy(), target_path);
		targets.ensure_default(get_settings_proxy(), target_path);
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
		targets.add(get_settings_proxy(), target_path , key, arg);
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to add target: " + key, e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to add target: " + key);
	}
}

void NRPEClient::add_command(std::string name, std::string args) {
	try {
		nscapi::core_helper::core_proxy core(get_core(), get_id());
		std::string key = commands.add_command(name, args);
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

struct client_handler : public socket_helpers::client::client_handler {
	void log_debug(std::string file, int line, std::string msg) const {
		if (GET_CORE()->should_log(NSCAPI::log_level::debug)) {
			GET_CORE()->log(NSCAPI::log_level::debug, file, line, msg);
		}
	}
	void log_error(std::string file, int line, std::string msg) const {
		if (GET_CORE()->should_log(NSCAPI::log_level::error)) {
			GET_CORE()->log(NSCAPI::log_level::error, file, line, msg);
		}
	}
	std::string expand_path(std::string path) {
		return GET_CORE()->expand_path(path);
	}

};

void NRPEClient::query_fallback(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response, const Plugin::QueryRequestMessage &request_message) {
	client::configuration config(nrpe_client::command_prefix, 
		boost::shared_ptr<nrpe_client::clp_handler_impl>(new nrpe_client::clp_handler_impl(boost::shared_ptr<socket_helpers::client::client_handler>(new client_handler()))), 
		boost::shared_ptr<nrpe_client::target_handler>(new nrpe_client::target_handler(targets)));
	nrpe_client::setup(config, request_message.header());
	commands.parse_query(nrpe_client::command_prefix, nrpe_client::default_command, request.command(), config, request, *response, request_message);
}

void NRPEClient::nrpe_forward(const std::string &command, Plugin::QueryRequestMessage &request, Plugin::QueryResponseMessage *response) {
	client::configuration config(nrpe_client::command_prefix, 
		boost::shared_ptr<nrpe_client::clp_handler_impl>(new nrpe_client::clp_handler_impl(boost::shared_ptr<socket_helpers::client::client_handler>(new client_handler()))), 
		boost::shared_ptr<nrpe_client::target_handler>(new nrpe_client::target_handler(targets)));
	nrpe_client::setup(config, request.header());
	commands.forward_query(config, request, *response);
}

bool NRPEClient::commandLineExec(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response, const Plugin::ExecuteRequestMessage &request_message) {
	if (request.arguments_size() > 0 && request.arguments(0) == "install")
		return install_server(request, response);
	client::configuration config(nrpe_client::command_prefix, 
		boost::shared_ptr<nrpe_client::clp_handler_impl>(new nrpe_client::clp_handler_impl(boost::shared_ptr<socket_helpers::client::client_handler>(new client_handler()))), 
		boost::shared_ptr<nrpe_client::target_handler>(new nrpe_client::target_handler(targets)));
	nrpe_client::setup(config, request_message.header());
	return commands.parse_exec(nrpe_client::command_prefix, nrpe_client::default_command, request.command(), config, request, *response, request_message);
}

void NRPEClient::handleNotification(const std::string &, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage *response_message) {
	client::configuration config(nrpe_client::command_prefix);
	config.target_lookup = boost::shared_ptr<nrpe_client::target_handler>(new nrpe_client::target_handler(targets)); 
	config.handler = boost::shared_ptr<nrpe_client::clp_handler_impl>(new nrpe_client::clp_handler_impl(boost::shared_ptr<socket_helpers::client::client_handler>(new client_handler())));
	nrpe_client::setup(config, request_message.header());
	commands.forward_submit(config, request_message, *response_message);
}


bool NRPEClient::install_server(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response) {

	namespace po = boost::program_options;
	namespace pf = nscapi::protobuf::functions;
	po::variables_map vm;
	po::options_description desc;
	std::string allowed_hosts, cert, key, arguments = "false", chipers, insecure;
	unsigned int length = 1024;
	const std::string path = "/settings/NRPE/server";

	pf::settings_query q(get_id());
	q.get("/settings/default", "allowed hosts", "127.0.0.1");
	q.get(path, "insecure", "false");
	q.get(path, "certificate", "${certificate-path}/certificate.pem");
	q.get(path, "certificate key", "${certificate-path}/certificate_key.pem");
	q.get(path, "allow arguments", false);
	q.get(path, "allow nasty characters", false);
	q.get(path, "allowed ciphers", "");

	
	get_core()->settings_query(q.request(), q.response());
	if (!q.validate_response()) {
		nscapi::protobuf::functions::set_response_bad(*response, q.get_response_error());
		return true;
	}
	BOOST_FOREACH(const pf::settings_query::key_values &val, q.get_query_key_response()) {
		if (val.path == "/settings/default" && val.key && *val.key == "allowed hosts")
			allowed_hosts = val.get_string();
		else if (val.path == path && val.key && *val.key == "certificate")
			cert = val.get_string();
		else if (val.path == path && val.key && *val.key == "certificate key")
			key = val.get_string();
		else if (val.path == path && val.key && *val.key == "allowed ciphers")
			chipers = val.get_string();
		else if (val.path == path && val.key && *val.key == "insecure")
			insecure = val.get_string();
		else if (val.path == path && val.key && *val.key == "allow arguments" && val.get_bool())
			arguments = "true";
		else if (val.path == path && val.key && *val.key == "allow nasty characters" && val.get_bool())
			arguments = "safe";
	}
	if (chipers == "ADH")
		insecure = "true";
	if (chipers == "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH") 
		insecure = "false";

	desc.add_options()
		("help", "Show help.")

		("allowed-hosts,h", po::value<std::string>(&allowed_hosts)->default_value(allowed_hosts), 
		"Set which hosts are allowed to connect")

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

		nscapi::protobuf::functions::settings_query s(get_id());
		result << "Enabling NRPE via SSH from: " << allowed_hosts << std::endl;
		s.set("/settings/default", "allowed hosts", allowed_hosts);
		s.set("/modules", "NRPEServer", "enabled");
		s.set("/settings/NRPE/server", "ssl", "true");
		if (insecure == "true") {
			result << "WARNING: NRPE is currently insecure." << std::endl;
			s.set("/settings/NRPE/server", "insecure", "true");
			s.set("/settings/NRPE/server", "allowed ciphers", "ADH");
		} else {
			result << "NRPE is currently reasonably secure using " << cert << " and " << key << "." << std::endl;
			s.set("/settings/NRPE/server", "insecure", "false");
			s.set("/settings/NRPE/server", "allowed ciphers", "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");
			s.set("/settings/NRPE/server", "certificate", cert);
			s.set("/settings/NRPE/server", "certificate key", key);
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
		s.set(path, "payload length", strEx::s::xtos(length));
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


//////////////////////////////////////////////////////////////////////////
// Parser setup/Helpers
//

