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

#include <boost/asio.hpp>

#include <strEx.h>

#include "SyslogClient.h"

#include <settings/client/settings_client.hpp>

namespace sh = nscapi::settings_helper;
namespace str = nscp::helpers;
namespace po = boost::program_options;
namespace ip = boost::asio::ip;

/**
 * Default c-tor
 * @return 
 */
SyslogClient::SyslogClient() {}
/**
 * Default d-tor
 * @return 
 */
SyslogClient::~SyslogClient() {}
/**
 * Load (initiate) module.
 * Start the background collector thread and let it run until unloadModule() is called.
 * @return true
 */
bool SyslogClient::loadModule() {
	return false;
}
bool SyslogClient::loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode) {

	facilities["kernel"] = 0;
	facilities["user"] = 1;
	facilities["mail"] = 2;
	facilities["system"] = 3;
	facilities["security"] = 4;
	facilities["internal"] = 5;
	facilities["printer"] = 6;
	facilities["news"] = 7;
	facilities["UUCP"] = 8;
	facilities["clock"] = 9;
	facilities["authorization"] = 10;
	facilities["FTP"] = 11;
	facilities["NTP"] = 12;
	facilities["audit"] = 13;
	facilities["alert"] = 14;
	facilities["clock"] = 15;
	facilities["local0"] = 16;
	facilities["local1"] = 17;
	facilities["local2"] = 18;
	facilities["local3"] = 19;
	facilities["local4"] = 20;
	facilities["local5"] = 21;
	facilities["local6"] = 22;
	facilities["local7"] = 23;
	severities["emergency"] = 0;
	severities["alert"] = 1;
	severities["critical"] = 2;
	severities["error"] = 3;
	severities["warning"] = 4;
	severities["notice"] = 5;
	severities["informational"] = 6;
	severities["debug"] = 7;

	try {
		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias(_T("syslog"), alias, _T("client"));
		target_path = settings.alias().get_settings_path(_T("targets"));

		settings.alias().add_path_to_settings()
			(_T("SYSLOG CLIENT SECTION"), _T("Section for SYSLOG passive check module."))

			(_T("targets"), sh::fun_values_path(boost::bind(&SyslogClient::add_target, this, _1, _2)), 
			_T("REMOTE TARGET DEFINITIONS"), _T(""))

			;

		settings.alias().add_key_to_settings()
			(_T("hostname"), sh::string_key(&hostname_),
			_T("HOSTNAME"), _T("The host name of this host if set to blank (default) the windows name of the computer will be used."))

			(_T("channel"), sh::wstring_key(&channel_, _T("syslog")),
			_T("CHANNEL"), _T("The channel to listen to."))

			;

		settings.register_all();
		settings.notify();

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
bool SyslogClient::unloadModule() {
	return true;
}

void SyslogClient::add_target(std::wstring key, std::wstring arg) {
	try {
		targets.add(get_settings_proxy(), target_path , key, arg);
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to add target: ") + key);
	}
}
void SyslogClient::add_local_options(po::options_description &desc, connection_data &command_data) {
	/*
	desc.add_options()
		("payload-length,l", po::value<unsigned int>(&command_data.payload_length)->zero_tokens()->default_value(512), "The payload length to use in the NSCA packet.")
		("encryption,e", po::value<std::string>(&command_data.encryption)->default_value("ASE"), "Encryption algorithm to use.")
		;
		*/
}

std::wstring SyslogClient::setup(client::configuration &config, const std::wstring &command) {
	clp_handler_impl *handler = new clp_handler_impl(this);
	add_local_options(config.local, handler->local_data);
	std::wstring cmd = client::command_line_parser::parse_command(command, _T("syslog"));
	config.handler = client::configuration::handler_type(handler);
	return cmd;
}

NSCAPI::nagiosReturn SyslogClient::handleCommand(const std::wstring &target, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &message, std::wstring &perf) {
	if (command == _T("submit_syslog")) {
		client::configuration config;
		std::wstring cmd = setup(config, command);
		std::list<std::string> errors = client::command_line_parser::simple_submit(config, cmd, arguments);
		BOOST_FOREACH(std::string p, errors) {
			NSC_LOG_ERROR_STD(utf8::cvt<std::wstring>(p));
		}
		return errors.empty()?NSCAPI::returnOK:NSCAPI::returnCRIT;
	}
	return NSCAPI::returnIgnored;
}


int SyslogClient::commandLineExec(const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &result) {
	client::configuration config;
	std::wstring cmd = setup(config, command);
	return client::command_line_parser::commandLineExec(config, cmd, arguments, result);
}

NSCAPI::nagiosReturn SyslogClient::handleRAWNotification(const wchar_t* channel, std::string request, std::string &response) {
	try {
		client::configuration config;
		net::wurl url;
		url.protocol = _T("syslog");
		nscapi::target_handler::optarget target = targets.find_target(_T("default"));
		if (target) {
			url.host = target->host;
			try {
				url.port = strEx::stoi(target->options[_T("port")]);
			} catch (...) {}
			config.data->recipient.data["message template"] = utf8::cvt<std::string>(target->options[_T("message template")]);
			config.data->recipient.data["tag template"] = utf8::cvt<std::string>(target->options[_T("tag template")]);
			config.data->recipient.data["severity"] = utf8::cvt<std::string>(target->options[_T("severity")]);
			config.data->recipient.data["facility"] = utf8::cvt<std::string>(target->options[_T("facility")]);
		}
		config.data->recipient.id = "default";
		config.data->recipient.address = utf8::cvt<std::string>(url.to_string());
		config.data->host_self.id = "self";
		config.data->host_self.host = hostname_;

		setup(config, _T(""));
		if (!client::command_line_parser::relay_submit(config, request, response)) {
			NSC_LOG_ERROR_STD(_T("Failed to submit message..."));
			return NSCAPI::hasFailed;
		}
		return NSCAPI::isSuccess;
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
int SyslogClient::clp_handler_impl::query(client::configuration::data_type data, std::string request, std::string &reply) {
	NSC_LOG_ERROR_STD(_T("SYSLOG does not support query patterns"));
	return NSCAPI::hasFailed;
}
int SyslogClient::clp_handler_impl::exec(client::configuration::data_type data, std::string request, std::string &reply) {
	NSC_LOG_ERROR_STD(_T("SYSLOG does not support exec patterns"));
	return NSCAPI::hasFailed;
}
std::string	SyslogClient::parse_priority(std::string severity, std::string facility) {
	syslog_map::const_iterator cit1 = facilities.find(facility);
	if (cit1 == facilities.end()) {
		NSC_LOG_ERROR("Undefined facility: " + facility);
		return "<0>";
	}
	syslog_map::const_iterator cit2 = severities.find(severity);
	if (cit2 == severities.end()) {
		NSC_LOG_ERROR("Undefined severity: " + severity);
		return "<0>";
	}
	std::stringstream ss;
	ss << '<' << (cit1->second*8+cit2->second) << '>';
	return ss.str();
}
std::string render_message(const ::Plugin::QueryResponseMessage::Response& payload, std::string priority, std::string tag_syntax, std::string message_syntax) {
	std::string date = "Nov 10 00:12:00";
	std::string tag = tag_syntax;
	std::string message = message_syntax;
	return priority + date + " " + tag + " " + message;

}

int SyslogClient::clp_handler_impl::submit(client::configuration::data_type data, ::Plugin::Common_Header* header, const std::string &request, std::string &response) {
	try {

		Plugin::SubmitRequestMessage message;
		message.ParseFromString(request);

		std::wstring alias, command, msg, perf;
		nscapi::functions::destination_container recipient;
		nscapi::functions::parse_destination(*header, header->recipient_id(), recipient, true);
		nscapi::functions::destination_container source;
		nscapi::functions::parse_destination(*header, header->source_id(), source);

		std::string severity = recipient.data["severity"];
		std::string facility = recipient.data["facility"];
		std::string tag_syntax = recipient.data["tag template"];
		std::string message_syntax = recipient.data["message template"];
		net::url url = net::parse(recipient.address, 514);

		boost::asio::io_service io_service;
		ip::udp::resolver resolver(io_service);
		ip::udp::resolver::query query(ip::udp::v4(), url.host, url.get_port());
		ip::udp::endpoint receiver_endpoint = *resolver.resolve(query);

		ip::udp::socket socket(io_service);
		socket.open(ip::udp::v4());

		for (int i=0;i < message.payload_size(); ++i) {
			std::string msg = render_message(message.payload(i), instance->parse_priority(severity, facility), tag_syntax, message_syntax);
			socket.send_to(boost::asio::buffer(msg), receiver_endpoint);
		}

		nscapi::functions::create_simple_submit_response(_T(""), _T(""), NSCAPI::isSuccess, _T(""), response);
		return NSCAPI::isSuccess;
	} catch (std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Exception: ") + utf8::cvt<std::wstring>(e.what()));
		nscapi::functions::create_simple_submit_response(_T(""), _T(""), NSCAPI::hasFailed, utf8::cvt<std::wstring>(e.what()), response);
		return NSCAPI::hasFailed;
	}
}

NSC_WRAP_DLL();
NSC_WRAPPERS_MAIN_DEF(SyslogClient);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_IGNORE_CMD_DEF();
NSC_WRAPPERS_HANDLE_NOTIFICATION_DEF();
