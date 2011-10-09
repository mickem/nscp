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
#include <boost/filesystem.hpp>

#include <nscapi/functions.hpp>
#include <config.h>
#include <strEx.h>
#include <net/net.hpp>
#include <nscp/client/socket.hpp>

#include <protobuf/plugin.pb.h>

#include <settings/client/settings_client.hpp>


namespace sh = nscapi::settings_helper;

NSCPClient::NSCPClient() {
}

NSCPClient::~NSCPClient() {
}

bool NSCPClient::loadModule() {
	return false;
}

bool NSCPClient::loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode) {
	std::map<std::wstring,std::wstring> commands;

	try {

		register_command(_T("query_nscp"), _T("Submit a query to a remote host via NSCP"));
		register_command(_T("submit_nscp"), _T("Submit a query to a remote host via NSCP"));
		register_command(_T("exec_nscp"), _T("Execute remote command on a remote host via NSCP"));

		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias(_T("NSCP"), alias, _T("client"));
		target_path = settings.alias().get_settings_path(_T("targets"));

		settings.alias().add_path_to_settings()

			(_T("handlers"), sh::fun_values_path(boost::bind(&NSCPClient::add_command, this, _1, _2)), 
			_T("CLIENT HANDLER SECTION"), _T(""))

			(_T("targets"), sh::fun_values_path(boost::bind(&NSCPClient::add_target, this, _1, _2)), 
			_T("REMOTE TARGET DEFINITIONS"), _T(""))

			;
/*
		settings.alias().add_key_to_settings()

			(_T("payload length"),  sh::uint_key(&buffer_length_, 1024),
			_T("PAYLOAD LENGTH"), _T("Length of payload to/from the NSCP agent. This is a hard specific value so you have to \"configure\" (read recompile) your NSCP agent to use the same value for it to work."))

			;
*/

		settings.register_all();
		settings.notify();

	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Exception caught: <UNKNOWN EXCEPTION>"));
		return false;
	}

	boost::filesystem::wpath p = GET_CORE()->getBasePath() + std::wstring(_T("/security/nrpe_dh_512.pem"));
	cert_ = p.string();
	if (boost::filesystem::is_regular(p)) {
		NSC_DEBUG_MSG_STD(_T("Using certificate: ") + cert_);
	} else {
		NSC_LOG_ERROR_STD(_T("Certificate not found: ") + cert_);
	}

	return true;
}

void NSCPClient::add_target(std::wstring key, std::wstring arg) {
	try {
		targets.add(get_settings_proxy(), target_path , key, arg);
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to add target: ") + key);
	}
}

void NSCPClient::add_command(std::wstring name, std::wstring args) {
	try {
		client::configuration config;
		std::wstring cmd = setup(config, name);
		std::wstring key = commands.add_command(config, name, args);
		if (!key.empty())
			register_command(key.c_str(), _T("Custom command for: ") + name);
	} catch (boost::program_options::validation_error &e) {
		NSC_LOG_ERROR_STD(_T("Could not parse: ") + name + to_wstring(e.what()));
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Could not parse: ") + name);
	}
}

bool NSCPClient::unloadModule() {
	return true;
}

bool NSCPClient::hasCommandHandler() {
	return true;
}
bool NSCPClient::hasMessageHandler() {
	return false;
}

void NSCPClient::add_local_options(po::options_description &desc, nscp_connection_data &command_data) {
	desc.add_options()
		("no-ssl,n", po::value<bool>(&command_data.no_ssl)->zero_tokens()->default_value(false), "Do not initial an ssl handshake with the server, talk in plain text.")
		("cert,c", po::value<std::wstring>(&command_data.cert)->default_value(cert_), "Certificate to use.")
		;
}

std::wstring NSCPClient::setup(client::configuration &config, const std::wstring &command) {
	clp_handler_impl *handler = new clp_handler_impl(this);
	add_local_options(config.local, handler->local_data);
	std::wstring cmd = command;
	if (command.length() > 5 && command.substr(0,5) == _T("nscp_"))
		cmd = command.substr(5);
	if (command.length() > 5 && command.substr(command.length()-5) == _T("_nscp"))
		cmd = command.substr(0, command.length()-5);
	config.handler = client::configuration::handler_type(handler);
	return cmd;
}

NSCAPI::nagiosReturn NSCPClient::handleCommand(const std::wstring &target, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &message, std::wstring &perf) {
	if (command == _T("query_nscp")) {
		client::configuration config;
		std::wstring cmd = setup(config, command);
		return client::command_line_parser::query(config, cmd, arguments, message, perf);
	}
	if (command == _T("submit_nscp")) {
		client::configuration config;
		std::wstring cmd = setup(config, command);
		std::list<std::string> errors = client::command_line_parser::simple_submit(config, cmd, arguments);
		BOOST_FOREACH(std::string p, errors) {
			NSC_LOG_ERROR_STD(utf8::cvt<std::wstring>(p));
		}
		return errors.empty()?NSCAPI::returnOK:NSCAPI::returnCRIT;
	}
	if (command == _T("exec_nscp")) {
		client::configuration config;
		std::wstring cmd = setup(config, command);
		return client::command_line_parser::exec(config, cmd, arguments, message);
	}
	return commands.exec_simple(target, command, arguments, message, perf);
	return NSCAPI::returnIgnored;
}


int NSCPClient::commandLineExec(const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &result) {
	client::configuration config;
	std::wstring cmd = setup(config, command);
	return client::command_line_parser::commandLineExec(config, cmd, arguments, result);
}

//////////////////////////////////////////////////////////////////////////
// Parser implementations
int NSCPClient::clp_handler_impl::query(client::configuration::data_type data, std::string request, std::string &reply) {
	NSCAPI::nagiosReturn ret = NSCAPI::returnOK;
	try {
		std::list<nscp::packet> chunks;
		chunks.push_back(nscp::factory::create_envelope_request(1));
		chunks.push_back(nscp::factory::create_payload(nscp::data::command_request, request, 0));
		chunks = instance->send(data, local_data, chunks);
		BOOST_FOREACH(nscp::packet &chunk, chunks) {
			if (nscp::checks::is_query_response(chunk)) {
				reply = chunk.payload;
			} else if (nscp::checks::is_error(chunk)) {
				NSCPIPC::ErrorMessage message;
				message.ParseFromString(chunk.payload);
				for (int i=0;i<message.error_size();i++) {
					NSC_LOG_ERROR_STD(_T("Error: ") + utf8::cvt<std::wstring>(message.error(i).message()));
				}
				ret = NSCAPI::returnUNKNOWN;
			} else {
				NSC_LOG_ERROR_STD(_T("Unsupported message type: ") + strEx::itos(chunk.signature.payload_type));
				ret = NSCAPI::returnUNKNOWN;
			}
		}
		return ret;
	} catch (std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Exception: ") + utf8::cvt<std::wstring>(e.what()));
		return NSCAPI::returnUNKNOWN;
	}
}
int NSCPClient::clp_handler_impl::submit(client::configuration::data_type data, ::Plugin::Common_Header* header, const std::string &request, std::string &response) {
	std::list<std::string> result;
	try {
		std::list<nscp::packet> chunks;
		chunks.push_back(nscp::factory::create_payload(nscp::data::command_response, request, 0));
		chunks = instance->send(data, local_data, chunks);
		BOOST_FOREACH(nscp::packet &chunk, chunks) {
			if (nscp::checks::is_query_response(chunk)) {
				result.push_back(chunk.payload);
			} else if (nscp::checks::is_error(chunk)) {
				NSCPIPC::ErrorMessage message;
				message.ParseFromString(chunk.payload);
				for (int i=0;i<message.error_size();i++) {
					result.push_back("Error: " + message.error(i).message());
				}
			} else {
				NSC_LOG_ERROR_STD(_T("Unsupported message type: ") + strEx::itos(chunk.signature.payload_type));
				result.push_back("Invalid payload");
			}
		}

		if (result.empty()) {
			std::wstring msg;
			BOOST_FOREACH(std::string &e, result) {
				msg += utf8::cvt<std::wstring>(e);
			}
			nscapi::functions::create_simple_submit_response(_T(""), _T(""), result.empty()?NSCAPI::isSuccess:NSCAPI::hasFailed, msg, response);
		}
		return result.empty()?NSCAPI::isSuccess:NSCAPI::hasFailed;
	} catch (std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Exception: ") + utf8::cvt<std::wstring>(e.what()));
		nscapi::functions::create_simple_submit_response(_T(""), _T(""), NSCAPI::hasFailed, utf8::cvt<std::wstring>(e.what()), response);
		return NSCAPI::hasFailed;
	}
}
int NSCPClient::clp_handler_impl::exec(client::configuration::data_type data, std::string request, std::string &reply) {
	int ret = NSCAPI::returnOK;
	try {
		std::list<nscp::packet> chunks;
		chunks.push_back(nscp::factory::create_envelope_request(1));
		chunks.push_back(nscp::factory::create_payload(nscp::data::exec_request, request, 0));
		chunks = instance->send(data, local_data, chunks);
		BOOST_FOREACH(nscp::packet &chunk, chunks) {
			if (nscp::checks::is_exec_response(chunk)) {
				reply = chunk.payload;
			} else if (nscp::checks::is_error(chunk)) {
				NSCPIPC::ErrorMessage message;
				message.ParseFromString(chunk.payload);
				for (int i=0;i<message.error_size();i++) {
					NSC_LOG_ERROR_STD(_T("Error: ") + utf8::cvt<std::wstring>(message.error(i).message()));
					ret = NSCAPI::returnUNKNOWN;
				}
			} else {
				NSC_LOG_ERROR_STD(_T("Unsupported message type: ") + strEx::itos(chunk.signature.payload_type));
				ret = NSCAPI::returnUNKNOWN;
			}
		}
		return ret;
	} catch (std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Exception: ") + utf8::cvt<std::wstring>(e.what()));
		return NSCAPI::returnUNKNOWN;
	}
}

//////////////////////////////////////////////////////////////////////////
// Socket interface
std::list<nscp::packet> NSCPClient::send(client::configuration::data_type generic_data, nscp_connection_data &data, std::list<nscp::packet> &chunks) {
	chunks.push_front(nscp::factory::create_envelope_request(1));
	std::list<nscp::packet> tmp, result;
	// @ move this to use headers instead
	/*
	std::wstring host = generic_data->host;
	if (host.empty() && !generic_data->target.empty()) {
		nscapi::target_handler::optarget t = targets.find_target(generic_data->target);
		if (t)
			host = (*t).host;
	}
	*/
	net::wurl url = net::parse(_T("..."), 5666);
	if (!data.no_ssl) {
#ifdef USE_SSL
		tmp = send_ssl(url.host, url.port, data.cert, generic_data->timeout, chunks);
#else
		NSC_LOG_ERROR_STD(_T("SSL not avalible (not compiled with USE_SSL)"));
		result.push_back(nscp::factory::create_error(_T("SSL support not available (compiled without USE_SSL)!")));
#endif
	} else {
		tmp = send_nossl(url.host, url.port, generic_data->timeout, chunks);
	}
	BOOST_FOREACH(nscp::packet &p, tmp) {
		if (nscp::checks::is_envelope_response(p)) {
			std::cout << "Got envelope" << std::endl;
		} else {
			result.push_back(p);
		}
	}
	return result;
}

#ifdef USE_SSL
std::list<nscp::packet> NSCPClient::send_ssl(std::wstring host, int port, std::wstring cert, int timeout, const std::list<nscp::packet> &chunks) {
	NSC_DEBUG_MSG_STD(_T("Connecting to: ") + host + _T(":") + strEx::itos(port));
	boost::asio::io_service io_service;
	boost::asio::ssl::context ctx(io_service, boost::asio::ssl::context::sslv23);
	SSL_CTX_set_cipher_list(ctx.impl(), "ADH");
	ctx.use_tmp_dh_file(to_string(cert));
	ctx.set_verify_mode(boost::asio::ssl::context::verify_none);
	nscp::client::ssl_socket socket(io_service, ctx, host, port);
	socket.send(chunks, boost::posix_time::seconds(timeout));
	return socket.recv(boost::posix_time::seconds(timeout));
}
#endif

std::list<nscp::packet> NSCPClient::send_nossl(std::wstring host, int port, int timeout, const std::list<nscp::packet> &chunks) {
	boost::asio::io_service io_service;
	nscp::client::socket socket(io_service, host, port);
	socket.send(chunks, boost::posix_time::seconds(timeout));
	return socket.recv(boost::posix_time::seconds(timeout));
}

NSC_WRAP_DLL();
NSC_WRAPPERS_MAIN_DEF(NSCPClient);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF();
NSC_WRAPPERS_CLI_DEF();

