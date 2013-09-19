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
#include "SyslogClient.h"

#include <utils.h>
#include <list>
#include <string>

#include <boost/asio.hpp>

#include <strEx.h>

#include <settings/client/settings_client.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_plugin_interface.hpp>

namespace sh = nscapi::settings_helper;
namespace ip = boost::asio::ip;

const std::string command_prefix("syslog");
const std::string default_command("submit");
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

bool SyslogClient::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {

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
		settings.set_alias("syslog", alias, "client");
		target_path = settings.alias().get_settings_path("targets");

		settings.alias().add_path_to_settings()
			("SYSLOG CLIENT SECTION", "Section for SYSLOG passive check module.")
			("handlers", sh::fun_values_path(boost::bind(&SyslogClient::add_command, this, _1, _2)), 
			"CLIENT HANDLER SECTION", "")

			("targets", sh::fun_values_path(boost::bind(&SyslogClient::add_target, this, _1, _2)), 
			"REMOTE TARGET DEFINITIONS", "")
			;

		settings.alias().add_key_to_settings()
			("hostname", sh::string_key(&hostname_),
			"HOSTNAME", "The host name of this host if set to blank (default) the windows name of the computer will be used.")

			("channel", sh::string_key(&channel_, "syslog"),
			"CHANNEL", "The channel to listen to.")

			;
		settings.register_all();
		settings.notify();

		targets.add_samples(get_settings_proxy(), target_path);
		targets.add_missing(get_settings_proxy(), target_path, "default", "", true);
		nscapi::core_helper::core_proxy core(get_core(), get_id());
		core.register_channel(channel_);
	} catch (nscapi::nscapi_exception &e) {
		NSC_LOG_ERROR_EXR("load", e);
		return false;
	} catch (std::exception &e) {
		NSC_LOG_ERROR_EXR("load", e);
		return false;
	} catch (...) {
		NSC_LOG_ERROR_EX("load");
		return false;
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
// Settings helpers
//

void SyslogClient::add_target(std::string key, std::string arg) {
	try {
		targets.add(get_settings_proxy(), target_path , key, arg);
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to add target: " + key, e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to add target: " + key);
	}
}

void SyslogClient::add_command(std::string name, std::string args) {
	try {
		nscapi::core_helper::core_proxy core(get_core(), get_id());
		std::string key = commands.add_command(name, args);
		if (!key.empty())
			core.register_command(key.c_str(), "NRPE relay for: " + name);
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
bool SyslogClient::unloadModule() {
	return true;
}

void SyslogClient::query_fallback(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response, const Plugin::QueryRequestMessage &request_message) {
	client::configuration config(command_prefix);
	setup(config, request_message.header());
	commands.parse_query(command_prefix, default_command, request.command(), config, request, *response, request_message);
}

bool SyslogClient::commandLineExec(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response, const Plugin::ExecuteRequestMessage &request_message) {
	client::configuration config(command_prefix);
	setup(config, request_message.header());
	return commands.parse_exec(command_prefix, default_command, request.command(), config, request, *response, request_message);
}

void SyslogClient::handleNotification(const std::string &channel, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage *response_message) {
	client::configuration config(command_prefix);
	setup(config, request_message.header());
	commands.forward_submit(config, request_message, *response_message);
}

//////////////////////////////////////////////////////////////////////////
// Parser setup/Helpers
//

void SyslogClient::add_local_options(po::options_description &desc, client::configuration::data_type data) {
	desc.add_options()
		("severity,s", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->recipient, "severity", _1)), 
		"Severity of error message")

		("unknown-severity", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->recipient, "unknown_severity", _1)), 
		"Severity of error message")

		("ok-severity", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->recipient, "ok_severity", _1)), 
		"Severity of error message")

		("warning-severity", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->recipient, "warning_severity", _1)), 
		"Severity of error message")

		("critical-severity", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->recipient, "critical_severity", _1)), 
		"Severity of error message")

		("facility,f", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->recipient, "facility", _1)), 
		"Facility of error message")

		("tag template", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->recipient, "tag template", _1)), 
		"Tag template (TODO)")

		("message template", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->recipient, "message template", _1)), 
		"Message template (TODO)")
		;
}

void SyslogClient::setup(client::configuration &config, const ::Plugin::Common_Header& header) {
	boost::shared_ptr<clp_handler_impl> handler = boost::shared_ptr<clp_handler_impl>(new clp_handler_impl(this));
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

SyslogClient::connection_data SyslogClient::parse_header(const ::Plugin::Common_Header &header, client::configuration::data_type data) {
	nscapi::protobuf::functions::destination_container recipient;
	nscapi::protobuf::functions::parse_destination(header, header.recipient_id(), recipient, true);
	return connection_data(recipient, data->recipient);
}

//////////////////////////////////////////////////////////////////////////
// Parser implementations
//

int SyslogClient::clp_handler_impl::query(client::configuration::data_type data, const Plugin::QueryRequestMessage &request_message, Plugin::QueryResponseMessage &response_message) {
	NSC_LOG_ERROR_STD("SYSLOG does not support query patterns");
	nscapi::protobuf::functions::set_response_bad(*response_message.add_payload(), "SYSLOG does not support query patterns");
	return NSCAPI::isSuccess;
}

int SyslogClient::clp_handler_impl::submit(client::configuration::data_type data, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage &response_message) {
	const ::Plugin::Common_Header& request_header = request_message.header();
	connection_data con = parse_header(request_header, data);

	nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_header);

	//TODO: Map seveity!

	std::list<std::string> messages;
	for (int i=0;i < request_message.payload_size(); ++i) {
		const ::Plugin::QueryResponseMessage::Response& payload = request_message.payload(i);
		std::string date = "Nov 10 00:12:00"; // TODO is this actually used?
		std::string tag = con.tag_syntax;
		std::string message = con.message_syntax;
		strEx::replace(message, "%message%", payload.message());
		strEx::replace(tag, "%message%", payload.message());

		std::string severity = con.severity;
		if (payload.result() == ::Plugin::Common_ResultCode_OK)
			severity = con.ok_severity;
		if (payload.result() == ::Plugin::Common_ResultCode_WARNING)
			severity = con.warn_severity;
		if (payload.result() == ::Plugin::Common_ResultCode_CRITCAL)
			severity = con.crit_severity;
		if (payload.result() == ::Plugin::Common_ResultCode_UNKNOWN)
			severity = con.unknown_severity;

		messages.push_back(instance->parse_priority(severity, con.facility) + date + " " + tag + " " + message);
	}
	boost::tuple<int,std::string> ret = instance->send(con, messages);
	nscapi::protobuf::functions::append_simple_submit_response_payload(response_message.add_payload(), "UNKNOWN", ret.get<0>(), ret.get<1>());
	return NSCAPI::isSuccess;
}

int SyslogClient::clp_handler_impl::exec(client::configuration::data_type data, const Plugin::ExecuteRequestMessage &request_message, Plugin::ExecuteResponseMessage &response_message) {
	NSC_LOG_ERROR_STD("SYSLOG does not support exec patterns");
	nscapi::protobuf::functions::set_response_bad(*response_message.add_payload(), "SYSLOG does not support exec patterns");
	return NSCAPI::isSuccess;
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

//////////////////////////////////////////////////////////////////////////
// Protocol implementations
//

boost::tuple<int,std::string> SyslogClient::send(connection_data con, std::list<std::string> messages) {
	try {
		NSC_DEBUG_MSG_STD("Connection details: " + con.to_string());

		boost::asio::io_service io_service;
		ip::udp::resolver resolver(io_service);
		ip::udp::resolver::query query(ip::udp::v4(), con.host, strEx::s::xtos(con.port));
		ip::udp::endpoint receiver_endpoint = *resolver.resolve(query);

		ip::udp::socket socket(io_service);
		socket.open(ip::udp::v4());

		BOOST_FOREACH(const std::string msg, messages) {
			NSC_DEBUG_MSG_STD("Sending data: " + msg);
			socket.send_to(boost::asio::buffer(msg), receiver_endpoint);
		}
		return boost::make_tuple(NSCAPI::returnOK, "OK");
	} catch (std::runtime_error &e) {
		NSC_LOG_ERROR_EXR("Failed to send", e);
		return boost::make_tuple(NSCAPI::returnUNKNOWN, "Socket error: " + utf8::utf8_from_native(e.what()));
	} catch (std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to send", e);
		return boost::make_tuple(NSCAPI::returnUNKNOWN, "Error: " + utf8::utf8_from_native(e.what()));
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to send");
		return boost::make_tuple(NSCAPI::returnUNKNOWN, "Unknown error -- REPORT THIS!");
	}
}
