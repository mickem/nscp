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
#include "stdafx.h"
#include "NRPEClient.h"

#include <time.h>
#include <strEx.h>

#include <settings/client/settings_client.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_core_helper.hpp>

namespace sh = nscapi::settings_helper;

const std::string command_prefix("nrpe");
const std::string default_command("query");
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

bool NRPEClient::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {

	try {

		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias("NRPE", alias, "client");
		target_path = settings.alias().get_settings_path("targets");

		settings.alias().add_path_to_settings()
			("NRPE CLIENT SECTION", "Section for NRPE active/passive check module.")

			("handlers", sh::fun_values_path(boost::bind(&NRPEClient::add_command, this, _1, _2)), 
			"CLIENT HANDLER SECTION", "")

			("targets", sh::fun_values_path(boost::bind(&NRPEClient::add_target, this, _1, _2)), 
			"REMOTE TARGET DEFINITIONS", "")
			;

		settings.alias().add_key_to_settings()
			("channel", sh::string_key(&channel_, "NRPE"),
			"CHANNEL", "The channel to listen to.")

			;

		settings.register_all();
		settings.notify();

		nscapi::core_helper::core_proxy core(get_core(), get_id());
		targets.add_missing(get_settings_proxy(), target_path, "default", "", true);
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

std::string get_command(std::string alias, std::string command = "") {
	if (!alias.empty())
		return alias; 
	if (!command.empty())
		return command; 
	return "_NRPE_CHECK";
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

void NRPEClient::query_fallback(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response, const Plugin::QueryRequestMessage &request_message) {
	client::configuration config(command_prefix);
	setup(config, request_message.header());
	commands.parse_query(command_prefix, default_command, request.command(), config, request, *response, request_message);
}

void NRPEClient::nrpe_forward(const std::string &command, const Plugin::QueryRequestMessage &request, Plugin::QueryResponseMessage *response) {
	client::configuration config(command_prefix);
	setup(config, request.header());
	commands.forward_query(config, request, *response);
}

bool NRPEClient::commandLineExec(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response, const Plugin::ExecuteRequestMessage &request_message) {
	client::configuration config(command_prefix);
	setup(config, request_message.header());
	return commands.parse_exec(command_prefix, default_command, request.command(), config, request, *response, request_message);
}

void NRPEClient::handleNotification(const std::string &channel, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage *response_message) {
	client::configuration config(command_prefix);
	setup(config, request_message.header());
	commands.forward_submit(config, request_message, *response_message);
}


//////////////////////////////////////////////////////////////////////////
// Parser setup/Helpers
//

void NRPEClient::add_local_options(po::options_description &desc, client::configuration::data_type data) {
 	desc.add_options()
 		("no-ssl,n", po::value<bool>()->zero_tokens()->default_value(false)->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_bool_data, &data->recipient, "no ssl", _1)), 
		"Do not initial an ssl handshake with the server, talk in plain-text.")

		("certificate", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->recipient, "certificate", _1)), 
		"Length of payload (has to be same as on the server)")

		("dh", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->recipient, "dh", _1)), 
		"The pre-generated DH key (if ADH is used this will be your 'key' though it is not a secret key)")

		("certificate-key", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->recipient, "certificate key", _1)), 
		"Client certificate to use")

		("certificate-format", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->recipient, "certificate format", _1)), 
		"Client certificate format (default is PEM)")

		("ca", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->recipient, "ca", _1)), 
		"A file representing the Certificate authority used to validate peer certificates")

		("verify", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->recipient, "verify mode", _1)), 
		"Which verification mode to use: none: no verification, peer: that peer has a certificate, peer-cert: that peer has a valid certificate, ...")

		("allowed-ciphers", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->recipient, "allowed ciphers", _1)), 
		"Which ciphers are allowed for legacy reasons this defaults to ADH which is not secure preferably set this to DEFAULT which is better or a an even stronger cipher")

		("payload-length,l", po::value<unsigned int>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_int_data, &data->recipient, "payload length", _1)), 
		"Length of payload (has to be same as on the server)")

		("buffer-length", po::value<unsigned int>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_int_data, &data->recipient, "payload length", _1)), 
		"Same as payload-length (used for legacy reasons)")

 		("ssl", po::value<bool>()->zero_tokens()->default_value(false)->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_bool_data, &data->recipient, "ssl", _1)), 
		"Initial an ssl handshake with the server.")
 		;
}

void NRPEClient::setup(client::configuration &config, const ::Plugin::Common_Header& header) {
	boost::shared_ptr<clp_handler_impl> handler = boost::shared_ptr<clp_handler_impl>(new clp_handler_impl(this));
	add_local_options(config.local, config.data);

	config.data->recipient.id = header.recipient_id();
	config.default_command = default_command;
	std::string recipient = config.data->recipient.id;
	if (!targets.has_object(recipient)) {
		recipient = "default";
	}
	nscapi::targets::optional_target_object opt = targets.find_object(recipient);

	if (opt) {
		nscapi::targets::target_object t = *opt;
		nscapi::protobuf::functions::destination_container def = t.to_destination_container();
		config.data->recipient.apply(def);
	}
	config.data->host_self.id = "self";
	//config.data->host_self.host = hostname_;

	config.target_lookup = handler;
	config.handler = handler;
}

NRPEClient::connection_data NRPEClient::parse_header(const ::Plugin::Common_Header &header, client::configuration::data_type data) {
	nscapi::protobuf::functions::destination_container recipient;
	nscapi::protobuf::functions::parse_destination(header, header.recipient_id(), recipient, true);
	return connection_data(recipient, data->recipient);
}

//////////////////////////////////////////////////////////////////////////
// Parser implementations
//

int NRPEClient::clp_handler_impl::query(client::configuration::data_type data, const Plugin::QueryRequestMessage &request_message, Plugin::QueryResponseMessage &response_message) {
	const ::Plugin::Common_Header& request_header = request_message.header();
	connection_data con = parse_header(request_header, data);

	nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_header);

	for (int i=0;i<request_message.payload_size();i++) {
		std::string command = get_command(request_message.payload(i).alias(), request_message.payload(i).command());
		std::string data = command;
		for (int a=0;a<request_message.payload(i).arguments_size();a++) {
			data += "!" + request_message.payload(i).arguments(a);
		}
		boost::tuple<int,std::string> ret = instance->send(con, data);
		strEx::s::token rdata = strEx::s::getToken(ret.get<1>(), '|');
		nscapi::protobuf::functions::append_simple_query_response_payload(response_message.add_payload(), command, ret.get<0>(), rdata.first, rdata.second);
	}
	return NSCAPI::isSuccess;
}

int NRPEClient::clp_handler_impl::submit(client::configuration::data_type data, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage &response_message) {
	const ::Plugin::Common_Header& request_header = request_message.header();
	connection_data con = parse_header(request_header, data);
	std::wstring channel = utf8::cvt<std::wstring>(request_message.channel());
	
	nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_header);

	for (int i=0;i<request_message.payload_size();++i) {
		std::string command = get_command(request_message.payload(i).alias(), request_message.payload(i).command());
		std::string data = command;
		for (int a=0;a<request_message.payload(i).arguments_size();a++) {
			data += "!" + request_message.payload(i).arguments(i);
		}
		boost::tuple<int,std::string> ret = instance->send(con, data);
		nscapi::protobuf::functions::append_simple_submit_response_payload(response_message.add_payload(), command, ret.get<0>(), ret.get<1>());
	}
	return NSCAPI::isSuccess;
}

int NRPEClient::clp_handler_impl::exec(client::configuration::data_type data, const Plugin::ExecuteRequestMessage &request_message, Plugin::ExecuteResponseMessage &response_message) {
	const ::Plugin::Common_Header& request_header = request_message.header();
	connection_data con = parse_header(request_header, data);

	nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_header);

	for (int i=0;i<request_message.payload_size();i++) {
		std::string command = get_command(request_message.payload(i).command());
		std::string data = command;
		for (int a=0;a<request_message.payload(i).arguments_size();a++)
			data += "!" + request_message.payload(i).arguments(a);
		boost::tuple<int,std::string> ret = instance->send(con, data);
		nscapi::protobuf::functions::append_simple_exec_response_payload(response_message.add_payload(), command, ret.get<0>(), ret.get<1>());
	}
	return NSCAPI::isSuccess;
}

//////////////////////////////////////////////////////////////////////////
// Protocol implementations
//
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
};

boost::tuple<int,std::string> NRPEClient::send(connection_data con, const std::string data) {
	try {
		NSC_DEBUG_MSG_STD("Connection details: " + con.to_string());
		if (con.ssl.enabled) {
#ifndef USE_SSL
			NSC_LOG_ERROR_STD(_T("SSL not avalible (compiled without USE_SSL)"));
			return boost::make_tuple(NSCAPI::returnUNKNOWN, _T("SSL support not available (compiled without USE_SSL)"));
#endif
		}
		nrpe::packet packet = nrpe::packet::make_request(data, con.buffer_length);
		socket_helpers::client::client<nrpe::client::protocol> client(con, boost::shared_ptr<client_handler>(new client_handler()));
		client.connect();
		nrpe::packet response = client.process_request(packet);
		client.shutdown();
		return boost::make_tuple(static_cast<int>(response.getResult()), response.getPayload());
	} catch (nrpe::nrpe_exception &e) {
		return boost::make_tuple(NSCAPI::returnUNKNOWN, std::string("NRPE Packet errro: ") + e.what());
	} catch (std::runtime_error &e) {
		NSC_LOG_ERROR_EXR("failed to send", e);
		return boost::make_tuple(NSCAPI::returnUNKNOWN, "Socket error: " + utf8::utf8_from_native(e.what()));
	} catch (std::exception &e) {
		NSC_LOG_ERROR_EXR("failed to send", e);
		return boost::make_tuple(NSCAPI::returnUNKNOWN, "Error: " + utf8::utf8_from_native(e.what()));
	} catch (...) {
		NSC_LOG_ERROR_EX("failed to send");
		return boost::make_tuple(NSCAPI::returnUNKNOWN, "Unknown error -- REPORT THIS!");
	}
}
