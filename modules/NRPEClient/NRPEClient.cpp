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

#include <strEx.h>
#include <nrpe/client/socket.hpp>

#include <settings/client/settings_client.hpp>

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

/**
 * Load (initiate) module.
 * Start the background collector thread and let it run until unloadModule() is called.
 * @return true
 */
bool NRPEClient::loadModule() {
	return false;
}

bool NRPEClient::loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode) {
	std::map<std::wstring,std::wstring> commands;

	std::wstring certificate;
	unsigned int timeout = 30, buffer_length = 1024;
	bool use_ssl = true;
	try {

		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias(_T("NRPE"), alias, _T("client"));

		target_path = settings.alias().get_settings_path(_T("targets"));

		settings.alias().add_path_to_settings()
			(_T("NRPE CLIENT SECTION"), _T("Section for NRPE active/passive check module."))

			(_T("handlers"), sh::fun_values_path(boost::bind(&NRPEClient::add_command, this, _1, _2)), 
			_T("CLIENT HANDLER SECTION"), _T(""))

			(_T("targets"), sh::fun_values_path(boost::bind(&NRPEClient::add_target, this, _1, _2)), 
			_T("REMOTE TARGET DEFINITIONS"), _T(""))
			;

		settings.alias().add_key_to_settings()
			(_T("channel"), sh::wstring_key(&channel_, _T("NRPE")),
			_T("CHANNEL"), _T("The channel to listen to."))

			;

		settings.alias(_T("/targets/default")).add_key_to_settings()

			(_T("timeout"), sh::uint_key(&timeout, 30),
			_T("TIMEOUT"), _T("Timeout when reading/writing packets to/from sockets."))

			(_T("use ssl"), sh::bool_key(&use_ssl, true),
			_T("ENABLE SSL ENCRYPTION"), _T("This option controls if SSL should be enabled."))

			(_T("certificate"), sh::wpath_key(&certificate, _T("${certificate-path}/nrpe_dh_512.pem")),
			_T("SSL CERTIFICATE"), _T(""))

			(_T("payload length"),  sh::uint_key(&buffer_length, 1024),
			_T("PAYLOAD LENGTH"), _T("Length of payload to/from the NRPE agent. This is a hard specific value so you have to \"configure\" (read recompile) your NRPE agent to use the same value for it to work."))
			;

		settings.register_all();
		settings.notify();

		get_core()->registerSubmissionListener(get_id(), channel_);

		if (!targets.has_target(_T("default"))) {
			add_target(_T("default"), _T("default"));
			targets.rebuild();
		}
		nscapi::target_handler::optarget t = targets.find_target(_T("default"));
		if (t) {
			if (!t->has_option("certificate"))
				t->options[_T("certificate")] = certificate;
			if (!t->has_option("timeout"))
				t->options[_T("timeout")] = strEx::itos(timeout);
			if (!t->has_option("payload length"))
				t->options[_T("payload length")] = strEx::itos(buffer_length);
			if (!t->has_option("ssl"))
				t->options[_T("ssl")] = use_ssl?_T("true"):_T("false");
			targets.add(*t);
		} else {
			NSC_LOG_ERROR(_T("Default target not found!"));
		}

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

void NRPEClient::add_target(std::wstring key, std::wstring arg) {
	try {
		nscapi::target_handler::target t = targets.add(get_settings_proxy(), target_path , key, arg);
		if (t.has_option(_T("certificate"))) {
			boost::filesystem::wpath p = t.options[_T("certificate")];
			if (!boost::filesystem::is_regular(p)) {
				p = get_core()->getBasePath() / p;
				t.options[_T("certificate")] = utf8::cvt<std::wstring>(p.string());
				targets.add(t);
			}
			if (boost::filesystem::is_regular(p)) {
				NSC_DEBUG_MSG_STD(_T("Using certificate: ") + p.string());
			} else {
				NSC_LOG_ERROR_STD(_T("Certificate not found: ") + p.string());
			}
		}
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to add target: ") + key);
	}
}

void NRPEClient::add_command(std::wstring name, std::wstring args) {
	try {
		std::wstring key = commands.add_command(name, args);
		if (!key.empty())
			register_command(key.c_str(), _T("NRPE relay for: ") + name);
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
bool NRPEClient::unloadModule() {
	return true;
}

NSCAPI::nagiosReturn NRPEClient::handleCommand(const std::wstring &target, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &message, std::wstring &perf) {
	std::wstring cmd = client::command_line_parser::parse_command(command, _T("nrpe"));

	client::configuration config;
	setup(config);
	if (cmd == _T("query"))
		return client::command_line_parser::query(config, cmd, arguments, message, perf);
	if (cmd == _T("submit")) {
		boost::tuple<int,std::wstring> result = client::command_line_parser::simple_submit(config, cmd, arguments);
		message = result.get<1>();
		return result.get<0>();
	}
	if (cmd == _T("exec")) {
		return client::command_line_parser::exec(config, cmd, arguments, message);
	}
	return commands.exec_simple(config, target, command, arguments, message, perf);
}

int NRPEClient::commandLineExec(const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &result) {
	std::wstring cmd = client::command_line_parser::parse_command(command, _T("nrpe"));
	if (!client::command_line_parser::is_command(cmd))
		return NSCAPI::returnIgnored;

	client::configuration config;
	setup(config);
	return client::command_line_parser::commandLineExec(config, cmd, arguments, result);
}

NSCAPI::nagiosReturn NRPEClient::handleRAWNotification(const wchar_t* channel, std::string request, std::string &response) {
	try {
		client::configuration config;
		setup(config);

		if (!client::command_line_parser::relay_submit(config, request, response)) {
			NSC_LOG_ERROR_STD(_T("Failed to submit message..."));
			return NSCAPI::hasFailed;
		}
		return NSCAPI::isSuccess;
	} catch (std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Failed to send data: ") + utf8::to_unicode(e.what()));
		return NSCAPI::hasFailed;
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to send data: UNKNOWN"));
		return NSCAPI::hasFailed;
	}
}

//////////////////////////////////////////////////////////////////////////
// Parser setup/Helpers
//

void NRPEClient::add_local_options(po::options_description &desc, client::configuration::data_type data) {
 	desc.add_options()
		("certificate,c", po::value<std::string>()->notifier(boost::bind(&nscapi::functions::destination_container::set_string_data, &data->recipient, "certificate", _1)), 
			"Length of payload (has to be same as on the server)")

		("buffer-length,l", po::value<unsigned int>()->notifier(boost::bind(&nscapi::functions::destination_container::set_int_data, &data->recipient, "payload length", _1)), 
			"Length of payload (has to be same as on the server)")

 		("no-ssl,n", po::value<bool>()->zero_tokens()->default_value(false)->notifier(boost::bind(&nscapi::functions::destination_container::set_bool_data, &data->recipient, "no ssl", _1)), 
			"Do not initial an ssl handshake with the server, talk in plaintext.")
 		;
}

void NRPEClient::setup(client::configuration &config) {
	boost::shared_ptr<clp_handler_impl> handler = boost::shared_ptr<clp_handler_impl>(new clp_handler_impl(this));
	add_local_options(config.local, config.data);

	net::wurl url;
	url.protocol = _T("nrpe");
	url.port = 5666;
	nscapi::target_handler::optarget opt = targets.find_target(_T("default"));
	if (opt) {
		nscapi::target_handler::target t = *opt;
		url.host = t.host;
		if (t.has_option("port")) {
			try {
				url.port = strEx::stoi(t.options[_T("port")]);
			} catch (...) {}
		}
		std::string keys[] = {"certificate", "timeout", "payload length", "ssl"};
		BOOST_FOREACH(std::string s, keys) {
			config.data->recipient.data[s] = utf8::cvt<std::string>(t.options[utf8::cvt<std::wstring>(s)]);
		}
	}
	config.data->recipient.id = "default";
	config.data->recipient.address = utf8::cvt<std::string>(url.to_string());
	config.data->host_self.id = "self";
	//config.data->host_self.host = hostname_;

	config.target_lookup = handler;
	config.handler = handler;
}

NRPEClient::connection_data NRPEClient::parse_header(const ::Plugin::Common_Header &header) {
	nscapi::functions::destination_container recipient;
	nscapi::functions::parse_destination(header, header.recipient_id(), recipient, true);
	return connection_data(recipient);
}

//////////////////////////////////////////////////////////////////////////
// Parser implementations
//

int NRPEClient::clp_handler_impl::query(client::configuration::data_type data, ::Plugin::Common_Header* header, const std::string &request, std::string &reply) {
	NSCAPI::nagiosReturn ret = NSCAPI::returnOK;
	try {
		Plugin::QueryRequestMessage request_message;
		request_message.ParseFromString(request);
		connection_data con = parse_header(*header);

		Plugin::QueryResponseMessage response_message;
		nscapi::functions::create_simple_header(response_message.mutable_header());	// TODO copy request header (inverted)

		for (int i=0;i<request_message.payload_size();i++) {
			std::string command = get_command(request_message.payload(i).alias(), request_message.payload(i).command());
			std::string data = command;
			for (int a=0;a<request_message.payload(i).arguments_size();a++) {
				data += "!" + request_message.payload(i).arguments(a);
			}
			boost::tuple<int,std::wstring> ret = instance->send(con, data);
			std::pair<std::wstring,std::wstring> rdata = strEx::split(ret.get<1>(), _T("|"));
			nscapi::functions::append_simple_query_response_payload(response_message.add_payload(), utf8::cvt<std::wstring>(command), ret.get<0>(), rdata.first, rdata.second);
		}
		response_message.SerializeToString(&reply);
		return NSCAPI::isSuccess;
	} catch (std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Exception: ") + utf8::to_unicode(e.what()));
		nscapi::functions::create_simple_query_response(_T("command"), NSCAPI::returnUNKNOWN, _T("Exception: ") + utf8::to_unicode(e.what()), _T(""), reply);
		return NSCAPI::returnUNKNOWN;
	}
}

int NRPEClient::clp_handler_impl::submit(client::configuration::data_type data, ::Plugin::Common_Header* header, const std::string &request, std::string &reply) {
	std::wstring channel;
	try {
		Plugin::SubmitRequestMessage message;
		message.ParseFromString(request);
		connection_data con = parse_header(*header);
		channel = utf8::cvt<std::wstring>(message.channel());
		
		for (int i=0;i<message.payload_size();++i) {
			std::string command = get_command(message.payload(i).alias(), message.payload(i).command());
			std::string data = command;
			for (int a=0;a<message.payload(i).arguments_size();a++) {
				data += "!" + message.payload(i).arguments(i);
			}
			boost::tuple<int,std::wstring> ret = instance->send(con, data);
			// TODO: Change this to append!
			nscapi::functions::create_simple_submit_response(channel, utf8::cvt<std::wstring>(command), ret.get<0>(), _T("Message submitted successfully: ") + ret.get<1>(), reply);
			return NSCAPI::isSuccess;
		}
		nscapi::functions::create_simple_submit_response(channel, _T("UNKNOWN"), NSCAPI::returnUNKNOWN, _T("Empty message was submitted"), reply);
		return NSCAPI::isSuccess;
	} catch (std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Exception: ") + utf8::to_unicode(e.what()));
		nscapi::functions::create_simple_submit_response(channel, _T("UNKNOWN"), NSCAPI::returnUNKNOWN, utf8::to_unicode(e.what()), reply);
		return NSCAPI::hasFailed;
	}	
}

int NRPEClient::clp_handler_impl::exec(client::configuration::data_type data, ::Plugin::Common_Header* header, const std::string &request, std::string &reply) {
	NSCAPI::nagiosReturn ret = NSCAPI::returnOK;
	try {
		Plugin::ExecuteRequestMessage request_message;
		request_message.ParseFromString(request);
		connection_data con = parse_header(*header);

		for (int i=0;i<request_message.payload_size();i++) {
			std::string command = get_command(request_message.payload(i).command());
			std::string data = command;
			for (int a=0;a<request_message.payload(i).arguments_size();a++) {
				data += "!" + request_message.payload(i).arguments(a);
			}
			boost::tuple<int,std::wstring> ret = instance->send(con, data);
			nscapi::functions::create_simple_exec_response(utf8::cvt<std::wstring>(command), ret.get<0>(), ret.get<1>(), reply);
		}
		return NSCAPI::isSuccess;
	} catch (std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Exception: ") + utf8::to_unicode(e.what()));
		nscapi::functions::create_simple_exec_response(_T("command"), NSCAPI::returnUNKNOWN, _T("Exception: ") + utf8::to_unicode(e.what()), reply);
		return NSCAPI::hasFailed;
	}
}

//////////////////////////////////////////////////////////////////////////
// Protocol implementations
//

boost::tuple<int,std::wstring> NRPEClient::send(connection_data con, const std::string data) {
	try {
		NSC_DEBUG_MSG_STD(_T("NRPE Connection details: ") + con.to_wstring());
		NSC_DEBUG_MSG_STD(_T("NRPE data: ") + utf8::cvt<std::wstring>(data));
		nrpe::packet packet;
		if (con.use_ssl) {
#ifdef USE_SSL
			packet = send_ssl(con.cert, con.host, con.port, con.timeout, nrpe::packet::make_request(utf8::cvt<std::wstring>(data), con.buffer_length));
#else
			NSC_LOG_ERROR_STD(_T("SSL not avalible (compiled without USE_SSL)"));
			return boost::tie(NSCAPI::returnUNKNOWN, _T("SSL support not available (compiled without USE_SSL)"));
#endif
		} else
			packet = send_nossl(con.host, con.port, con.timeout, nrpe::packet::make_request(utf8::cvt<std::wstring>(data), con.buffer_length));
		return boost::make_tuple(static_cast<int>(packet.getResult()), packet.getPayload());
	} catch (nrpe::nrpe_packet_exception &e) {
		return boost::tie(NSCAPI::returnUNKNOWN, _T("NRPE Packet errro: ") + e.wwhat());
	} catch (std::runtime_error &e) {
		NSC_LOG_ERROR_STD(_T("Socket error: ") + utf8::to_unicode(e.what()));
		return boost::tie(NSCAPI::returnUNKNOWN, _T("Socket error: ") + utf8::to_unicode(e.what()));
	} catch (std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Error: ") + utf8::to_unicode(e.what()));
		return boost::tie(NSCAPI::returnUNKNOWN, _T("Error: ") + utf8::to_unicode(e.what()));
	} catch (...) {
		return boost::tie(NSCAPI::returnUNKNOWN, _T("Unknown error -- REPORT THIS!"));
	}
}


#ifdef USE_SSL
nrpe::packet NRPEClient::send_ssl(std::string cert, std::string host, std::string port, int timeout, nrpe::packet packet) {
	boost::asio::io_service io_service;
	boost::asio::ssl::context ctx(io_service, boost::asio::ssl::context::sslv23);
	SSL_CTX_set_cipher_list(ctx.impl(), "ADH");
	ctx.use_tmp_dh_file(to_string(cert));
	ctx.set_verify_mode(boost::asio::ssl::context::verify_none);
	nrpe::client::ssl_socket socket(io_service, ctx, host, port);
	socket.send(packet, boost::posix_time::seconds(timeout));
	return socket.recv(packet, boost::posix_time::seconds(timeout));
}
#endif

nrpe::packet NRPEClient::send_nossl(std::string host, std::string port, int timeout, nrpe::packet packet) {
	boost::asio::io_service io_service;
	nrpe::client::socket socket(io_service, host, port);
	socket.send(packet, boost::posix_time::seconds(timeout));
	return socket.recv(packet, boost::posix_time::seconds(timeout));
}

NSC_WRAP_DLL();
NSC_WRAPPERS_MAIN_DEF(NRPEClient);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF();
NSC_WRAPPERS_CLI_DEF();
NSC_WRAPPERS_HANDLE_NOTIFICATION_DEF();

