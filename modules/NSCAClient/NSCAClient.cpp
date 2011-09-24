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

#include <utils.h>
#include <list>
#include <string>

#include <strEx.h>

#include "NSCAClient.h"
#include <nsca/nsca_enrypt.hpp>
#include <nsca/nsca_packet.hpp>
#include <nsca/nsca_socket.hpp>

#include <settings/client/settings_client.hpp>

namespace sh = nscapi::settings_helper;
namespace str = nscp::helpers;
namespace po = boost::program_options;

/**
 * Default c-tor
 * @return 
 */
NSCAAgent::NSCAAgent() {}
/**
 * Default d-tor
 * @return 
 */
NSCAAgent::~NSCAAgent() {}
/**
 * Load (initiate) module.
 * Start the background collector thread and let it run until unloadModule() is called.
 * @return true
 */
bool NSCAAgent::loadModule() {
	return false;
}
/*
DEFINE_SETTING_S(REPORT_MODE, NSCA_SERVER_SECTION, "report", "all");
DESCRIBE_SETTING(REPORT_MODE, "REPORT MODE", "What to report to the server (any of the following: all, critical, warning, unknown, ok)");
*/
bool NSCAAgent::loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode) {


	try {
		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias(_T("NSCA"), alias, _T("client"));

		settings.alias().add_path_to_settings()
			(_T("NSCA AGENT SECTION"), _T("Section for NSCA passive check module."))
			;

		settings.alias().add_key_to_settings()
			(_T("hostname"), sh::string_key(&hostname_),
			_T("HOSTNAME"), _T("The host name of this host if set to blank (default) the windows name of the computer will be used."))

			(_T("hostname cache"), sh::bool_key(&cacheNscaHost_),
			_T("CACHE HOSTNAME"), _T(""))

			(_T("delay"), sh::string_fun_key<std::wstring>(boost::bind(&NSCAAgent::set_delay, this, _1), _T("0")),
			_T("DELAY"), _T(""))

			(_T("payload length"), sh::uint_key(&payload_length_, 512),
			_T("PAYLOAD LENGTH"), _T("The password to use. Again has to be the same as the server or it wont work at all."))

			(_T("channel"), sh::wstring_key(&channel_, _T("NSCA")),
			_T("CHANNEL"), _T("The channel to listen to."))

			;

		settings.alias().add_path_to_settings(_T("server"))
			(_T("NSCA SERVER"), _T("Configure the NSCA server to report to."))
			;

		settings.alias().add_key_to_settings(_T("server"))
			(_T("host"), sh::wstring_key(&nscahost_),
			_T("NSCA HOST"), _T("The NSCA server to report results to."))

			(_T("port"), sh::uint_key(&nscaport_, 5666),
			_T("NSCA PORT"), _T("The NSCA server port"))

			(_T("encryption method"), sh::int_key(&encryption_method_),
			_T("ENCRYPTION METHOD"), _T("Number corresponding to the various encryption algorithms (see the wiki). Has to be the same as the server or it wont work at all."))

			(_T("password"), sh::string_key(&password_),
			_T("PASSWORD"), _T("The password to use. Again has to be the same as the server or it wont work at all."))

			(_T("timeout"), sh::uint_key(&timeout_, 30),
			_T("SOCKET TIMEOUT"), _T("Timeout when reading packets on incoming sockets. If the data has not arrived withint this time we will bail out."))
			;

		settings.register_all();
		settings.notify();

		NSC_DEBUG_MSG(_T("==> Registring listsner for: ") + channel_ + _T(" for ") + to_wstring(get_id()));
		get_core()->registerSubmissionListener(get_id(), channel_);

	} catch (nscapi::nscapi_exception &e) {
		NSC_LOG_ERROR_STD(_T("Failed to register command: ") + utf8::cvt<std::wstring>(e.what()));
		return false;
	} catch (std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Exception caught: ") + utf8::cvt<std::wstring>(e.what()));
		return false;
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to register command."));
		return false;
	}
	return true;
}
/**
 * Unload (terminate) module.
 * Attempt to stop the background processing thread.
 * @return true if successfully, false if not (if not things might be bad)
 */
bool NSCAAgent::unloadModule() {
	return true;
}
std::wstring NSCAAgent::getCryptos() {
	std::wstring ret = _T("{");
	for (int i=0;i<LAST_ENCRYPTION_ID;i++) {
		if (nsca::nsca_encrypt::hasEncryption(i)) {
			std::wstring name;
			try {
				nsca::nsca_encrypt::any_encryption *core = nsca::nsca_encrypt::get_encryption_core(i);
				if (core == NULL)
					name = _T("Broken<NULL>");
				else
					name = str::to_wstring(core->getName());
			} catch (nsca::nsca_encrypt::encryption_exception &e) {
				name = str::to_wstring(e.what());
			}
			if (ret.size() > 1)
				ret += _T(", ");
			ret += strEx::itos(i) + _T("=") + name;
		}
	}
	return ret + _T("}");
}

void NSCAAgent::add_local_options(po::options_description &desc, nscp_connection_data &command_data) {
	desc.add_options()
		("payload-length,l", po::value<unsigned int>(&command_data.payload_length)->zero_tokens()->default_value(512), "The payload length to use in the NSCA packet.")
		("encryption,e", po::value<std::string>(&command_data.encryption)->default_value("ASE"), "Encryption algorithm to use.")
		;
}

std::wstring NSCAAgent::setup(client::configuration &config, const std::wstring &command) {
	clp_handler_impl *handler = new clp_handler_impl(this, payload_length_, time_delta_);
	add_local_options(config.local, handler->local_data);
	std::wstring cmd = command;
	if (command.length() > 5 && command.substr(0,5) == _T("nsca_"))
		cmd = command.substr(5);
	if (command.length() > 5 && command.substr(command.length()-5) == _T("_nsca"))
		cmd = command.substr(0, command.length()-5);
	config.handler = client::configuration::handler_type(handler);
	return cmd;
}


NSCAPI::nagiosReturn NSCAAgent::handleCommand(const std::wstring &target, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &message, std::wstring &perf) {
	if (command == _T("submit_nsca")) {
		client::configuration config;
		std::wstring cmd = setup(config, command);
		std::list<std::string> errors = client::command_line_parser::simple_submit(config, cmd, arguments);
		BOOST_FOREACH(std::string p, errors) {
			NSC_LOG_ERROR_STD(utf8::cvt<std::wstring>(p));
		}
		return errors.empty()?NSCAPI::returnOK:NSCAPI::returnCRIT;
	}
	//return commands.exec_simple(target, command, arguments, message, perf);
	return NSCAPI::returnIgnored;
}


int NSCAAgent::commandLineExec(const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &result) {
	client::configuration config;
	std::wstring cmd = setup(config, command);
	return client::command_line_parser::commandLineExec(config, cmd, arguments, result);
}


//NSCAPI::nagiosReturn handleRAWNotification(const wchar_t* channel, std::string request, std::string &response);
NSCAPI::nagiosReturn NSCAAgent::handleRAWNotification(const wchar_t* channel, std::string request, std::string &response) {
	try {
		client::configuration config;
		setup(config, _T(""));
		if (!client::command_line_parser::relay_submit(config, request, response)) {
			NSC_LOG_ERROR_STD(_T("Failed to submit message..."));
			return NSCAPI::hasFailed;
		}
		return NSCAPI::isSuccess;
		/*
		boost::asio::io_service io_service;
		nsca::socket socket(io_service);
		socket.connect(nscahost_, nscaport_);
		nsca::packet packet(hostname_, payload_length_, time_delta_);
		packet.code = code;
		packet.service = utf8::cvt<std::string>(command);
		packet.result = utf8::cvt<std::string>(msg) + "|" + utf8::cvt<std::string>(perf);
		socket.recv_iv(password_, encryption_method_, boost::posix_time::seconds(timeout_));
		socket.send_nsca(packet, boost::posix_time::seconds(timeout_));
		return NSCAPI::isSuccess;
		*/
	} catch (nsca::nsca_encrypt::encryption_exception &e) {
		NSC_LOG_ERROR_STD(_T("Failed to encrypt data: ") + str::to_wstring(e.what()));
		return NSCAPI::hasFailed;
	} catch (std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Failed to send data: ") + utf8::cvt<std::wstring>(e.what()));
		return NSCAPI::hasFailed;
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to send data: UNKNOWN"));
		return NSCAPI::hasFailed;
	}
}
NSCAPI::nagiosReturn NSCAAgent::send(sender_information &data, const std::list<nsca::packet> packets) {
	try {
		boost::asio::io_service io_service;
		nsca::socket socket(io_service);
		socket.connect(data.host, data.port);
		if (!socket.recv_iv(data.password, data.get_encryption(), boost::posix_time::seconds(data.timeout))) {
			NSC_LOG_ERROR_STD(_T("Failed to read iv"));
			return NSCAPI::hasFailed;
		}
		NSC_LOG_ERROR_STD(_T("Got IV sending data: ") + strEx::itos(packets.size()));
		BOOST_FOREACH(const nsca::packet &packet, packets) {
			socket.send_nsca(packet, boost::posix_time::seconds(data.timeout));
		}
		return NSCAPI::isSuccess;
	} catch (nsca::nsca_encrypt::encryption_exception &e) {
		NSC_LOG_ERROR_STD(_T("Failed to encrypt data: ") + str::to_wstring(e.what()));
		return NSCAPI::hasFailed;
	} catch (std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Failed to send data: ") + utf8::cvt<std::wstring>(e.what()));
		return NSCAPI::hasFailed;
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to send data: UNKNOWN"));
		return NSCAPI::hasFailed;
	}
}


//////////////////////////////////////////////////////////////////////////
// Parser implementations
int NSCAAgent::clp_handler_impl::query(client::configuration::data_type data, std::string request, std::string &reply) {
	NSC_LOG_ERROR_STD(_T("NSCA does not support query patterns"));
	return NSCAPI::hasFailed;
}
std::list<std::string> NSCAAgent::clp_handler_impl::submit(client::configuration::data_type data, ::Plugin::Common_Header* header, const std::string &request, std::string &response) {
	std::list<std::string> result;
	try {
		NSC_DEBUG_MSG(_T(">>>") + str::to_wstring(strEx::format_buffer(request)));
		Plugin::SubmitRequestMessage message;
		message.ParseFromString(request);
		NSC_DEBUG_MSG(_T("<<<") + str::to_wstring(strEx::format_buffer(message.SerializeAsString())));
		std::list<nsca::packet> list;

		std::wstring command, msg, perf;
		nscapi::functions::destination_container recipient; // = data->host_default_recipient;
		//recipient.address = data->host; // "default host"
		// this should be set before calling...
		/*
		if (recipient.data.find("password") == recipient.data.end())
			recipient.data["password"] = "default password";
		if (recipient.data.find("encryption") == recipient.data.end())
			recipient.data["encryption"] = "default encryption";
		if (recipient.data.find("payload length") != recipient.data.end())
			local_data.payload_length = strEx::stoi(recipient.data["payload length"]);
		if (recipient.data.find("time offset") != recipient.data.end())
			local_data.time_delta = strEx::stol_as_time_sec(to_wstring(recipient.data["time offset"]), 1);
		*/
		nscapi::functions::parse_destination(*header, header->recipient_id(), recipient, true);
		nscapi::functions::destination_container source;
		//source.host = hostname_;
		nscapi::functions::parse_destination(*header, header->source_id(), source);

		for (int i=0;i < message.payload_size(); ++i) {
			nsca::packet packet(source.host, local_data.payload_length, local_data.time_delta);
			packet.code = nscapi::functions::parse_simple_submit_request_payload(message.payload(i), command, msg, perf);
			packet.service = utf8::cvt<std::string>(command);
			packet.result = utf8::cvt<std::string>(msg) + "|" + utf8::cvt<std::string>(perf);
			list.push_back(packet);
		}

		sender_information si(recipient);
		if (instance->send(si, list) != NSCAPI::isSuccess) {
			NSC_LOG_ERROR_STD(_T("Failed to send NSCA message"));
			result.push_back("Invalid payload");
		}
		return result;
	} catch (std::exception &e) {
		result.push_back(e.what());
		NSC_LOG_ERROR_STD(_T("Exception: ") + utf8::cvt<std::wstring>(e.what()));
		return result;
	}
}
int NSCAAgent::clp_handler_impl::exec(client::configuration::data_type data, std::string request, std::string &reply) {
	NSC_LOG_ERROR_STD(_T("NSCA does not support exec patterns"));
	return NSCAPI::hasFailed;
}



NSC_WRAP_DLL();
NSC_WRAPPERS_MAIN_DEF(NSCAAgent);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_IGNORE_CMD_DEF();
NSC_WRAPPERS_HANDLE_NOTIFICATION_DEF();
