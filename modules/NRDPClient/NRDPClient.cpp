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
#include "NRDPClient.h"

#include <utils.h>
#include <list>
#include <string>

#include <strEx.h>
#include <http/client.hpp>

#include <settings/client/settings_client.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_core_helper.hpp>

namespace sh = nscapi::settings_helper;

const std::string command_prefix("nrdp");
const std::string default_command("query");
/**
 * Default c-tor
 * @return 
 */
NRDPClient::NRDPClient() {}

/**
 * Default d-tor
 * @return 
 */
NRDPClient::~NRDPClient() {}


bool NRDPClient::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {

	std::wstring template_string, sender, recipient;
	try {
		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias("NRDP", alias, "client");
		target_path = settings.alias().get_settings_path("targets");

		settings.alias().add_path_to_settings()
			("SMTP CLIENT SECTION", "Section for SMTP passive check module.")
			("handlers", sh::fun_values_path(boost::bind(&NRDPClient::add_command, this, _1, _2)), 
			"CLIENT HANDLER SECTION", "")

			("targets", sh::fun_values_path(boost::bind(&NRDPClient::add_target, this, _1, _2)), 
			"REMOTE TARGET DEFINITIONS", "")
			;

		settings.alias().add_key_to_settings()
			("hostname", sh::string_key(&hostname_, "auto"),
			"HOSTNAME", "The host name of this host if set to blank (default) the windows name of the computer will be used.")

			("channel", sh::string_key(&channel_, "NRDP"),
			"CHANNEL", "The channel to listen to.")

			;

		settings.register_all();
		settings.notify();

		targets.add_missing(get_settings_proxy(), target_path, "default", "", true);


		nscapi::core_helper::core_proxy core(get_core(), get_id());
		core.register_channel(channel_);

		if (hostname_ == "auto") {
			hostname_ = boost::asio::ip::host_name();
		} else if (hostname_ == "auto-lc") {
			hostname_ = boost::asio::ip::host_name();
			std::transform(hostname_.begin(), hostname_.end(), hostname_.begin(), ::tolower);
		} else {
			std::pair<std::string,std::string> dn = strEx::split<std::string>(boost::asio::ip::host_name(), ".");

			try {
				boost::asio::io_service svc;
				boost::asio::ip::tcp::resolver resolver (svc);
				boost::asio::ip::tcp::resolver::query query (boost::asio::ip::host_name(), "");
				boost::asio::ip::tcp::resolver::iterator iter = resolver.resolve (query), end;

				std::string s;
				while (iter != end) {
					s += iter->host_name();
					s += " - ";
					s += iter->endpoint().address().to_string();
					iter++;
				}
			} catch (const std::exception& e) {
				NSC_LOG_ERROR_EXR("Failed to resolve: ", e);
			}


			strEx::replace(hostname_, "${host}", dn.first);
			strEx::replace(hostname_, "${domain}", dn.second);
		}
	} catch (nscapi::nscapi_exception &e) {
		NSC_LOG_ERROR_EXR("NSClient API exception: ", e);
		return false;
	} catch (std::exception &e) {
		NSC_LOG_ERROR_EXR("NSClient API exception: ", e);
		return false;
	} catch (...) {
		NSC_LOG_ERROR_EX("NSClient API exception: ");
		return false;
	}
	return true;
}

std::string get_command(std::string alias, std::string command = "") {
	if (!alias.empty())
		return alias; 
	if (!command.empty())
		return command; 
	return "TODO";
}

//////////////////////////////////////////////////////////////////////////
// Settings helpers
//

void NRDPClient::add_target(std::string key, std::string arg) {
	try {
		targets.add(get_settings_proxy(), target_path , key, arg);
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to add target: " + key, e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to add target: " + key);
	}
}

void NRDPClient::add_command(std::string name, std::string args) {
	nscapi::core_helper::core_proxy core(get_core(), get_id());
	try {
		std::string key = commands.add_command(name, args);
		if (!key.empty())
			core.register_command(key, "NRPE relay for: " + name);
	} catch (boost::program_options::validation_error &e) {
		NSC_LOG_ERROR_EXR("Failed to add command: " + name, e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to add target: " + name);
	}
}

/**
 * Unload (terminate) module.
 * Attempt to stop the background processing thread.
 * @return true if successfully, false if not (if not things might be bad)
 */
bool NRDPClient::unloadModule() {
	return true;
}

void NRDPClient::query_fallback(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response, const Plugin::QueryRequestMessage &request_message) {
	client::configuration config(command_prefix);
	setup(config, request_message.header());
	commands.parse_query(command_prefix, default_command, request.command(), config, request, *response, request_message);
}

bool NRDPClient::commandLineExec(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response, const Plugin::ExecuteRequestMessage &request_message) {
	client::configuration config(command_prefix);
	setup(config, request_message.header());
	return commands.parse_exec(command_prefix, default_command, request.command(), config, request, *response, request_message);
}

void NRDPClient::handleNotification(const std::string &channel, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage *response_message) {
	client::configuration config(command_prefix);
	setup(config, request_message.header());
	commands.forward_submit(config, request_message, *response_message);
}

//////////////////////////////////////////////////////////////////////////
// Parser setup/Helpers
//

void NRDPClient::add_local_options(po::options_description &desc, client::configuration::data_type data) {
 	desc.add_options()
		("key", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->recipient, "token", _1)), 
		"The security token")

		("password", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->recipient, "token", _1)), 
		"The security token")

		("source-host", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->host_self, "host", _1)), 
		"Source/sender host name (default is auto which means use the name of the actual host)")

		("sender-host", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->host_self, "host", _1)), 
		"Source/sender host name (default is auto which means use the name of the actual host)")

		("token", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->recipient, "token", _1)), 
		"The security token")
 		;
}

void NRDPClient::setup(client::configuration &config, const ::Plugin::Common_Header& header) {
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

NRDPClient::connection_data NRDPClient::parse_header(const ::Plugin::Common_Header &header, client::configuration::data_type data) {
	nscapi::protobuf::functions::destination_container recipient, sender;
	nscapi::protobuf::functions::parse_destination(header, header.recipient_id(), recipient, true);
	nscapi::protobuf::functions::parse_destination(header, header.sender_id(), sender, true);
	return connection_data(recipient, data->recipient, sender);
}

//////////////////////////////////////////////////////////////////////////
// Parser implementations
//

int NRDPClient::clp_handler_impl::query(client::configuration::data_type data, const Plugin::QueryRequestMessage &request_message, Plugin::QueryResponseMessage &response_message) {
	NSC_LOG_ERROR_STD("SMTP does not support query patterns");
	return NSCAPI::hasFailed;
}

int NRDPClient::clp_handler_impl::submit(client::configuration::data_type data, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage &response_message) {
	const ::Plugin::Common_Header& request_header = request_message.header();
	connection_data con = parse_header(request_header, data);
	std::wstring channel = utf8::cvt<std::wstring>(request_message.channel());

	nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_header);

	nrdp::data nrdp_data;
	for (int i=0;i < request_message.payload_size(); ++i) {
		const ::Plugin::QueryResponseMessage::Response& payload = request_message.payload(i);
		std::wstring alias, msg, perf;
		NSCAPI::nagiosReturn result = nscapi::protobuf::functions::parse_simple_submit_request_payload(request_message.payload(i), alias, msg, perf);
		if (alias == _T("host_check"))
			nrdp_data.add_host(con.sender_hostname, result, utf8::cvt<std::string>(msg), utf8::cvt<std::string>(perf));
		else
			nrdp_data.add_service(con.sender_hostname, utf8::cvt<std::string>(alias), result, utf8::cvt<std::string>(msg), utf8::cvt<std::string>(perf));
	}

	boost::tuple<int,std::wstring> ret = instance->send(con, nrdp_data);
	nscapi::protobuf::functions::append_simple_submit_response_payload(response_message.add_payload(), "TODO", ret.get<0>(), utf8::cvt<std::string>(ret.get<1>()));
	return NSCAPI::isSuccess;
}

int NRDPClient::clp_handler_impl::exec(client::configuration::data_type data, const Plugin::ExecuteRequestMessage &request_message, Plugin::ExecuteResponseMessage &response_message) {
	const ::Plugin::Common_Header& request_header = request_message.header();
	connection_data con = parse_header(request_header, data);

	nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_header);

	nrdp::data nrdp_data;
	for (int i=0;i < request_message.payload_size(); ++i) {
		const ::Plugin::ExecuteRequestMessage::Request& payload = request_message.payload(i);
		nscapi::protobuf::functions::decoded_simple_command_data data = nscapi::protobuf::functions::parse_simple_exec_request_payload(request_message.payload(i));
		//nrdp_data.add_command(data.command, data.args);
	}
	boost::tuple<int,std::wstring> ret = instance->send(con, nrdp_data);
	nscapi::protobuf::functions::append_simple_exec_response_payload(response_message.add_payload(), "TODO", ret.get<0>(), utf8::cvt<std::string>(ret.get<1>()));
	return NSCAPI::isSuccess;
}

//////////////////////////////////////////////////////////////////////////
// Protocol implementations
//

boost::tuple<int,std::wstring> NRDPClient::send(connection_data data, const nrdp::data &nrdp_data) {
	try {
		NSC_DEBUG_MSG_STD("Connection details: " + data.to_string());
		http::client c;
		http::client::request_type request;
		request.add_default_headers();
		http::client::request_type::post_map_type post;
		post["token"] = data.token;
		post["XMLDATA"] = nrdp_data.render_request();
		post["cmd"] = "submitcheck";
		request.add_post_payload(post);
		NSC_DEBUG_MSG_STD(nrdp_data.render_request());
		NSC_DEBUG_MSG_STD(request.payload);
		http::client::response_type response = c.execute(data.host, data.port, "/nrdp/server/", request);
		NSC_DEBUG_MSG_STD(response.payload);
		return boost::make_tuple(NSCAPI::returnUNKNOWN, _T(""));
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Sending NSCA data", e);
		return boost::make_tuple(NSCAPI::returnUNKNOWN, _T("Error: ") + utf8::to_unicode(e.what()));
	} catch (...) {
		NSC_LOG_ERROR_EX("Sending NSCA data");
		return boost::make_tuple(NSCAPI::returnUNKNOWN, _T("Unknown error -- REPORT THIS!"));
	}
}
