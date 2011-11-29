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

namespace sh = nscapi::settings_helper;
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

	std::wstring severity, facility, tag_syntax, message_syntax;
	std::wstring ok_severity, warn_severity, crit_severity, unknown_severity;
	try {
		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias(_T("syslog"), alias, _T("client"));
		target_path = settings.alias().get_settings_path(_T("targets"));

		settings.alias().add_path_to_settings()
			(_T("SYSLOG CLIENT SECTION"), _T("Section for SYSLOG passive check module."))
			(_T("handlers"), sh::fun_values_path(boost::bind(&SyslogClient::add_command, this, _1, _2)), 
			_T("CLIENT HANDLER SECTION"), _T(""))

			(_T("targets"), sh::fun_values_path(boost::bind(&SyslogClient::add_target, this, _1, _2)), 
			_T("REMOTE TARGET DEFINITIONS"), _T(""))
			;

		settings.alias().add_key_to_settings()
			(_T("hostname"), sh::string_key(&hostname_),
			_T("HOSTNAME"), _T("The host name of this host if set to blank (default) the windows name of the computer will be used."))

			(_T("channel"), sh::wstring_key(&channel_, _T("syslog")),
			_T("CHANNEL"), _T("The channel to listen to."))

			;

		settings.alias().add_key_to_settings(_T("targets/default"))

			(_T("severity"), sh::wpath_key(&severity, _T("error")),
			_T("SSL CERTIFICATE"), _T(""))

			(_T("facility"), sh::wpath_key(&facility, _T("kernel")),
			_T("SSL CERTIFICATE"), _T(""))

			(_T("tag_syntax"), sh::wpath_key(&tag_syntax, _T("NSCP")),
			_T("SSL CERTIFICATE"), _T(""))

			(_T("message_syntax"), sh::wpath_key(&message_syntax, _T("%message%")),
			_T("SSL CERTIFICATE"), _T(""))

			(_T("ok severity"), sh::wpath_key(&ok_severity, _T("informational")),
			_T("SSL CERTIFICATE"), _T(""))

			(_T("warning severity"), sh::wpath_key(&warn_severity, _T("warning")),
			_T("SSL CERTIFICATE"), _T(""))

			(_T("critical severity"), sh::wpath_key(&crit_severity, _T("critical")),
			_T("SSL CERTIFICATE"), _T(""))

			(_T("unknown severity"), sh::wpath_key(&unknown_severity, _T("emergency")),
			_T("SSL CERTIFICATE"), _T(""))

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
			if (!t->has_option("severity"))
				t->options[_T("severity")] = severity;
			if (!t->has_option("facility"))
				t->options[_T("facility")] = facility;
			if (!t->has_option("tag syntax"))
				t->options[_T("tag syntax")] = tag_syntax;
			if (!t->has_option("message syntax"))
				t->options[_T("message syntax")] = message_syntax;
			if (!t->has_option("ok severity"))
				t->options[_T("ok severity")] = ok_severity;
			if (!t->has_option("warning severity"))
				t->options[_T("warning severity")] = warn_severity;
			if (!t->has_option("critical severity"))
				t->options[_T("critical severity")] = crit_severity;
			if (!t->has_option("unknown severity"))
				t->options[_T("unknown severity")] = unknown_severity;
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


//////////////////////////////////////////////////////////////////////////
// Settings helpers
//

void SyslogClient::add_target(std::wstring key, std::wstring arg) {
	try {
		targets.add(get_settings_proxy(), target_path , key, arg);
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to add target: ") + key);
	}
}

void SyslogClient::add_command(std::wstring name, std::wstring args) {
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
bool SyslogClient::unloadModule() {
	return true;
}

NSCAPI::nagiosReturn SyslogClient::handleCommand(const std::wstring &target, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &message, std::wstring &perf) {
	std::wstring cmd = client::command_line_parser::parse_command(command, _T("nsca"));

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

int SyslogClient::commandLineExec(const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &result) {
	std::wstring cmd = client::command_line_parser::parse_command(command, _T("nsca"));
	if (!client::command_line_parser::is_command(cmd))
		return NSCAPI::returnIgnored;

	client::configuration config;
	setup(config);
	return client::command_line_parser::commandLineExec(config, cmd, arguments, result);
}

NSCAPI::nagiosReturn SyslogClient::handleRAWNotification(const wchar_t* channel, std::string request, std::string &response) {
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

void SyslogClient::add_local_options(po::options_description &desc, client::configuration::data_type data) {
	desc.add_options()
		("severity,s", po::value<std::string>()->notifier(boost::bind(&nscapi::functions::destination_container::set_string_data, &data->recipient, "severity", _1)), 
		"Severity of error message")

		("unknown-severity", po::value<std::string>()->notifier(boost::bind(&nscapi::functions::destination_container::set_string_data, &data->recipient, "unknown_severity", _1)), 
		"Severity of error message")

		("ok-severity", po::value<std::string>()->notifier(boost::bind(&nscapi::functions::destination_container::set_string_data, &data->recipient, "ok_severity", _1)), 
		"Severity of error message")

		("warning-severity", po::value<std::string>()->notifier(boost::bind(&nscapi::functions::destination_container::set_string_data, &data->recipient, "warning_severity", _1)), 
		"Severity of error message")

		("critical-severity", po::value<std::string>()->notifier(boost::bind(&nscapi::functions::destination_container::set_string_data, &data->recipient, "critical_severity", _1)), 
		"Severity of error message")

		("facility,f", po::value<std::string>()->notifier(boost::bind(&nscapi::functions::destination_container::set_string_data, &data->recipient, "facility", _1)), 
		"Facility of error message")

		("tag template", po::value<std::string>()->notifier(boost::bind(&nscapi::functions::destination_container::set_string_data, &data->recipient, "tag template", _1)), 
		"Tag template (TODO)")

		("message template", po::value<std::string>()->notifier(boost::bind(&nscapi::functions::destination_container::set_string_data, &data->recipient, "message template", _1)), 
		"Message template (TODO)")
		;
}

void SyslogClient::setup(client::configuration &config) {
	boost::shared_ptr<clp_handler_impl> handler = boost::shared_ptr<clp_handler_impl>(new clp_handler_impl(this));
	add_local_options(config.local, config.data);

	net::wurl url;
	url.protocol = _T("syslog");
	url.port = 514;
	nscapi::target_handler::optarget opt = targets.find_target(_T("default"));
	if (opt) {
		nscapi::target_handler::target t = *opt;
		url.host = t.host;
		if (t.has_option("port")) {
			try {
				url.port = strEx::stoi(t.options[_T("port")]);
			} catch (...) {}
		}
		std::string keys[] = {"message template", "tag template", "severity", "ok_severity", "warning_severity", "critical_severity", "unknown_severity", "facility"};
		BOOST_FOREACH(std::string s, keys) {
			config.data->recipient.data[s] = utf8::cvt<std::string>(t.options[utf8::cvt<std::wstring>(s)]);
		}
	}
	config.data->recipient.id = "default";
	config.data->recipient.address = utf8::cvt<std::string>(url.to_string());
	config.data->host_self.id = "self";
	config.data->host_self.host = hostname_;

	config.target_lookup = handler;
	config.handler = handler;
}

SyslogClient::connection_data SyslogClient::parse_header(const ::Plugin::Common_Header &header) {
	nscapi::functions::destination_container recipient;
	nscapi::functions::parse_destination(header, header.recipient_id(), recipient, true);
	return connection_data(recipient);
}

//////////////////////////////////////////////////////////////////////////
// Parser implementations
//

int SyslogClient::clp_handler_impl::query(client::configuration::data_type data, ::Plugin::Common_Header* header, const std::string &request, std::string &reply) {
	NSC_LOG_ERROR_STD(_T("SYSLOG does not support query patterns"));
	return NSCAPI::hasFailed;
}

int SyslogClient::clp_handler_impl::submit(client::configuration::data_type data, ::Plugin::Common_Header* header, const std::string &request, std::string &reply) {
	std::wstring channel;
	try {
		Plugin::SubmitRequestMessage message;
		message.ParseFromString(request);
		connection_data con = parse_header(*header);
		channel = utf8::cvt<std::wstring>(message.channel());

		//TODO: Map seveity!

		std::list<std::string> messages;
		for (int i=0;i < message.payload_size(); ++i) {
			const ::Plugin::QueryResponseMessage::Response& payload = message.payload(i);
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
		boost::tuple<int,std::wstring> ret = instance->send(con, messages);
		nscapi::functions::create_simple_submit_response(channel, _T("UNKNOWN"), ret.get<0>(), _T("Message submitted successfully: ") + ret.get<1>(), reply);
		return NSCAPI::isSuccess;
	} catch (std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Exception: ") + utf8::to_unicode(e.what()));
		nscapi::functions::create_simple_submit_response(channel, _T("UNKNOWN"), NSCAPI::returnUNKNOWN, utf8::to_unicode(e.what()), reply);
		return NSCAPI::hasFailed;
	}
}

int SyslogClient::clp_handler_impl::exec(client::configuration::data_type data, ::Plugin::Common_Header* header, const std::string &request, std::string &reply) {
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

//////////////////////////////////////////////////////////////////////////
// Protocol implementations
//

boost::tuple<int,std::wstring> SyslogClient::send(connection_data con, std::list<std::string> messages) {
	try {
		NSC_DEBUG_MSG_STD(_T("NRPE Connection details: ") + con.to_wstring());

		boost::asio::io_service io_service;
		ip::udp::resolver resolver(io_service);
		ip::udp::resolver::query query(ip::udp::v4(), con.host, con.port);
		ip::udp::endpoint receiver_endpoint = *resolver.resolve(query);

		ip::udp::socket socket(io_service);
		socket.open(ip::udp::v4());

		BOOST_FOREACH(const std::string msg, messages) {
			NSC_DEBUG_MSG_STD(_T("Sending data: ") + utf8::cvt<std::wstring>(msg));
			socket.send_to(boost::asio::buffer(msg), receiver_endpoint);
		}
		return boost::make_tuple(NSCAPI::returnOK, _T("OK"));
	} catch (std::runtime_error &e) {
		NSC_LOG_ERROR_STD(_T("Socket error: ") + utf8::to_unicode(e.what()));
		return boost::make_tuple(NSCAPI::returnUNKNOWN, _T("Socket error: ") + utf8::to_unicode(e.what()));
	} catch (std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Error: ") + utf8::to_unicode(e.what()));
		return boost::make_tuple(NSCAPI::returnUNKNOWN, _T("Error: ") + utf8::to_unicode(e.what()));
	} catch (...) {
		return boost::make_tuple(NSCAPI::returnUNKNOWN, _T("Unknown error -- REPORT THIS!"));
	}
}


NSC_WRAP_DLL();
NSC_WRAPPERS_MAIN_DEF(SyslogClient);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF();
NSC_WRAPPERS_CLI_DEF();
NSC_WRAPPERS_HANDLE_NOTIFICATION_DEF();

