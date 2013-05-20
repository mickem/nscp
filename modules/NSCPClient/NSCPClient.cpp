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
#include "NSCPClient.h"

#include <time.h>
#include <strEx.h>

#include <settings/client/settings_client.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>

#include <nscp/client/nscp_client_protocol.hpp>
#include <nscapi/nscapi_core_helper.hpp>

namespace sh = nscapi::settings_helper;

const std::string command_prefix("nscp");
const std::string default_command("query");
/**
 * Default c-tor
 * @return 
 */
NSCPClient::NSCPClient() {}

/**
 * Default d-tor
 * @return 
 */
NSCPClient::~NSCPClient() {}

bool NSCPClient::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {
	std::map<std::wstring,std::wstring> commands;

	try {

		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias("nscp", alias, "client");
		target_path = settings.alias().get_settings_path("targets");

		settings.alias().add_path_to_settings()
			("NSCP CLIENT SECTION", "Section for NSCP active/passive check module.")

			("handlers", sh::fun_values_path(boost::bind(&NSCPClient::add_command, this, _1, _2)), 
			"CLIENT HANDLER SECTION", "")

			("targets", sh::fun_values_path(boost::bind(&NSCPClient::add_target, this, _1, _2)), 
			"REMOTE TARGET DEFINITIONS", "")
			;

		settings.alias().add_key_to_settings()
			("channel", sh::string_key(&channel_, "NSCP"),
			"CHANNEL", "The channel to listen to.")

			;

		settings.register_all();
		settings.notify();

		targets.add_missing(get_settings_proxy(), target_path, "default", "", true);
		nscapi::core_helper::core_proxy core(get_core(), get_id());
		core.register_channel(channel_);
	} catch (nscapi::nscapi_exception &e) {
		NSC_LOG_ERROR_EXR("load", e);
		return false;
	} catch (std::exception &e) {
		NSC_LOG_ERROR_EXR("load", e);
		return false;
	} catch (...) {
		NSC_LOG_ERROR_EX("load");
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

void NSCPClient::add_target(std::string key, std::string arg) {
	try {
		targets.add(get_settings_proxy(), target_path , key, arg);
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to add target: " + key, e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to add target: " + key);
	}
}

void NSCPClient::add_command(std::string name, std::string args) {
	try {
		nscapi::core_helper::core_proxy core(get_core(), get_id());
		std::string key = commands.add_command(name, args);
		if (!key.empty())
			core.register_command(key, "NSCP relay for: " + name);
	} catch (const std::exception &e) {
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
bool NSCPClient::unloadModule() {
	return true;
}

void NSCPClient::query_fallback(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response, const Plugin::QueryRequestMessage &request_message) {
	client::configuration config(command_prefix);
	setup(config, request_message.header());
	commands.parse_query(command_prefix, default_command, request.command(), config, request, *response, request_message);
}

bool NSCPClient::commandLineExec(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response, const Plugin::ExecuteRequestMessage &request_message) {
	client::configuration config(command_prefix);
	setup(config, request_message.header());
	return commands.parse_exec(command_prefix, default_command, request.command(), config, request, *response, request_message);
}

void NSCPClient::handleNotification(const std::string &channel, const Plugin::QueryResponseMessage::Response &request, Plugin::SubmitResponseMessage::Response *response, const Plugin::SubmitRequestMessage &request_message) {
	client::configuration config(command_prefix);
	setup(config, request_message.header());
	commands.parse_submit(command_prefix, default_command, request.command(), config, request, *response, request_message);
}

//////////////////////////////////////////////////////////////////////////
// Parser setup/Helpers
//

void NSCPClient::add_local_options(po::options_description &desc, client::configuration::data_type data) {
	desc.add_options()
 		("no-ssl,n", po::value<bool>()->zero_tokens()->default_value(false)->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_bool_data, &data->recipient, "no ssl", _1)), 
			"Do not initial an ssl handshake with the server, talk in plaintext.")

		("certificate,c", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->recipient, "certificate", _1)), 
		"Length of payload (has to be same as on the server)")

		("dh", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->recipient, "dh", _1)), 
		"Length of payload (has to be same as on the server)")

		("certificate-key,k", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->recipient, "certificate key", _1)), 
		"Client certificate to use")

		("certificate-format", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->recipient, "certificate format", _1)), 
		"Client certificate format")

		("ca", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->recipient, "ca", _1)), 
		"Certificate authority")

		("verify", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->recipient, "verify mode", _1)), 
		"Client certificate format")

		("allowed-ciphers", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->recipient, "allowed ciphers", _1)), 
		"Client certificate format")

 		("ssl,n", po::value<bool>()->zero_tokens()->default_value(false)->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_bool_data, &data->recipient, "ssl", _1)), 
			"Initial an ssl handshake with the server.")

		("timeout", po::value<unsigned int>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_int_data, &data->recipient, "timeout", _1)), 
		"")


		;
}

void NSCPClient::setup(client::configuration &config, const ::Plugin::Common_Header& header) {
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

NSCPClient::connection_data NSCPClient::parse_header(const ::Plugin::Common_Header &header, client::configuration::data_type data) {
	nscapi::protobuf::functions::destination_container recipient;
	nscapi::protobuf::functions::parse_destination(header, header.recipient_id(), recipient, true);
	return connection_data(recipient, data->recipient);
}

//////////////////////////////////////////////////////////////////////////
// Parser implementations
//

std::string gather_and_log_errors(std::string  &payload) {
	NSCPIPC::ErrorMessage message;
	message.ParseFromString(payload);
	std::string ret;
	for (int i=0;i<message.error_size();i++) {
		ret += message.error(i).message();
		NSC_LOG_ERROR_STD("Error: " + message.error(i).message());
	}
	return ret;
}

int NSCPClient::clp_handler_impl::send(connection_data &con, ::NSCPIPC::Common_MessageTypes type, const std::string &request_message, std::string &response_message) {
	NSCPIPC::PayloadMessage request_payload;
	nscp::packet request_packet;
	request_payload.set_type(type);
	request_payload.set_message(request_message);
	request_packet.add_payload(request_payload.SerializeAsString());

	nscp::packet response_packet = instance->send(con, request_packet);
	NSCPIPC::PayloadMessage response_payload;
	std::string tmp;
	if (response_packet.get_error(tmp)) {
		NSCPIPC::ErrorMessage error;
		error.ParseFromString(tmp);
		for (int i=0;i<error.error_size();i++)
			NSC_LOG_ERROR("Invalid response: " + error.error(i).message());
		return NSCAPI::hasFailed;
	} else if (response_packet.get_payload(tmp)) {
		response_payload.ParseFromString(tmp);
		response_message = response_payload.message();
	} else {
		NSC_LOG_ERROR("Invalid response");
		return NSCAPI::hasFailed;
	}
	return NSCAPI::isSuccess;
}

int NSCPClient::clp_handler_impl::query(client::configuration::data_type data, const Plugin::QueryRequestMessage &request_message, Plugin::QueryResponseMessage &response_message) {
	const ::Plugin::Common_Header& request_header = request_message.header();
	connection_data con = parse_header(request_header, data);

	std::string tmp;
	int ret = send(con, NSCPIPC::Common_MessageTypes_QUERY_REQUEST, request_message.SerializeAsString(), tmp);
	response_message.ParseFromString(tmp);
	return ret;
}

int NSCPClient::clp_handler_impl::submit(client::configuration::data_type data, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage &response_message) {
	const ::Plugin::Common_Header& request_header = request_message.header();
	connection_data con = parse_header(request_header, data);
	
	std::string tmp;
	int ret = send(con, NSCPIPC::Common_MessageTypes_SUBMIT_REQUEST, request_message.SerializeAsString(), tmp);
	response_message.ParseFromString(tmp);
	return ret;
}

int NSCPClient::clp_handler_impl::exec(client::configuration::data_type data, const Plugin::ExecuteRequestMessage &request_message, Plugin::ExecuteResponseMessage &response_message) {
	const ::Plugin::Common_Header& request_header = request_message.header();
	connection_data con = parse_header(request_header, data);

	std::string tmp;
	int ret = send(con, NSCPIPC::Common_MessageTypes_EXEC_REQUEST, request_message.SerializeAsString(), tmp);
	response_message.ParseFromString(tmp);
	return ret;
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

nscp::packet NSCPClient::send(connection_data con, nscp::packet &packet) {
	nscp::packet response;
	try {
		NSC_DEBUG_MSG_STD("Connection details: " + con.to_string());
		if (con.ssl.enabled) {
#ifndef USE_SSL
			NSC_LOG_ERROR_STD(_T("SSL not avalible (compiled without USE_SSL)"));
			return response;
#endif
		}
		socket_helpers::client::client<nscp::client::protocol> client(con, boost::shared_ptr<client_handler>(new client_handler()));
		client.connect();
		response = client.process_request(packet);
		client.shutdown();
		return response;
	} catch (std::runtime_error &e) {
		NSC_LOG_ERROR_EXR("Failed to send", e);
		return response;
	} catch (std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to send", e);
		return response;
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to send");
		return response;
	}
}
