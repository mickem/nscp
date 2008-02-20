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
#include <msvc_wrappers.h>
#include <execute_process.hpp>
#include <program_options_ex.hpp>

NRPEClient gNRPEClient;

BOOL APIENTRY DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	NSCModuleWrapper::wrapDllMain(hModule, ul_reason_for_call);
	return TRUE;
}

NRPEClient::NRPEClient() : buffer_length_(0) {
}

NRPEClient::~NRPEClient() {
}

bool NRPEClient::loadModule() {
	timeout = NSCModuleHelper::getSettingsInt(NRPE_SECTION_TITLE, NRPE_SETTINGS_TIMEOUT ,NRPE_SETTINGS_TIMEOUT_DEFAULT);
	socketTimeout_ = NSCModuleHelper::getSettingsInt(NRPE_SECTION_TITLE, NRPE_SETTINGS_READ_TIMEOUT ,NRPE_SETTINGS_READ_TIMEOUT_DEFAULT);
	buffer_length_ = NSCModuleHelper::getSettingsInt(NRPE_SECTION_TITLE, NRPE_SETTINGS_STRLEN, NRPE_SETTINGS_STRLEN_DEFAULT);
	return true;
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
NSCAPI::nagiosReturn NRPEClient::handleCommand(const strEx::blindstr command, const unsigned int argLen, TCHAR **char_args, std::wstring &message, std::wstring &perf)
{
	return 0;
}



int NRPEClient::commandLineExec(const TCHAR* command, const unsigned int argLen, TCHAR** args) {
		try {
			boost::program_options::options_description desc("Allowed options");
			buffer_length_ = NSCModuleHelper::getSettingsInt(NRPE_SECTION_TITLE, NRPE_SETTINGS_STRLEN, NRPE_SETTINGS_STRLEN_DEFAULT);
			desc.add_options()
				("help,h", "Show this help message.")
				("host,H", boost::program_options::wvalue<std::wstring>(), "The address of the host running the NRPE daemon")
				("port,p", boost::program_options::value<int>(), "The port on which the daemon is running (default=5666)")
				("command,c", boost::program_options::wvalue<std::wstring>(), "The name of the command that the remote daemon should run")
				("timeout,t", boost::program_options::value<int>(), "Number of seconds before connection times out (default=10)")
				("buffer-length,l", boost::program_options::value<int>(), std::string("Length of payload (has to be same as on the server (default=" + strEx::s::itos(buffer_length_) + ")").c_str())
				("no-ssl,n", "Do not initial an ssl handshake with the server, talk in plaintext.")
				("arguments,a", boost::program_options::wvalue<std::vector<std::wstring>>(), "list of arguments")
				;
			boost::program_options::positional_options_description p;
			p.add("arguments", -1);
			
			boost::program_options::variables_map vm;
			boost::program_options::store(
				basic_command_line_parser_ex<TCHAR>(command, argLen, args).options(desc).positional(p).run()
				, vm);
			boost::program_options::notify(vm);    

			if (vm.count("help")) {
				std::cout << desc << "\n";
				return 1;
			}
			std::wstring host = _T("localhost");
			std::wstring command;
			std::wstring arguments;
			int port = 5666;
			int timeout = 10;
			bool ssl = true;

			if (vm.count("host"))
				host = vm["host"].as<std::wstring>();
			if (vm.count("port"))
				port = vm["port"].as<int>();
			if (vm.count("timeout"))
				timeout = vm["timeout"].as<int>();
			if (vm.count("buffer-length"))
				buffer_length_ = vm["buffer-length"].as<int>();
			if (vm.count("command"))
				command = vm["command"].as<std::wstring>();
			if (vm.count("arguments")) {
				std::vector<std::wstring> v = vm["arguments"].as<std::vector<std::wstring>>();
				for (std::vector<std::wstring>::const_iterator cit = v.begin(); cit != v.end(); ++cit) {
					if (!arguments.empty())
						arguments += _T("!");
					arguments += *cit;
				}
			}
			if (vm.count("no-ssl"))
				ssl = false;
			return execute_nrpe_command(host, port, ssl, timeout, command, arguments);
		} catch (boost::program_options::validation_error &e) {
			std::cout << e.what() << std::endl;
		} catch (...) {
			std::cout << "Unknown exception parsing command line" << std::endl;
		}
	return 0;
}
int NRPEClient::execute_nrpe_command(std::wstring host, int port, bool ssl, int timeout, std::wstring command, std::wstring arguments) {
	try {
		std::wstring cmd = command;
		if (cmd.empty())
			cmd = _T("_NRPE_CHECK");
		if (!arguments.empty())
			cmd += _T("!") + arguments;
		NRPEPacket packet;
		if (ssl)
			packet = send_ssl(host, port, timeout, NRPEPacket::make_request(cmd, buffer_length_));
		else
			packet = send_nossl(host, port, timeout, NRPEPacket::make_request(cmd, buffer_length_));
		std::wcout << packet.getPayload() << std::endl;
		return packet.getResult();
	} catch (simpleSocket::SocketException &e) {
		std::wcout << _T("whoops...") << e.getMessage() <<  std::endl;
	} catch (simpleSSL::SSLException &e) {
		std::wcout << _T("whoops...") << e.getMessage() <<  std::endl;
	} catch (...) {
		std::cout << "whoops..." << std::endl;
	}
	return NSCAPI::returnUNKNOWN;
}
NRPEPacket NRPEClient::send_ssl(std::wstring host, int port, int timeout, NRPEPacket packet)
{
	simpleSSL::Socket socket(true);
	socket.connect(host, port);
	socket.sendAll(packet.getBuffer(), packet.getBufferLength());
	simpleSocket::DataBuffer buffer;
	socket.readAll(buffer);
	packet.readFrom(buffer.getBuffer(), buffer.getLength());
	return packet;
}
NRPEPacket NRPEClient::send_nossl(std::wstring host, int port, int timeout, NRPEPacket packet)
{
	simpleSocket::Socket socket(true);
	socket.connect(host, port);
	socket.sendAll(packet.getBuffer(), packet.getBufferLength());
	simpleSocket::DataBuffer buffer;
	socket.readAll(buffer);
	packet.readFrom(buffer.getBuffer(), buffer.getLength());
	return packet;
}




NRPEPacket NRPEClient::handlePacket(NRPEPacket p) {
	if (p.getType() != NRPEPacket::queryPacket) {
		NSC_LOG_ERROR(_T("Request is not a query."));
		throw NRPEException(_T("Invalid query type"));
	}
	if (p.getVersion() != NRPEPacket::version2) {
		NSC_LOG_ERROR(_T("Request had unsupported version."));
		throw NRPEException(_T("Invalid version"));
	}
	if (!p.verifyCRC()) {
		NSC_LOG_ERROR(_T("Request had invalid checksum."));
		throw NRPEException(_T("Invalid checksum"));
	}
	strEx::token cmd = strEx::getToken(p.getPayload(), '!');
	if (cmd.first == _T("_NRPE_CHECK")) {
		return NRPEPacket(NRPEPacket::responsePacket, NRPEPacket::version2, NSCAPI::returnOK, _T("I (") SZVERSION _T(") seem to be doing fine..."), buffer_length_);
	}
	std::wstring msg, perf;

	if (NSCModuleHelper::getSettingsInt(NRPE_SECTION_TITLE, NRPE_SETTINGS_ALLOW_ARGUMENTS, NRPE_SETTINGS_ALLOW_ARGUMENTS_DEFAULT) == 0) {
		if (!cmd.second.empty()) {
			NSC_LOG_ERROR(_T("Request contained arguments (not currently allowed, check the allow_arguments option)."));
			throw NRPEException(_T("Request contained arguments (not currently allowed, check the allow_arguments option)."));
		}
	}
	if (NSCModuleHelper::getSettingsInt(NRPE_SECTION_TITLE, NRPE_SETTINGS_ALLOW_NASTY_META, NRPE_SETTINGS_ALLOW_NASTY_META_DEFAULT) == 0) {
		if (cmd.first.find_first_of(NASTY_METACHARS) != std::wstring::npos) {
			NSC_LOG_ERROR(_T("Request command contained illegal metachars!"));
			throw NRPEException(_T("Request command contained illegal metachars!"));
		}
		if (cmd.second.find_first_of(NASTY_METACHARS) != std::wstring::npos) {
			NSC_LOG_ERROR(_T("Request arguments contained illegal metachars!"));
			throw NRPEException(_T("Request command contained illegal metachars!"));
		}
	}

	NSCAPI::nagiosReturn ret = -3;
	try {
		ret = NSCModuleHelper::InjectSplitAndCommand(cmd.first, cmd.second, '!', msg, perf);
	} catch (...) {
		return NRPEPacket(NRPEPacket::responsePacket, NRPEPacket::version2, NSCAPI::returnUNKNOWN, _T("UNKNOWN: Internal exception"), buffer_length_);
	}
	switch (ret) {
		case NSCAPI::returnInvalidBufferLen:
			msg = _T("UNKNOWN: Return buffer to small to handle this command.");
			ret = NSCAPI::returnUNKNOWN;
			break;
		case NSCAPI::returnIgnored:
			msg = _T("UNKNOWN: No handler for that command");
			ret = NSCAPI::returnUNKNOWN;
			break;
		case NSCAPI::returnOK:
		case NSCAPI::returnWARN:
		case NSCAPI::returnCRIT:
		case NSCAPI::returnUNKNOWN:
			break;
		default:
			msg = _T("UNKNOWN: Internal error.");
			ret = NSCAPI::returnUNKNOWN;
	}
	if (msg.length() > buffer_length_) {
		NSC_LOG_ERROR(_T("Truncating returndata as it is bigger then NRPE allowes :("));
		msg = msg.substr(0,buffer_length_-1);
	}
	if (perf.empty()||noPerfData_) {
		return NRPEPacket(NRPEPacket::responsePacket, NRPEPacket::version2, ret, msg, buffer_length_);
	} else {
		return NRPEPacket(NRPEPacket::responsePacket, NRPEPacket::version2, ret, msg + _T("|") + perf, buffer_length_);
	}
}

NSC_WRAPPERS_MAIN_DEF(gNRPEClient);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gNRPEClient);
NSC_WRAPPERS_HANDLE_CONFIGURATION(gNRPEClient);
NSC_WRAPPERS_CLI_DEF(gNRPEClient);


MODULE_SETTINGS_START(NRPEClient, _T("NRPE Listener configuration"), _T("...")) 

PAGE(_T("NRPE Listsner configuration")) 

ITEM_EDIT_TEXT(_T("port"), _T("This is the port the NRPEClient.dll will listen to.")) 
ITEM_MAP_TO(_T("basic_ini_text_mapper")) 
OPTION(_T("section"), _T("NRPE")) 
OPTION(_T("key"), _T("port")) 
OPTION(_T("default"), _T("5666")) 
ITEM_END()

ITEM_CHECK_BOOL(_T("allow_arguments"), _T("This option determines whether or not the NRPE daemon will allow clients to specify arguments to commands that are executed.")) 
ITEM_MAP_TO(_T("basic_ini_bool_mapper")) 
OPTION(_T("section"), _T("NRPE")) 
OPTION(_T("key"), _T("allow_arguments")) 
OPTION(_T("default"), _T("false")) 
OPTION(_T("true_value"), _T("1")) 
OPTION(_T("false_value"), _T("0")) 
ITEM_END()

ITEM_CHECK_BOOL(_T("allow_nasty_meta_chars"), _T("This might have security implications (depending on what you do with the options)")) 
ITEM_MAP_TO(_T("basic_ini_bool_mapper")) 
OPTION(_T("section"), _T("NRPE")) 
OPTION(_T("key"), _T("allow_nasty_meta_chars")) 
OPTION(_T("default"), _T("false")) 
OPTION(_T("true_value"), _T("1")) 
OPTION(_T("false_value"), _T("0")) 
ITEM_END()

ITEM_CHECK_BOOL(_T("use_ssl"), _T("This option will enable SSL encryption on the NRPE data socket (this increases security somwhat.")) 
ITEM_MAP_TO(_T("basic_ini_bool_mapper")) 
OPTION(_T("section"), _T("NRPE")) 
OPTION(_T("key"), _T("use_ssl")) 
OPTION(_T("default"), _T("true")) 
OPTION(_T("true_value"), _T("1")) 
OPTION(_T("false_value"), _T("0")) 
ITEM_END()

PAGE_END()
ADVANCED_PAGE(_T("Access configuration")) 

ITEM_EDIT_OPTIONAL_LIST(_T("Allow connection from:"), _T("This is the hosts that will be allowed to poll performance data from the NRPE server.")) 
OPTION(_T("disabledCaption"), _T("Use global settings (defined previously)")) 
OPTION(_T("enabledCaption"), _T("Specify hosts for NRPE server")) 
OPTION(_T("listCaption"), _T("Add all IP addresses (not hosts) which should be able to connect:")) 
OPTION(_T("separator"), _T(",")) 
OPTION(_T("disabled"), _T("")) 
ITEM_MAP_TO(_T("basic_ini_text_mapper")) 
OPTION(_T("section"), _T("NRPE")) 
OPTION(_T("key"), _T("allowed_hosts")) 
OPTION(_T("default"), _T("")) 
ITEM_END()

PAGE_END()
MODULE_SETTINGS_END()
