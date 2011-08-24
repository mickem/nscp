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
#include <nscp/client/socket.hpp>

#include <protobuf/plugin.pb.h>

#include <settings/client/settings_client.hpp>


namespace sh = nscapi::settings_helper;

NSCPClient gNSCPClient;

NSCPClient::NSCPClient() : buffer_length_(0) {
}

NSCPClient::~NSCPClient() {
}

bool NSCPClient::loadModule() {
	return false;
}

bool NSCPClient::loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode) {
	std::map<std::wstring,std::wstring> commands;

	try {

		get_core()->registerCommand(_T("query_nscp"), _T("Submit a query to a remote host via NSCP"));
		//"/settings/NSCP/client/handlers"
		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias(_T("NSCP"), alias, _T("client"));

		settings.alias().add_path_to_settings()

			(_T("handlers"), sh::fun_values_path(boost::bind(&NSCPClient::add_command, this, _1, _2)), 
			_T("CLIENT HANDLER SECTION"), _T(""))

			(_T("servers"), sh::fun_values_path(boost::bind(&NSCPClient::add_server, this, _1, _2)), 
			_T("REMOTE SERVER DEFINITIONS"), _T(""))

			;

		settings.alias().add_key_to_settings()

			(_T("payload length"),  sh::uint_key(&buffer_length_, 1024),
			_T("PAYLOAD LENGTH"), _T("Length of payload to/from the NSCP agent. This is a hard specific value so you have to \"configure\" (read recompile) your NSCP agent to use the same value for it to work."))

			;


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

void NSCPClient::add_options(po::options_description &desc, nscp_connection_data &command_data) {
	desc.add_options()
		("host,H", po::wvalue<std::wstring>(&command_data.host), "The address of the host running the NSCP daemon")
		("port,p", po::value<int>(&command_data.port), "The port on which the daemon is running (default=5668)")
		("command,c", po::wvalue<std::wstring>(&command_data.command), "The name of the command that the remote daemon should run")
		("timeout,t", po::value<int>(&command_data.timeout), "Number of seconds before connection times out (default=10)")
		("no-ssl,n", po::value<bool>(&command_data.no_ssl)->zero_tokens()->default_value(false), "Do not initial an ssl handshake with the server, talk in plaintext.")
		("arguments,a", po::wvalue<std::vector<std::wstring> >(&command_data.arguments), "list of arguments")
		;
}

void NSCPClient::add_server(std::wstring key, std::wstring args) {
}

void NSCPClient::add_command(std::wstring key, std::wstring args) {
	try {

		NSCPClient::nscp_connection_data command_data;
		boost::program_options::variables_map vm;

		po::options_description desc("Allowed options");
		add_options(desc, command_data);

		po::positional_options_description p;
		p.add("arguments", -1);

		std::vector<std::wstring> list;
		//explicit escaped_list_separator(Char e = '\\', Char c = ',',Char q = '\"')
		boost::escaped_list_separator<wchar_t> sep(L'\\', L' ', L'\"');
		typedef boost::tokenizer<boost::escaped_list_separator<wchar_t>,std::wstring::const_iterator, std::wstring > tokenizer_t;
		tokenizer_t tok(args, sep);
		for(tokenizer_t::iterator beg=tok.begin(); beg!=tok.end();++beg){
			list.push_back(*beg);
		}

		po::wparsed_options parsed = po::basic_command_line_parser<wchar_t>(list).options(desc).positional(p).run();
		po::store(parsed, vm);
		po::notify(vm);

		NSC_DEBUG_MSG_STD(_T("Added NSCP Client: ") + key.c_str() + _T(" = ") + command_data.toString());
		commands[key.c_str()] = command_data;

		GET_CORE()->registerCommand(key.c_str(), command_data.toString());

	} catch (boost::program_options::validation_error &e) {
		NSC_LOG_ERROR_STD(_T("Could not parse: ") + key.c_str() + strEx::string_to_wstring(e.what()));
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Could not parse: ") + key.c_str());
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
NSCAPI::nagiosReturn NSCPClient::handleCommand(const std::wstring &target, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &message, std::wstring &perf) {
	if (command == _T("query_nscp")) {
		return query_nscp(arguments, message, perf);
	}
	command_list::const_iterator cit = commands.find(command);
	if (cit == commands.end())
		return NSCAPI::returnIgnored;

	nscp_result_data r; // = execute_nscp_command((*cit).second, arguments);
	message = r.text;
	return r.result;
}



NSCAPI::nagiosReturn NSCPClient::query_nscp(std::list<std::wstring> &arguments, std::wstring &message, std::wstring perf) {
	try {
		NSCPClient::nscp_connection_data command_data;
		boost::program_options::variables_map vm;

		po::options_description desc("Allowed options");
		add_options(desc, command_data);

		std::vector<std::wstring> vargs(arguments.begin(), arguments.end());
		po::positional_options_description p;
		p.add("arguments", -1);
		po::wparsed_options parsed = po::basic_command_line_parser<wchar_t>(vargs).options(desc).positional(p).run();
		po::store(parsed, vm);
		po::notify(vm);

		std::string foo = "foobar";
		execute_nscp_command(command_data, foo);
	} catch (boost::program_options::validation_error &e) {
		message = _T("Error: ") + utf8::cvt<std::wstring>(e.what());
		return NSCAPI::returnUNKNOWN;
	} catch (...) {
		message = _T("Unknown exception parsing command line");
		return NSCAPI::returnUNKNOWN;
	}
	return NSCAPI::returnUNKNOWN;
}

int NSCPClient::commandLineExec(const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &result) {
	if (command != _T("query_nscp") && command != _T("help"))
		return NSCAPI::returnIgnored;
	try {
		NSCPClient::nscp_connection_data command_data;
		boost::program_options::variables_map vm;

		po::options_description desc("Allowed options");
		add_options(desc, command_data);

		std::vector<std::wstring> vargs(arguments.begin(), arguments.end());
		po::positional_options_description p;
		p.add("arguments", -1);
		po::wparsed_options parsed = po::basic_command_line_parser<wchar_t>(vargs).options(desc).positional(p).run();
		po::store(parsed, vm);
		po::notify(vm);

		if (command == _T("help")) {
			std::stringstream ss;
			ss << "NSCPClient Command line syntax for command: query" << std::endl;;
			ss << desc;
			result = utf8::cvt<std::wstring>(ss.str());
			return NSCAPI::returnOK;
		}

		std::string buffer;
		nscapi::functions::create_simple_query_request(command_data.command, command_data.arguments, buffer);
		execute_nscp_command(command_data, buffer);
		nscp_result_data res;// = execute_nscp_command(command_data, command_data.arguments);
		result = res.text;
		return res.result;
	} catch (boost::program_options::validation_error &e) {
		result = _T("Error: ") + utf8::cvt<std::wstring>(e.what());
		return NSCAPI::returnUNKNOWN;
	} catch (...) {
		result = _T("Unknown exception parsing command line");
		return NSCAPI::returnUNKNOWN;
	}
	return NSCAPI::returnUNKNOWN;
}
NSCPClient::nscp_result_data NSCPClient::execute_nscp_command(nscp_connection_data con, std::string buffer) {
	try {
		std::list<nscp::packet> chunks;
		chunks.push_back(nscp::packet::build_envelope_request(1));
		chunks.push_back(nscp::packet::create_payload(nscp::data::command_request, buffer, 0));
		if (!con.no_ssl) {
#ifdef USE_SSL
			chunks = send_ssl(con.host, con.port, con.timeout, chunks);
#else
			NSC_LOG_ERROR_STD(_T("SSL not avalible (not compiled with USE_SSL)"));
			return nscp_result_data(NSCAPI::returnUNKNOWN, _T("SSL support not available (compiled without USE_SSL)!"));
#endif
		} else {
			chunks = send_nossl(con.host, con.port, con.timeout, chunks);
		}
		BOOST_FOREACH(nscp::packet &chunk, chunks) {
			NSC_DEBUG_MSG_STD(_T("Found chunk: ") + utf8::cvt<std::wstring>(strEx::format_buffer(chunk.payload.c_str(), chunk.payload.size())));
		}
		return nscp_result_data(NSCAPI::returnUNKNOWN, _T("Hello"));
	} catch (nscp::nscp_exception &e) {
		NSC_LOG_ERROR_STD(_T("Socket error: ") + utf8::cvt<std::wstring>(e.what()));
		return nscp_result_data(NSCAPI::returnUNKNOWN, _T("Socket error: ") + utf8::cvt<std::wstring>(e.what()));
	} catch (std::runtime_error &e) {
		NSC_LOG_ERROR_STD(_T("Socket error: ") + utf8::cvt<std::wstring>(e.what()));
		return nscp_result_data(NSCAPI::returnUNKNOWN, _T("Socket error: ") + utf8::cvt<std::wstring>(e.what()));
	} catch (std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Exception: ") + utf8::cvt<std::wstring>(e.what()));
		return nscp_result_data(NSCAPI::returnUNKNOWN, _T("Socket error: ") + utf8::cvt<std::wstring>(e.what()));
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Unknown exception..."));
		return nscp_result_data(NSCAPI::returnUNKNOWN, _T("Unknown error -- REPORT THIS!"));
	}
}


#ifdef USE_SSL
std::list<nscp::packet> NSCPClient::send_ssl(std::wstring host, int port, int timeout, std::list<nscp::packet> &chunks) {
	boost::asio::io_service io_service;
	boost::asio::ssl::context ctx(io_service, boost::asio::ssl::context::sslv23);
	SSL_CTX_set_cipher_list(ctx.impl(), "ADH");
	ctx.use_tmp_dh_file(to_string(cert_));
	ctx.set_verify_mode(boost::asio::ssl::context::verify_none);
	nscp::client::ssl_socket socket(io_service, ctx, host, port);
	socket.send(chunks, boost::posix_time::seconds(timeout));
	return socket.recv(boost::posix_time::seconds(timeout));
}
#endif

std::list<nscp::packet> NSCPClient::send_nossl(std::wstring host, int port, int timeout, std::list<nscp::packet> &chunks) {
	boost::asio::io_service io_service;
	nscp::client::socket socket(io_service, host, port);
	socket.send(chunks, boost::posix_time::seconds(timeout));
	return socket.recv(boost::posix_time::seconds(timeout));
}

NSC_WRAP_DLL();
NSC_WRAPPERS_MAIN_DEF(gNSCPClient);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gNSCPClient);
NSC_WRAPPERS_CLI_DEF(gNSCPClient);

