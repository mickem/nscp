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

#include <protobuf/plugin.pb.h>

#include <settings/client/settings_client.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>

#include <nscp/client/nscp_client_protocol.hpp>

namespace sh = nscapi::settings_helper;

const std::wstring NSCPClient::command_prefix = _T("nscp");
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

/**
 * Load (initiate) module.
 * Start the background collector thread and let it run until unloadModule() is called.
 * @return true
 */
bool NSCPClient::loadModule() {
	return false;
}

bool NSCPClient::loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode) {
	std::map<std::wstring,std::wstring> commands;

	try {


		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias(_T("nscp"), alias, _T("client"));
		target_path = settings.alias().get_settings_path(_T("targets"));

		settings.alias().add_path_to_settings()
			(_T("NSCP CLIENT SECTION"), _T("Section for NSCP active/passive check module."))

			(_T("handlers"), sh::fun_values_path(boost::bind(&NSCPClient::add_command, this, _1, _2)), 
			_T("CLIENT HANDLER SECTION"), _T(""))

			(_T("targets"), sh::fun_values_path(boost::bind(&NSCPClient::add_target, this, _1, _2)), 
			_T("REMOTE TARGET DEFINITIONS"), _T(""))
			;

		settings.alias().add_key_to_settings()
			(_T("channel"), sh::wstring_key(&channel_, _T("NSCP")),
			_T("CHANNEL"), _T("The channel to listen to."))

			;

		settings.register_all();
		settings.notify();

		targets.add_missing(get_settings_proxy(), target_path, _T("default"), _T(""), true);


		get_core()->registerSubmissionListener(get_id(), channel_);
		register_command(_T("nscp_query"), _T("Submit a query to a remote host via NSCP"));
		register_command(_T("nscp_forward"), _T("Forward query to remote NSCP host"));
		register_command(_T("nscp_submit"), _T("Submit a query to a remote host via NSCP"));
		register_command(_T("nscp_exec"), _T("Execute remote command on a remote host via NSCP"));
		register_command(_T("nscp_help"), _T("Help on using NSCP Client"));
	} catch (nscapi::nscapi_exception &e) {
		NSC_LOG_ERROR_STD(_T("NSClient API exception: ") + utf8::to_unicode(e.what()));
		return false;
	} catch (std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Exception caught: ") + utf8::to_unicode(e.what()));
		return false;
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Exception caught: <UNKNOWN EXCEPTION>"));
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

void NSCPClient::add_target(std::wstring key, std::wstring arg) {
	try {
		targets.add(get_settings_proxy(), target_path , key, arg);
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Failed to add target: ") + key + _T(", ") + utf8::to_unicode(e.what()));
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to add target: ") + key);
	}
}

void NSCPClient::add_command(std::wstring name, std::wstring args) {
	try {
		std::wstring key = commands.add_command(name, args);
		if (!key.empty())
			register_command(key.c_str(), _T("NSCP relay for: ") + name);
	} catch (boost::program_options::validation_error &e) {
		NSC_LOG_ERROR_STD(_T("Could not add command ") + name + _T(": ") + utf8::to_unicode(e.what()));
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Could not add command ") + name);
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

NSCAPI::nagiosReturn NSCPClient::handleRAWCommand(const wchar_t* char_command, const std::string &request, std::string &result) {
	std::wstring cmd = client::command_line_parser::parse_command(char_command, command_prefix);

	Plugin::QueryRequestMessage message;
	message.ParseFromString(request);

	client::configuration config;
	setup(config, message.header());

	return commands.process_query(cmd, config, message, result);
}

NSCAPI::nagiosReturn NSCPClient::commandRAWLineExec(const wchar_t* char_command, const std::string &request, std::string &result) {
	std::wstring cmd = client::command_line_parser::parse_command(char_command, command_prefix);

	Plugin::ExecuteRequestMessage message;
	message.ParseFromString(request);

	client::configuration config;
	setup(config, message.header());

	return commands.process_exec(cmd, config, message, result);
}

NSCAPI::nagiosReturn NSCPClient::handleRAWNotification(const wchar_t* channel, std::string request, std::string &result) {
	Plugin::SubmitRequestMessage message;
	message.ParseFromString(request);

	client::configuration config;
	setup(config, message.header());

	return client::command_line_parser::do_relay_submit(config, message, result);
}

//////////////////////////////////////////////////////////////////////////
// Parser setup/Helpers
//

void NSCPClient::add_local_options(po::options_description &desc, client::configuration::data_type data) {
	desc.add_options()
 		("no-ssl,n", po::value<bool>()->zero_tokens()->default_value(false)->notifier(boost::bind(&nscapi::functions::destination_container::set_bool_data, &data->recipient, "no ssl", _1)), 
			"Do not initial an ssl handshake with the server, talk in plaintext.")

		("certificate,c", po::value<std::string>()->notifier(boost::bind(&nscapi::functions::destination_container::set_string_data, &data->recipient, "certificate", _1)), 
		"Length of payload (has to be same as on the server)")

		("pfs", po::value<std::string>()->notifier(boost::bind(&nscapi::functions::destination_container::set_string_data, &data->recipient, "pfs", _1)), 
		"Length of payload (has to be same as on the server)")

		("certificate-key,k", po::value<std::string>()->notifier(boost::bind(&nscapi::functions::destination_container::set_string_data, &data->recipient, "certificate key", _1)), 
		"Client certificate to use")

		("certificate-format", po::value<std::string>()->notifier(boost::bind(&nscapi::functions::destination_container::set_string_data, &data->recipient, "certificate format", _1)), 
		"Client certificate format")

		("ca", po::value<std::string>()->notifier(boost::bind(&nscapi::functions::destination_container::set_string_data, &data->recipient, "ca", _1)), 
		"Certificate authority")

		("verify", po::value<std::string>()->notifier(boost::bind(&nscapi::functions::destination_container::set_string_data, &data->recipient, "verify mode", _1)), 
		"Client certificate format")

		("allowed-ciphers", po::value<std::string>()->notifier(boost::bind(&nscapi::functions::destination_container::set_string_data, &data->recipient, "allowed ciphers", _1)), 
		"Client certificate format")

 		("ssl,n", po::value<bool>()->zero_tokens()->default_value(false)->notifier(boost::bind(&nscapi::functions::destination_container::set_bool_data, &data->recipient, "ssl", _1)), 
			"Initial an ssl handshake with the server.")

		("timeout", po::value<unsigned int>()->notifier(boost::bind(&nscapi::functions::destination_container::set_int_data, &data->recipient, "timeout", _1)), 
		"")


		;
}

void NSCPClient::setup(client::configuration &config, const ::Plugin::Common_Header& header) {
	boost::shared_ptr<clp_handler_impl> handler = boost::shared_ptr<clp_handler_impl>(new clp_handler_impl(this));
	add_local_options(config.local, config.data);

	config.data->recipient.id = header.recipient_id();
	std::wstring recipient = utf8::cvt<std::wstring>(config.data->recipient.id);
	if (!targets.has_object(recipient)) {
		recipient = _T("default");
	}
	nscapi::targets::optional_target_object opt = targets.find_object(recipient);

	if (opt) {
		nscapi::targets::target_object t = *opt;
		nscapi::functions::destination_container def = t.to_destination_container();
		config.data->recipient.apply(def);
	}
	config.data->host_self.id = "self";
	//config.data->host_self.host = hostname_;

	config.target_lookup = handler;
	config.handler = handler;
}

NSCPClient::connection_data NSCPClient::parse_header(const ::Plugin::Common_Header &header, client::configuration::data_type data) {
	nscapi::functions::destination_container recipient;
	nscapi::functions::parse_destination(header, header.recipient_id(), recipient, true);
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
		NSC_LOG_ERROR_STD(_T("Error: ") + utf8::cvt<std::wstring>(message.error(i).message()));
	}
	return ret;
}
int NSCPClient::clp_handler_impl::query(client::configuration::data_type data, const Plugin::QueryRequestMessage &request_message, std::string &reply) {
	const ::Plugin::Common_Header& request_header = request_message.header();
	int ret = NSCAPI::returnUNKNOWN;
	connection_data con = parse_header(request_header, data);

	Plugin::QueryResponseMessage response_message;
	nscapi::functions::make_return_header(response_message.mutable_header(), request_header);

	std::list<nscp::packet> chunks;
	chunks.push_back(nscp::factory::create_payload(nscp::data::command_request, request_message.SerializeAsString()));
	chunks = instance->send(con, chunks);
	BOOST_FOREACH(nscp::packet &chunk, chunks) {
		if (nscp::checks::is_query_response(chunk)) {
			nscapi::functions::append_response_payloads(response_message, chunk.payload);
		} else if (nscp::checks::is_error(chunk)) {
			std::string error = gather_and_log_errors(chunk.payload);
			nscapi::functions::append_simple_query_response_payload(response_message.add_payload(), "", NSCAPI::returnUNKNOWN, error);
			ret = NSCAPI::returnUNKNOWN;
		} else {
			NSC_LOG_ERROR_STD(_T("Unsupported message type: ") + strEx::itos(chunk.signature.payload_type));
			nscapi::functions::append_simple_query_response_payload(response_message.add_payload(), "", NSCAPI::returnUNKNOWN, "Unsupported response type");
			ret = NSCAPI::returnUNKNOWN;
		}
	}
	response_message.SerializeToString(&reply);
	return ret;
}

int NSCPClient::clp_handler_impl::submit(client::configuration::data_type data, const Plugin::SubmitRequestMessage &request_message, std::string &reply) {
	const ::Plugin::Common_Header& request_header = request_message.header();
	int ret = NSCAPI::returnUNKNOWN;
	connection_data con = parse_header(request_header, data);
	Plugin::SubmitResponseMessage response_message;
	nscapi::functions::make_return_header(response_message.mutable_header(), request_header);

	std::list<nscp::packet> chunks;
	chunks.push_back(nscp::factory::create_payload(nscp::data::command_response, request_message.SerializeAsString()));
	chunks = instance->send(con, chunks);
	BOOST_FOREACH(nscp::packet &chunk, chunks) {
		if (nscp::checks::is_submit_response(chunk)) {
			nscapi::functions::append_response_payloads(response_message, chunk.payload);
		} else if (nscp::checks::is_error(chunk)) {
			std::string error = gather_and_log_errors(chunk.payload);
			nscapi::functions::append_simple_submit_response_payload(response_message.add_payload(), "", NSCAPI::returnUNKNOWN, error);
			ret = NSCAPI::returnUNKNOWN;
		} else {
			NSC_LOG_ERROR_STD(_T("Unsupported message type: ") + strEx::itos(chunk.signature.payload_type));
			nscapi::functions::append_simple_submit_response_payload(response_message.add_payload(), "", NSCAPI::returnUNKNOWN, "Unsupported response type");
			ret = NSCAPI::returnUNKNOWN;
		}
	}
	response_message.SerializeToString(&reply);
	return ret;
}

int NSCPClient::clp_handler_impl::exec(client::configuration::data_type data, const Plugin::ExecuteRequestMessage &request_message, std::string &reply) {
	const ::Plugin::Common_Header& request_header = request_message.header();
	int ret = NSCAPI::returnOK;
	connection_data con = parse_header(request_header, data);

	Plugin::ExecuteResponseMessage response_message;
	nscapi::functions::make_return_header(response_message.mutable_header(), request_header);

	std::list<nscp::packet> chunks;
	chunks.push_back(nscp::factory::create_payload(nscp::data::exec_request, request_message.SerializeAsString()));
	chunks = instance->send(con, chunks);
	BOOST_FOREACH(nscp::packet &chunk, chunks) {
		if (nscp::checks::is_exec_response(chunk)) {
			nscapi::functions::append_response_payloads(response_message, chunk.payload);
		} else if (nscp::checks::is_error(chunk)) {
			std::string error = gather_and_log_errors(chunk.payload);
			nscapi::functions::append_simple_exec_response_payload(response_message.add_payload(), "", NSCAPI::returnUNKNOWN, error);
			ret = NSCAPI::returnUNKNOWN;
		} else {
			NSC_LOG_ERROR_STD(_T("Unsupported message type: ") + strEx::itos(chunk.signature.payload_type));
			nscapi::functions::append_simple_exec_response_payload(response_message.add_payload(), "", NSCAPI::returnUNKNOWN, "Unsupported response type");
			ret = NSCAPI::returnUNKNOWN;
		}
	}
	response_message.SerializeToString(&reply);
	return ret;
}

//////////////////////////////////////////////////////////////////////////
// Protocol implementations
//
struct client_handler : public socket_helpers::client::client_handler {
	void log_debug(std::string file, int line, std::string msg) const {
		if (GET_CORE()->should_log(NSCAPI::log_level::debug)) {
			GET_CORE()->log(NSCAPI::log_level::debug, file, line, utf8::to_unicode(msg));
		}
	}
	void log_error(std::string file, int line, std::string msg) const {
		if (GET_CORE()->should_log(NSCAPI::log_level::error)) {
			GET_CORE()->log(NSCAPI::log_level::error, file, line, utf8::to_unicode(msg));
		}
	}
};

std::list<nscp::packet> NSCPClient::send(connection_data con, std::list<nscp::packet> &chunks) {
	std::list<nscp::packet> response;
	try {
		NSC_DEBUG_MSG_STD(_T("Connection details: ") + con.to_wstring());
		if (con.ssl.enabled) {
#ifndef USE_SSL
			NSC_LOG_ERROR_STD(_T("SSL not avalible (compiled without USE_SSL)"));
			return response;
#endif
		}
		socket_helpers::client::client<nscp::client::protocol> client(con, boost::shared_ptr<client_handler>(new client_handler()));
		client.connect();
		BOOST_FOREACH(nscp::packet packet, chunks) {
			response.push_back(client.process_request(packet));
		}
		client.shutdown();
		return response;
	} catch (std::runtime_error &e) {
		NSC_LOG_ERROR_STD(_T("Socket error: ") + utf8::to_unicode(e.what()));
		return response;
	} catch (std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Error: ") + utf8::to_unicode(e.what()));
		return response;
	} catch (...) {
		return response;
	}
}

NSC_WRAP_DLL()
NSC_WRAPPERS_MAIN_DEF(NSCPClient)
NSC_WRAPPERS_IGNORE_MSG_DEF()
NSC_WRAPPERS_HANDLE_CMD_DEF()
NSC_WRAPPERS_CLI_DEF()
NSC_WRAPPERS_HANDLE_NOTIFICATION_DEF()

