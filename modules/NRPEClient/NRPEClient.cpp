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
#ifdef USE_BOOST
#include <program_options_ex.hpp>
#endif
#include <strEx.h>

NRPEClient gNRPEClient;


NRPEClient::NRPEClient() : buffer_length_(0), bInitSSL(false) {
}

NRPEClient::~NRPEClient() {
}

bool NRPEClient::loadModule() {
	buffer_length_ = NSCModuleHelper::getSettingsInt(NRPE_SECTION_TITLE, NRPE_SETTINGS_STRLEN, NRPE_SETTINGS_STRLEN_DEFAULT);

	std::list<std::wstring> commands = NSCModuleHelper::getSettingsSection(NRPE_CLIENT_HANDLER_SECTION_TITLE);
	NSC_DEBUG_MSG_STD(_T("humm..."));
	for (std::list<std::wstring>::const_iterator it = commands.begin(); it != commands.end(); ++it) {
		NSC_DEBUG_MSG_STD(*it);
		std::wstring s = NSCModuleHelper::getSettingsString(NRPE_CLIENT_HANDLER_SECTION_TITLE, (*it), _T(""));
		if (s.empty()) {
			NSC_LOG_ERROR_STD(_T("Invalid NRPE-client entry: ") + (*it));
		} else {
			addCommand((*it).c_str(), s);
		}
	}
	return true;
}
void NRPEClient::initSSL() {
	if (bInitSSL)
		return;
#ifdef USE_SSL
	simpleSSL::SSL_init();
#endif
	bInitSSL = true;
}

void NRPEClient::addCommand(strEx::blindstr key, std::wstring args) {
#ifndef USE_BOOST
	NSC_LOG_ERROR_STD(_T("Could not parse: ") + key.c_str() + _T(" boost not avalible!"));
#else
	try {
		boost::program_options::options_description desc = get_optionDesc();
		boost::program_options::positional_options_description p = get_optionsPositional();

		boost::program_options::variables_map vm;
		boost::program_options::store(
			basic_command_line_parser_ex<TCHAR>(args).options(desc).positional(p).run()
			, vm);
		boost::program_options::notify(vm); 
		nrpe_connection_data cd = get_ConectionData(vm);
		NSC_DEBUG_MSG_STD(_T("Added NRPE Client: ") + key.c_str() + _T(" = ") + cd.toString());
		commands[key] = cd;
	} catch (boost::program_options::validation_error &e) {
		NSC_LOG_ERROR_STD(_T("Could not parse: ") + key.c_str() + strEx::string_to_wstring(e.what()));
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Could not parse: ") + key.c_str());
	}
#endif
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
	command_list::const_iterator cit = commands.find(command);
	if (cit == commands.end())
		return NSCAPI::returnIgnored;

	std::wstring args = (*cit).second.arguments;
	if (NSCModuleHelper::getSettingsInt(NRPE_SECTION_TITLE, NRPE_SETTINGS_ALLOW_ARGUMENTS, NRPE_SETTINGS_ALLOW_ARGUMENTS_DEFAULT) == 1) {
		arrayBuffer::arrayList arr = arrayBuffer::arrayBuffer2list(argLen, char_args);
		arrayBuffer::arrayList::const_iterator cit2 = arr.begin();
		int i=1;

		for (;cit2!=arr.end();cit2++,i++) {
			if (NSCModuleHelper::getSettingsInt(NRPE_SECTION_TITLE, NRPE_SETTINGS_ALLOW_NASTY_META, NRPE_SETTINGS_ALLOW_NASTY_META_DEFAULT) == 0) {
				if ((*cit2).find_first_of(NASTY_METACHARS) != std::wstring::npos) {
					NSC_LOG_ERROR(_T("Request string contained illegal metachars!"));
					return NSCAPI::returnIgnored;
				}
			}
			strEx::replace(args, _T("$ARG") + strEx::itos(i) + _T("$"), (*cit2));
		}
	}

	NSC_DEBUG_MSG_STD(_T("Rewrote command arguments: ") + args);
	nrpe_result_data r = execute_nrpe_command((*cit).second, args);
	message = r.text;
	return r.result;
}

#ifdef USE_BOOST
boost::program_options::options_description NRPEClient::get_optionDesc() {
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
	return desc;
}
boost::program_options::positional_options_description NRPEClient::get_optionsPositional() {
	boost::program_options::positional_options_description p;
	p.add("arguments", -1);
	return p;
}
NRPEClient::nrpe_connection_data NRPEClient::get_ConectionData(boost::program_options::variables_map &vm) {
	nrpe_connection_data ret(buffer_length_);
	if (vm.count("host"))
		ret.host = vm["host"].as<std::wstring>();
	if (vm.count("port"))
		ret.port = vm["port"].as<int>();
	if (vm.count("timeout"))
		ret.timeout = vm["timeout"].as<int>();
	if (vm.count("buffer-length"))
		ret.buffer_length = vm["buffer-length"].as<int>();
	if (vm.count("command"))
		ret.command = vm["command"].as<std::wstring>();
	if (vm.count("arguments")) {
		std::vector<std::wstring> v = vm["arguments"].as<std::vector<std::wstring>>();
		for (std::vector<std::wstring>::const_iterator cit = v.begin(); cit != v.end(); ++cit) {
			if (!ret.arguments.empty())
				ret.arguments += _T("!");
			ret.arguments += *cit;
		}
	}
	if (vm.count("no-ssl"))
		ret.ssl = false;
	return ret;
}
#endif
int NRPEClient::commandLineExec(const TCHAR* command, const unsigned int argLen, TCHAR** args) {
#ifndef USE_BOOST
	NSC_LOG_ERROR_STD(_T("Could not execute ") + std::wstring(command) + _T(" boost not avalible!"));
	return NSCAPI::returnUNKNOWN;
#else
	try {
		boost::program_options::options_description desc = get_optionDesc();
		boost::program_options::positional_options_description p = get_optionsPositional();
		
		boost::program_options::variables_map vm;
		boost::program_options::store(
			basic_command_line_parser_ex<TCHAR>(command, argLen, args).options(desc).positional(p).run()
			, vm);
		boost::program_options::notify(vm);    

		if (vm.count("help")) {
			std::cout << desc << "\n";
			return 1;
		}

		NRPEClient::nrpe_connection_data command = get_ConectionData(vm);
		nrpe_result_data result = execute_nrpe_command(command, command.arguments);
		std::wcout << result.text << std::endl;
		return result.result;
	} catch (boost::program_options::validation_error &e) {
		std::cout << e.what() << std::endl;
	} catch (...) {
		std::cout << "Unknown exception parsing command line" << std::endl;
	}
	return NSCAPI::returnUNKNOWN;
#endif
}
NRPEClient::nrpe_result_data NRPEClient::execute_nrpe_command(nrpe_connection_data con, std::wstring arguments) {
	try {
		NRPEPacket packet;
		if (con.ssl) {
#ifdef USE_SSL
			packet = send_ssl(con.host, con.port, con.timeout, NRPEPacket::make_request(con.get_cli(arguments), con.buffer_length));
#else
			return nrpe_result_data(NSCAPI::returnUNKNOWN, _T("SSL support not available (compiled without USE_SSL)!"));
#endif
		} else
			packet = send_nossl(con.host, con.port, con.timeout, NRPEPacket::make_request(con.get_cli(arguments), con.buffer_length));
		return nrpe_result_data(packet.getResult(), packet.getPayload());
	} catch (NRPEPacket::NRPEPacketException &e) {
		return nrpe_result_data(NSCAPI::returnUNKNOWN, _T("NRPE Packet errro: ") + e.getMessage());
	} catch (simpleSocket::SocketException &e) {
		return nrpe_result_data(NSCAPI::returnUNKNOWN, _T("Socket error: ") + e.getMessage());
#ifdef USE_SSL
	} catch (simpleSSL::SSLException &e) {
		return nrpe_result_data(NSCAPI::returnUNKNOWN, _T("SSL Socket error: ") + e.getMessage());
#endif
	} catch (...) {
		return nrpe_result_data(NSCAPI::returnUNKNOWN, _T("Unknown error -- REPORT THIS!"));
	}
}
NRPEPacket NRPEClient::send_ssl(std::wstring host, int port, int timeout, NRPEPacket packet)
{
#ifndef USE_SSL
	return send_nossl(host, port, timeout, packet);
#else
	initSSL();
	simpleSSL::Socket socket(true);
	socket.connect(host, port);
	NSC_DEBUG_MSG_STD(_T(">>>length: ") + strEx::itos(packet.getBufferLength()));
	socket.sendAll(packet.getBuffer(), packet.getBufferLength());
	simpleSocket::DataBuffer buffer;
	socket.readAll(buffer, packet.getBufferLength());
	NSC_DEBUG_MSG_STD(_T("<<<length: ") + strEx::itos(buffer.getLength()));
	packet.readFrom(buffer.getBuffer(), buffer.getLength());
	return packet;
#endif
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





NSC_WRAP_DLL();
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
