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
			(_T("EXTERNAL SCRIPT SECTION"), _T("Section for external scripts configuration options (CheckExternalScripts)."))

			(_T("handlers"), sh::fun_values_path(boost::bind(&NRPEClient::addCommand, this, _1, _2)), 
			_T("CLIENT HANDLER SECTION"), _T(""))

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
	return true;

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
		("help,h", "Show this help message.")
		("host,H", po::wvalue<std::wstring>(&command_data.host), "The address of the host running the NRPE daemon")
		("port,p", po::value<int>(&command_data.port), "The port on which the daemon is running (default=5666)")
		("command,c", po::wvalue<std::wstring>(&command_data.command), "The name of the command that the remote daemon should run")
		("timeout,t", po::value<int>(&command_data.timeout), "Number of seconds before connection times out (default=10)")
		("buffer-length,l", po::value<unsigned int>(&command_data.buffer_length), std::string("Length of payload (has to be same as on the server (default=" + to_string(buffer_length_) + ")").c_str())
		("no-ssl,n", po::value<bool>(&command_data.no_ssl)->zero_tokens()->default_value(false), "Do not initial an ssl handshake with the server, talk in plaintext.")
		("arguments,a", po::wvalue<std::vector<std::wstring> >(&command_data.argument_vector), "list of arguments")
		;
}


void NRPEClient::addCommand(std::wstring key, std::wstring args) {
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
NSCAPI::nagiosReturn NRPEClient::handleCommand(const std::wstring command, std::list<std::wstring> arguments, std::wstring &message, std::wstring &perf)
{
	command_list::const_iterator cit = commands.find(strEx::blindstr(command.c_str()));
	if (cit == commands.end())
		return NSCAPI::returnIgnored;

	std::wstring args = (*cit).second.arguments;
	if (SETTINGS_GET_BOOL(nrpe::ALLOW_ARGS) == 1) {
		int i=1;
		BOOST_FOREACH(std::wstring arg, arguments)
		{
			if (SETTINGS_GET_INT(nrpe::ALLOW_NASTY) == 0) {
				if (arg.find_first_of(NASTY_METACHARS) != std::wstring::npos) {
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

int NRPEClient::commandLineExec(const unsigned int argLen, wchar_t** args) {
	try {

		NRPEClient::nrpe_connection_data command_data;
		boost::program_options::variables_map vm;

		po::options_description desc("Allowed options");
		buffer_length_ = SETTINGS_GET_INT(nrpe::PAYLOAD_LENGTH);
		add_options(desc, command_data);

		po::positional_options_description p;
		p.add("arguments", -1);
		po::wparsed_options parsed = basic_command_line_parser_ex<wchar_t>(argLen, args).options(desc).positional(p).run();
		po::store(parsed, vm);
		po::notify(vm);
		command_data.parse_arguments();

		if (vm.count("help")) {
			std::cout << desc << "\n";
			return 1;
		}
		nrpe_result_data result = execute_nrpe_command(command_data, command_data.arguments);
		std::wcout << result.text << std::endl;
		return result.result;
	} catch (boost::program_options::validation_error &e) {
		std::cout << e.what() << std::endl;
	} catch (...) {
		std::cout << "Unknown exception parsing command line" << std::endl;
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
		return nrpe_result_data(NSCAPI::returnUNKNOWN, _T("Socket error: ") + boost::lexical_cast<std::wstring>(e.what()));
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
	return socket.recv(packet, boost::posix_time::seconds(timeout));
}
#endif

nrpe::packet NRPEClient::send_nossl(std::wstring host, int port, int timeout, nrpe::packet packet) {
	boost::asio::io_service io_service;
	nrpe::client::socket socket(io_service, host, port);
	socket.send(packet, boost::posix_time::seconds(timeout));
	return socket.recv(packet, boost::posix_time::seconds(timeout));
}

/*
NRPEPacket NRPEClient::send_nossl(std::wstring host, int port, int timeout, NRPEPacket packet)
{
	unsigned char dh512_p[] = {
		0xCF, 0xFF, 0x65, 0xC2, 0xC8, 0xB4, 0xD2, 0x68, 0x8C, 0xC1, 0x80, 0xB1,
		0x7B, 0xD6, 0xE8, 0xB3, 0x62, 0x59, 0x62, 0xED, 0xA7, 0x45, 0x6A, 0xF8,
		0xE9, 0xD8, 0xBE, 0x3F, 0x38, 0x42, 0x5F, 0xB2, 0xA5, 0x36, 0x03, 0xD3,
		0x06, 0x27, 0x81, 0xC8, 0x9B, 0x88, 0x50, 0x3B, 0x82, 0x3D, 0x31, 0x45,
		0x2C, 0xB4, 0xC5, 0xA5, 0xBE, 0x6A, 0xE3, 0x2E, 0xA6, 0x86, 0xFD, 0x6A,
		0x7E, 0x1E, 0x6A, 0x73,
	};
	unsigned char dh512_g[] = { 0x02, };

	DH *dh_2 = DH_new();
	dh_2->p = BN_bin2bn(dh512_p, sizeof(dh512_p), NULL);
	dh_2->g = BN_bin2bn(dh512_g, sizeof(dh512_g), NULL);

	FILE *outfile = fopen("d:\\nrpe_512.pem", "w");
	PEM_write_DHparams(outfile, dh_2);
	PEM_write_DHparams(stdout, dh_2);
	fclose(outfile);

	nrpe_socket socket(host, port);
	socket.send(packet, boost::posix_time::seconds(timeout));
	return socket.recv(packet, boost::posix_time::seconds(timeout));
}
*/





NSC_WRAP_DLL();
NSC_WRAPPERS_MAIN_DEF(gNRPEClient);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gNRPEClient);
NSC_WRAPPERS_CLI_DEF(gNRPEClient);

