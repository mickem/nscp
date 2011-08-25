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
#include <strEx.h>
#include <time.h>
#include <config.h>
#include <strEx.h>
#include <boost/filesystem.hpp>
#include <strEx.h>
#include <nrpe/client/socket.hpp>

#include <settings/client/settings_client.hpp>


namespace setting_keys {

	// NSClient Setting headlines
	namespace nrpe {
		DEFINE_PATH(SECTION, NRPE_SECTION_PROTOCOL);
		//DESCRIBE_SETTING(SECTION, "NRPE SECTION", "Section for NRPE (NRPEListener.dll) (check_nrpe) protocol options.");


		DEFINE_PATH(CH_SECTION, NRPE_CLIENT_HANDLER_SECTION);
		//DESCRIBE_SETTING(CH_SECTION, "CLIENT HANDLER SECTION", "...");

		DEFINE_SETTING_S(ALLOWED_HOSTS, NRPE_SECTION_PROTOCOL, GENERIC_KEY_ALLOWED_HOSTS, "");
		DESCRIBE_SETTING(ALLOWED_HOSTS, "ALLOWED HOST ADDRESSES", "This is a comma-delimited list of IP address of hosts that are allowed to talk to NSClient deamon. If you leave this blank the global version will be used instead.");

		DEFINE_SETTING_I(PORT, NRPE_SECTION_PROTOCOL, "port", 5666);
		//DESCRIBE_SETTING(PORT, "NSCLIENT PORT NUMBER", "This is the port the NSClientListener.dll will listen to.");

		DEFINE_SETTING_S(BINDADDR, NRPE_SECTION_PROTOCOL, GENERIC_KEY_BIND_TO, "");
		//DESCRIBE_SETTING(BINDADDR, "BIND TO ADDRESS", "Allows you to bind server to a specific local address. This has to be a dotted ip adress not a hostname. Leaving this blank will bind to all avalible IP adresses.");

		DEFINE_SETTING_I(READ_TIMEOUT, NRPE_SECTION_PROTOCOL, GENERIC_KEY_SOCK_READ_TIMEOUT, 30);
		//DESCRIBE_SETTING(READ_TIMEOUT, "SOCKET TIMEOUT", "Timeout when reading packets on incoming sockets. If the data has not arrived withint this time we will bail out.");

		DEFINE_SETTING_I(LISTENQUE, NRPE_SECTION_PROTOCOL, GENERIC_KEY_SOCK_LISTENQUE, 0);
		//DESCRIBE_SETTING_ADVANCED(LISTENQUE, "LISTEN QUEUE", "Number of sockets to queue before starting to refuse new incoming connections. This can be used to tweak the amount of simultaneous sockets that the server accepts.");

		DEFINE_SETTING_I(THREAD_POOL, NRPE_SECTION_PROTOCOL, "thread pool", 10);
		//DESCRIBE_SETTING_ADVANCED(THREAD_POOL, "THREAD POOL", "");



		DEFINE_SETTING_B(CACHE_ALLOWED, NRPE_SECTION_PROTOCOL, GENERIC_KEY_SOCK_CACHE_ALLOWED, false);
		DESCRIBE_SETTING_ADVANCED(CACHE_ALLOWED, "ALLOWED HOSTS CACHING", "Used to cache looked up hosts if you check dynamic/changing hosts set this to false.");

		//DEFINE_SETTING_B(KEYUSE_SSL, NRPE_SECTION_PROTOCOL, GENERIC_KEY_USE_SSL, true);
		//DESCRIBE_SETTING(KEYUSE_SSL, "USE SSL SOCKET", "This option controls if SSL should be used on the socket.");

		DEFINE_SETTING_I(PAYLOAD_LENGTH, NRPE_SECTION_PROTOCOL, "payload length", 1024);
		//DESCRIBE_SETTING_ADVANCED(PAYLOAD_LENGTH, "PAYLOAD LENGTH", "Length of payload to/from the NRPE agent. This is a hard specific value so you have to \"configure\" (read recompile) your NRPE agent to use the same value for it to work.");

		//DEFINE_SETTING_B(ALLOW_PERFDATA, NRPE_SECTION, "performance data", true);
		//DESCRIBE_SETTING_ADVANCED(ALLOW_PERFDATA, "PERFORMANCE DATA", "Send performance data back to nagios (set this to 0 to remove all performance data).");

		//DEFINE_SETTING_I(CMD_TIMEOUT, NRPE_SECTION, "command timeout", 60);
		//DESCRIBE_SETTING(CMD_TIMEOUT, "COMMAND TIMEOUT", "This specifies the maximum number of seconds that the NRPE daemon will allow plug-ins to finish executing before killing them off.");

		DEFINE_SETTING_B(ALLOW_ARGS, NRPE_SECTION, "allow arguments", false);
		//DESCRIBE_SETTING(ALLOW_ARGS, "COMMAND ARGUMENT PROCESSING", "This option determines whether or not the NRPE daemon will allow clients to specify arguments to commands that are executed.");

		DEFINE_SETTING_B(ALLOW_NASTY, NRPE_SECTION, "allow nasy characters", false);
		//DESCRIBE_SETTING(ALLOW_NASTY, "COMMAND ALLOW NASTY META CHARS", "This option determines whether or not the NRPE daemon will allow clients to specify nasty (as in |`&><'\"\\[]{}) characters in arguments.");

	}
}
namespace sh = nscapi::settings_helper;

NRPEClient gNRPEClient;

NRPEClient::NRPEClient() : buffer_length_(0) {
}

NRPEClient::~NRPEClient() {
}

bool NRPEClient::loadModule() {
	return false;
}

bool NRPEClient::loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode) {
	std::map<std::wstring,std::wstring> commands;

	try {

		//"/settings/NRPE/client/handlers"
		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias(_T("NRPE"), alias, _T("client"));

		settings.alias().add_path_to_settings()

			(_T("handlers"), sh::fun_values_path(boost::bind(&NRPEClient::add_command, this, _1, _2)), 
			_T("CLIENT HANDLER SECTION"), _T(""))

			(_T("servers"), sh::fun_values_path(boost::bind(&NRPEClient::add_server, this, _1, _2)), 
			_T("REMOTE SERVER DEFINITIONS"), _T(""))

			;

		settings.alias().add_key_to_settings()

			(_T("payload length"),  sh::uint_key(&buffer_length_, 1024),
			_T("PAYLOAD LENGTH"), _T("Length of payload to/from the NRPE agent. This is a hard specific value so you have to \"configure\" (read recompile) your NRPE agent to use the same value for it to work."))

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

void NRPEClient::add_options(po::options_description &desc, nrpe_connection_data &command_data) {
	desc.add_options()
		("host,H", po::wvalue<std::wstring>(&command_data.host), "The address of the host running the NRPE daemon")
		("port,p", po::value<int>(&command_data.port), "The port on which the daemon is running (default=5666)")
		("command,c", po::wvalue<std::wstring>(&command_data.command), "The name of the command that the remote daemon should run")
		("timeout,t", po::value<int>(&command_data.timeout), "Number of seconds before connection times out (default=10)")
		("buffer-length,l", po::value<unsigned int>(&command_data.buffer_length), std::string("Length of payload (has to be same as on the server (default=" + to_string(buffer_length_) + ")").c_str())
		("no-ssl,n", po::value<bool>(&command_data.no_ssl)->zero_tokens()->default_value(false), "Do not initial an ssl handshake with the server, talk in plaintext.")
		("arguments,a", po::wvalue<std::vector<std::wstring> >(&command_data.argument_vector), "list of arguments")
		;
}

void NRPEClient::add_server(std::wstring key, std::wstring args) {
}

void NRPEClient::add_command(std::wstring key, std::wstring args) {
	try {

		NRPEClient::nrpe_connection_data command_data;
		boost::program_options::variables_map vm;

		po::options_description desc("Allowed options");
		buffer_length_ = SETTINGS_GET_INT(nrpe::PAYLOAD_LENGTH);
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
		command_data.parse_arguments();

		NSC_DEBUG_MSG_STD(_T("Added NRPE Client: ") + key.c_str() + _T(" = ") + command_data.toString());
		commands[key.c_str()] = command_data;

		GET_CORE()->registerCommand(key.c_str(), command_data.toString());

	} catch (boost::program_options::validation_error &e) {
		NSC_LOG_ERROR_STD(_T("Could not parse: ") + key.c_str() + strEx::string_to_wstring(e.what()));
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Could not parse: ") + key.c_str());
	}
}

bool NRPEClient::unloadModule() {
	return true;
}

bool NRPEClient::hasCommandHandler() {
	return true;
}
bool NRPEClient::hasMessageHandler() {
	return false;
}
NSCAPI::nagiosReturn NRPEClient::handleCommand(const std::wstring &target, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &message, std::wstring &perf) {
	command_list::const_iterator cit = commands.find(strEx::blindstr(command.c_str()));
	if (cit == commands.end())
		return NSCAPI::returnIgnored;

	std::wstring args = (*cit).second.arguments;
	if (SETTINGS_GET_BOOL(nrpe::ALLOW_ARGS) == 1) {
		int i=1;
		BOOST_FOREACH(std::wstring arg, arguments)
		{
			if (SETTINGS_GET_INT(nrpe::ALLOW_NASTY) == 0) {
				if (arg.find_first_of(NASTY_METACHARS_W) != std::wstring::npos) {
					NSC_LOG_ERROR(_T("Request string contained illegal metachars!"));
					return NSCAPI::returnIgnored;
				}
			}
			strEx::replace(args, _T("$ARG") + strEx::itos(i++) + _T("$"), arg);
		}
	}

	NSC_DEBUG_MSG_STD(_T("Rewrote command arguments: ") + args);
	nrpe_result_data r = execute_nrpe_command((*cit).second, args);
	message = r.text;
	return r.result;
}

int NRPEClient::commandLineExec(const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &result) {
	if (command != _T("query_nrpe") && command != _T("help"))
		return NSCAPI::returnIgnored;
	try {
		NRPEClient::nrpe_connection_data command_data;
		boost::program_options::variables_map vm;

		po::options_description desc("Allowed options");
		buffer_length_ = SETTINGS_GET_INT(nrpe::PAYLOAD_LENGTH);
		add_options(desc, command_data);

		std::vector<std::wstring> vargs(arguments.begin(), arguments.end());
		po::positional_options_description p;
		p.add("arguments", -1);
		po::wparsed_options parsed = po::basic_command_line_parser<wchar_t>(vargs).options(desc).positional(p).run();
		po::store(parsed, vm);
		po::notify(vm);
		command_data.parse_arguments();
		if (command == _T("help")) {
			std::stringstream ss;
			ss << "NRPEClient Command line syntax for command: query" << std::endl;;
			ss << desc;
			result = utf8::cvt<std::wstring>(ss.str());
			return NSCAPI::returnOK;
		}

		nrpe_result_data res = execute_nrpe_command(command_data, command_data.arguments);
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
NRPEClient::nrpe_result_data NRPEClient::execute_nrpe_command(nrpe_connection_data con, std::wstring arguments) {
	try {
		nrpe::packet packet;
		if (!con.no_ssl) {
#ifdef USE_SSL
			packet = send_ssl(con.host, con.port, con.timeout, nrpe::packet::make_request(con.get_cli(arguments), con.buffer_length));
#else
			NSC_LOG_ERROR_STD(_T("SSL not avalible (not compiled with USE_SSL)"));
			return nrpe_result_data(NSCAPI::returnUNKNOWN, _T("SSL support not available (compiled without USE_SSL)!"));
#endif
		} else
			packet = send_nossl(con.host, con.port, con.timeout, nrpe::packet::make_request(con.get_cli(arguments), con.buffer_length));
		return nrpe_result_data(packet.getResult(), packet.getPayload());
	} catch (nrpe::nrpe_packet_exception &e) {
		return nrpe_result_data(NSCAPI::returnUNKNOWN, _T("NRPE Packet errro: ") + e.getMessage());
	} catch (std::runtime_error &e) {
		NSC_LOG_ERROR_STD(_T("Socket error: ") + utf8::cvt<std::wstring>(e.what()));
		return nrpe_result_data(NSCAPI::returnUNKNOWN, _T("Socket error: ") + utf8::cvt<std::wstring>(e.what()));
	} catch (...) {
		return nrpe_result_data(NSCAPI::returnUNKNOWN, _T("Unknown error -- REPORT THIS!"));
	}
}


#ifdef USE_SSL
nrpe::packet NRPEClient::send_ssl(std::wstring host, int port, int timeout, nrpe::packet packet) {
	boost::asio::io_service io_service;
	boost::asio::ssl::context ctx(io_service, boost::asio::ssl::context::sslv23);
	SSL_CTX_set_cipher_list(ctx.impl(), "ADH");
	ctx.use_tmp_dh_file(to_string(cert_));
	ctx.set_verify_mode(boost::asio::ssl::context::verify_none);
	nrpe::client::ssl_socket socket(io_service, ctx, host, port);
	socket.send(packet, boost::posix_time::seconds(timeout));
	nrpe::packet ret = socket.recv(packet, boost::posix_time::seconds(timeout));
	return ret;
}
#endif

nrpe::packet NRPEClient::send_nossl(std::wstring host, int port, int timeout, nrpe::packet packet) {
	boost::asio::io_service io_service;
	nrpe::client::socket socket(io_service, host, port);
	socket.send(packet, boost::posix_time::seconds(timeout));
	return socket.recv(packet, boost::posix_time::seconds(timeout));
}

NSC_WRAP_DLL();
NSC_WRAPPERS_MAIN_DEF(gNRPEClient);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gNRPEClient);
NSC_WRAPPERS_CLI_DEF(gNRPEClient);

