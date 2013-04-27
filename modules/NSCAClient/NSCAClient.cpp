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
#include "NSCAClient.h"

#include <utils.h>
#include <strEx.h>

#include <cryptopp/cryptopp.hpp>
#include <nsca/nsca_packet.hpp>

#include <nsca/client/nsca_client_protocol.hpp>
#include <socket/client.hpp>

#include <settings/client/settings_client.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_core_helper.hpp>

namespace sh = nscapi::settings_helper;

const std::string command_prefix = "nsca";
const std::string default_command("submit");
/**
 * Default c-tor
 * @return 
 */
NSCAClient::NSCAClient() {}

/**
 * Default d-tor
 * @return 
 */
NSCAClient::~NSCAClient() {}

bool NSCAClient::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {

	try {

		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias("NSCA", alias, "client");
		target_path = settings.alias().get_settings_path("targets");

		settings.alias().add_path_to_settings()
			("NSCA CLIENT SECTION", "Section for NSCA passive check module.")

			("handlers", sh::fun_values_path(boost::bind(&NSCAClient::add_command, this, _1, _2)), 
			"CLIENT HANDLER SECTION", "")

			("targets", sh::fun_values_path(boost::bind(&NSCAClient::add_target, this, _1, _2)), 
			"REMOTE TARGET DEFINITIONS", "")
			;

		settings.alias().add_key_to_settings()
			("hostname", sh::string_key(&hostname_, "auto"),
			"HOSTNAME", "The host name of this host if set to blank (default) the windows name of the computer will be used.")

			("channel", sh::string_key(&channel_, "NSCA"),
			"CHANNEL", "The channel to listen to.")

			("delay", sh::string_fun_key<std::wstring>(boost::bind(&NSCAClient::set_delay, this, _1), _T("0")),
			"DELAY", "", true)
			;

		settings.register_all();
		settings.notify();

		targets.add_missing(get_settings_proxy(), target_path, "default", "", true);


		nscapi::core_helper::core_proxy core(get_core(), get_id());
		core.register_channel(channel_);

		if (hostname_ == "auto") {
			hostname_ = boost::asio::ip::host_name();
		} else if (hostname_ == "auto-lc") {
			hostname_ = boost::asio::ip::host_name();
			std::transform(hostname_.begin(), hostname_.end(), hostname_.begin(), ::tolower);
		} else {
			std::pair<std::string,std::string> dn = strEx::split<std::string>(boost::asio::ip::host_name(), ".");

			try {
				boost::asio::io_service svc;
				boost::asio::ip::tcp::resolver resolver (svc);
				boost::asio::ip::tcp::resolver::query query (boost::asio::ip::host_name(), "");
				boost::asio::ip::tcp::resolver::iterator iter = resolver.resolve (query), end;

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


			strEx::replace(hostname_, "${host}", dn.first);
			strEx::replace(hostname_, "${domain}", dn.second);
		}
	} catch (nscapi::nscapi_exception &e) {
		NSC_LOG_ERROR_EXR("Failed to send", e);
		return false;
	} catch (std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to send", e);
		return false;
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to send");
		return false;
	}
	return true;
}

std::string get_command(std::string alias, std::string command = "") {
	if (!alias.empty())
		return alias; 
	if (!command.empty())
		return command; 
	return "host_check";
}

//////////////////////////////////////////////////////////////////////////
// Settings helpers
//

void NSCAClient::add_target(std::string key, std::string arg) {
	try {
		targets.add(get_settings_proxy(), target_path , key, arg);
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to add target: " + key, e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to add target: " + key);
	}
}

void NSCAClient::add_command(std::string name, std::string args) {
	nscapi::core_helper::core_proxy core(get_core(), get_id());
	try {
		std::string key = commands.add_command(name, args);
		if (!key.empty())
			core.register_command(key.c_str(), "NSCA relay for: " + name);
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
bool NSCAClient::unloadModule() {
	return true;
}

void NSCAClient::query_fallback(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response, const Plugin::QueryRequestMessage &request_message) {
	client::configuration config(command_prefix);
	setup(config, request_message.header());
	commands.parse_query(command_prefix, default_command, request.command(), config, request, *response, request_message);
}

bool NSCAClient::commandLineExec(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response, const Plugin::ExecuteRequestMessage &request_message) {
	client::configuration config(command_prefix);
	setup(config, request_message.header());
	return commands.parse_exec(command_prefix, default_command, request.command(), config, request, *response, request_message);
}

void NSCAClient::handleNotification(const std::string &channel, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage *response_message) {
	client::configuration config(command_prefix);
	setup(config, request_message.header());
	commands.forward_submit(config, request_message, *response_message);
}

//////////////////////////////////////////////////////////////////////////
// Parser setup/Helpers
//

void NSCAClient::add_local_options(po::options_description &desc, client::configuration::data_type data) {
	desc.add_options()
		("encryption,e", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->recipient, "encryption", _1)), 
		"Length of payload (has to be same as on the server)")

		("certificate", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->recipient, "certificate", _1)), 
		"Length of payload (has to be same as on the server)")

		("dh", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->recipient, "dh", _1)), 
		"Length of payload (has to be same as on the server)")

		("certificate-key", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->recipient, "certificate key", _1)), 
		"Client certificate to use")

		("certificate-format", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->recipient, "certificate format", _1)), 
		"Client certificate format")

		("ca", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->recipient, "ca", _1)), 
		"Certificate authority")

		("verify", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->recipient, "verify mode", _1)), 
		"Client certificate format")

		("allowed-ciphers", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->recipient, "allowed ciphers", _1)), 
		"Client certificate format")

		("payload-length,l", po::value<unsigned int>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_int_data, &data->recipient, "payload length", _1)), 
		"Length of payload (has to be same as on the server)")

		("buffer-length", po::value<unsigned int>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_int_data, &data->recipient, "payload length", _1)), 
			"Length of payload (has to be same as on the server)")

 		("ssl,n", po::value<bool>()->zero_tokens()->default_value(false)->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_bool_data, &data->recipient, "ssl", _1)), 
			"Initial an ssl handshake with the server.")

		("timeout", po::value<unsigned int>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_int_data, &data->recipient, "timeout", _1)), 
		"")

		("password", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->recipient, "password", _1)), 
		"Password")

		("source-host", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->host_self, "host", _1)), 
		"Source/sender host name (default is auto which means use the name of the actual host)")

		("sender-host", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->host_self, "host", _1)), 
		"Source/sender host name (default is auto which means use the name of the actual host)")

		("time-offset", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->recipient, "time offset", _1)), 
		"")
		;
}

void NSCAClient::setup(client::configuration &config, const ::Plugin::Common_Header& header) {
	boost::shared_ptr<clp_handler_impl> handler(new clp_handler_impl(this));
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
	config.data->host_self.address.host = hostname_;

	config.target_lookup = handler;
	config.handler = handler;
}

NSCAClient::connection_data NSCAClient::parse_header(const ::Plugin::Common_Header &header, client::configuration::data_type data) {
	nscapi::protobuf::functions::destination_container recipient, sender;
	nscapi::protobuf::functions::parse_destination(header, header.recipient_id(), recipient, true);
	nscapi::protobuf::functions::parse_destination(header, header.sender_id(), sender, true);
	return connection_data(recipient, data->recipient, sender);
}

//////////////////////////////////////////////////////////////////////////
// Parser implementations
//

int NSCAClient::clp_handler_impl::query(client::configuration::data_type data, const Plugin::QueryRequestMessage &request_message, Plugin::QueryResponseMessage &response_message) {
	const ::Plugin::Common_Header& request_header = request_message.header();
	connection_data con = parse_header(request_header, data);

	nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_header);

	std::list<nsca::packet> list;
	for (int i=0;i < request_message.payload_size(); ++i) {
		nsca::packet packet(con.sender_hostname, con.buffer_length, con.time_delta);
		nscapi::protobuf::functions::decoded_simple_command_data data = nscapi::protobuf::functions::parse_simple_query_request(request_message.payload(i));
		packet.code = 0;
		packet.result = utf8::cvt<std::string>(data.command);
		list.push_back(packet);
	}

	boost::tuple<int,std::wstring> ret = instance->send(con, list);

	nscapi::protobuf::functions::append_simple_query_response_payload(response_message.add_payload(), "TODO", ret.get<0>(), utf8::cvt<std::string>(ret.get<1>()), "");
	return NSCAPI::isSuccess;
}

int NSCAClient::clp_handler_impl::submit(client::configuration::data_type data, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage &response_message) {
	const ::Plugin::Common_Header& request_header = request_message.header();
	connection_data con = parse_header(request_header, data);
	std::wstring channel = utf8::cvt<std::wstring>(request_message.channel());

	nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_header);

	std::list<nsca::packet> list;

	for (int i=0;i < request_message.payload_size(); ++i) {
		nsca::packet packet(con.sender_hostname, con.buffer_length, con.time_delta);
		std::wstring alias, msg, perf;
		packet.code = nscapi::protobuf::functions::parse_simple_submit_request_payload(request_message.payload(i), alias, msg, perf);
		if (alias != _T("host_check"))
			packet.service = utf8::cvt<std::string>(alias);
		if (perf.empty())
			packet.result = utf8::cvt<std::string>(msg);
		else
			packet.result = utf8::cvt<std::string>(msg + _T("|") + perf);
		list.push_back(packet);
	}

	boost::tuple<int,std::wstring> ret = instance->send(con, list);
	nscapi::protobuf::functions::append_simple_submit_response_payload(response_message.add_payload(), "TODO", ret.get<0>(), utf8::cvt<std::string>(ret.get<1>()));
	return NSCAPI::isSuccess;
}

int NSCAClient::clp_handler_impl::exec(client::configuration::data_type data, const Plugin::ExecuteRequestMessage &request_message, Plugin::ExecuteResponseMessage &response_message) {
	const ::Plugin::Common_Header& request_header = request_message.header();
	connection_data con = parse_header(request_header, data);

	nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_header);

	std::list<nsca::packet> list;
	for (int i=0;i < request_message.payload_size(); ++i) {
		nsca::packet packet(con.sender_hostname, con.buffer_length, con.time_delta);
		nscapi::protobuf::functions::decoded_simple_command_data data = nscapi::protobuf::functions::parse_simple_exec_request_payload(request_message.payload(i));
		packet.code = 0;
		if (data.command != "host_check")
			packet.service = data.command;
		//packet.result = data.;
		list.push_back(packet);
	}
	boost::tuple<int,std::wstring> ret = instance->send(con, list);
	nscapi::protobuf::functions::append_simple_exec_response_payload(response_message.add_payload(), "TODO", ret.get<0>(), utf8::cvt<std::string>(ret.get<1>()));
	return NSCAPI::isSuccess;
}

//////////////////////////////////////////////////////////////////////////
// Protocol implementations
//
struct client_handler : public socket_helpers::client::client_handler {
	unsigned int encryption_;
	std::string password_;
	client_handler(NSCAClient::connection_data &con) 
		: encryption_(con.get_encryption())
		, password_(con.password)
	{}
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
	unsigned int get_encryption() {
		return encryption_;
	}
	std::string get_password() {
		return password_;
	}
};

boost::tuple<int,std::wstring> NSCAClient::send(connection_data con, const std::list<nsca::packet> packets) {
	try {
		NSC_DEBUG_MSG_STD("Connection details: " + con.to_string());

		socket_helpers::client::client<nsca::client::protocol<client_handler> > client(con, boost::shared_ptr<client_handler>(new client_handler(con)));
		client.connect();

		BOOST_FOREACH(const nsca::packet &packet, packets) {
			NSC_DEBUG_MSG_STD("Sending (data): " + packet.to_string());
			client.process_request(packet);
		}
		client.shutdown();
		return boost::make_tuple(NSCAPI::isSuccess, _T(""));
	} catch (const nscp::encryption::encryption_exception &e) {
		NSC_LOG_ERROR_EXR("Failed to send", e);
		return boost::make_tuple(NSCAPI::hasFailed, _T("NSCA error: ") + utf8::to_unicode(e.what()));
	} catch (const std::runtime_error &e) {
		NSC_LOG_ERROR_EXR("Failed to send", e);
		return boost::make_tuple(NSCAPI::hasFailed, _T("Socket error: ") + utf8::to_unicode(e.what()));
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to send", e);
		return boost::make_tuple(NSCAPI::hasFailed, _T("Error: ") + utf8::to_unicode(e.what()));
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to send");
		return boost::make_tuple(NSCAPI::hasFailed, _T("Unknown error -- REPORT THIS!"));
	}
}
