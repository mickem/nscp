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
#include "SMTPClient.h"

#include <utils.h>
#include <list>
#include <string>

#include <strEx.h>

#include <settings/client/settings_client.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_plugin_interface.hpp>

#include "smtp.hpp"

namespace sh = nscapi::settings_helper;

const std::string command_prefix("smtp");
const std::string default_command("submit");
/**
 * Default c-tor
 * @return 
 */
SMTPClient::SMTPClient() {}

/**
 * Default d-tor
 * @return 
 */
SMTPClient::~SMTPClient() {}

bool SMTPClient::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {

	std::wstring template_string, sender, recipient;
	try {
		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias("SMTP", alias, "client");
		target_path = settings.alias().get_settings_path("targets");

		settings.alias().add_path_to_settings()
			("SMTP CLIENT SECTION", "Section for SMTP passive check module.")
			("handlers", sh::fun_values_path(boost::bind(&SMTPClient::add_command, this, _1, _2)), 
			"CLIENT HANDLER SECTION", "")

			("targets", sh::fun_values_path(boost::bind(&SMTPClient::add_target, this, _1, _2)), 
			"REMOTE TARGET DEFINITIONS", "")
			;

		settings.alias().add_key_to_settings()
			("channel", sh::string_key(&channel_, "SMTP"),
			"CHANNEL", "The channel to listen to.")

			;

		settings.register_all();
		settings.notify();

		targets.add_missing(get_settings_proxy(), target_path, "default", "", true);
		nscapi::core_helper::core_proxy core(get_core(), get_id());
		core.register_channel(channel_);
	} catch (const nscapi::nscapi_exception &e) {
		NSC_LOG_ERROR_EXR("load", e);
		return false;
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("load", e);
		return false;
	} catch (...) {
		NSC_LOG_ERROR_EX("load");
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

void SMTPClient::add_target(std::string key, std::string arg) {
	try {
		targets.add(get_settings_proxy(), target_path , key, arg);
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to add target: " + key, e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to add target: " + key);
	}
}

void SMTPClient::add_command(std::string name, std::string args) {
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
bool SMTPClient::unloadModule() {
	return true;
}

void SMTPClient::query_fallback(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response, const Plugin::QueryRequestMessage &request_message) {
	client::configuration config(command_prefix, boost::shared_ptr<clp_handler_impl>(new clp_handler_impl()), boost::shared_ptr<target_handler>(new target_handler(targets)));
	setup(config, request_message.header());
	commands.parse_query(command_prefix, default_command, request.command(), config, request, *response, request_message);
}

bool SMTPClient::commandLineExec(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response, const Plugin::ExecuteRequestMessage &request_message) {
	client::configuration config(command_prefix, boost::shared_ptr<clp_handler_impl>(new clp_handler_impl()), boost::shared_ptr<target_handler>(new target_handler(targets)));
	setup(config, request_message.header());
	return commands.parse_exec(command_prefix, default_command, request.command(), config, request, *response, request_message);
}

void SMTPClient::handleNotification(const std::string &channel, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage *response_message) {
	client::configuration config(command_prefix, boost::shared_ptr<clp_handler_impl>(new clp_handler_impl()), boost::shared_ptr<target_handler>(new target_handler(targets)));
	setup(config, request_message.header());
	commands.forward_submit(config, request_message, *response_message);
}


//////////////////////////////////////////////////////////////////////////
// Parser setup/Helpers
//

void SMTPClient::add_local_options(po::options_description &desc, client::configuration::data_type data) {

 	desc.add_options()
		("sender", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->recipient, "sender", _1)), 
			"Length of payload (has to be same as on the server)")

		("recipient", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->recipient, "recipient", _1)), 
			"Length of payload (has to be same as on the server)")

		("template", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->recipient, "template", _1)), 
		"Do not initial an ssl handshake with the server, talk in plaintext.")
 		;
}

void SMTPClient::setup(client::configuration &config, const ::Plugin::Common_Header& header) {
	add_local_options(config.local, config.data);

	config.data->recipient.id = header.recipient_id();
	config.default_command = default_command;
	std::string recipient = config.data->recipient.id;
	if (!config.target_lookup->has_object(recipient))
		recipient = "default";
	config.target_lookup->apply(config.data->recipient, recipient);
	config.data->host_self.id = "self";
	//config.data->host_self.host = hostname_;
}

SMTPClient::connection_data parse_header(const ::Plugin::Common_Header &header, client::configuration::data_type data) {
	nscapi::protobuf::functions::destination_container recipient;
	nscapi::protobuf::functions::parse_destination(header, header.recipient_id(), recipient, true);
	return SMTPClient::connection_data(recipient, data->recipient);
}


nscapi::protobuf::types::destination_container SMTPClient::target_handler::lookup_target(std::string &id) const {
	nscapi::targets::optional_target_object opt = targets_.find_object(id);
	if (opt)
		return opt->to_destination_container();
	nscapi::protobuf::types::destination_container ret;
	return ret;
}

bool SMTPClient::target_handler::has_object(std::string alias) const {
	return targets_.has_object(alias);
}
bool SMTPClient::target_handler::apply(nscapi::protobuf::types::destination_container &dst, const std::string key) {
	nscapi::targets::optional_target_object opt = targets_.find_object(key);
	if (opt)
		dst.apply(opt->to_destination_container());
	return opt;
}

//////////////////////////////////////////////////////////////////////////
// Parser implementations
int SMTPClient::clp_handler_impl::query(client::configuration::data_type data, const Plugin::QueryRequestMessage &request_message, Plugin::QueryResponseMessage &response_message) {
	NSC_LOG_ERROR_STD("SMTP does not support query patterns");
	return NSCAPI::hasFailed;
}

int SMTPClient::clp_handler_impl::submit(client::configuration::data_type data, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage &response_message) {
	const ::Plugin::Common_Header& request_header = request_message.header();
	connection_data con = parse_header(request_header, data);
	std::wstring channel = utf8::cvt<std::wstring>(request_message.channel());

	nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_header);

	for (int i=0;i < request_message.payload_size(); ++i) {
		const ::Plugin::QueryResponseMessage::Response& payload = request_message.payload(i);
		boost::asio::io_service io_service;
		boost::shared_ptr<smtp::client::smtp_client> client(new smtp::client::smtp_client(io_service));
		std::list<std::string> recipients;
		std::string message = con.template_string;
		strEx::replace(message, "%message%", payload.message());
		recipients.push_back(con.recipient_str);
		client->send_mail(con.sender, recipients, "Hello world\n");
		io_service.run();
		nscapi::protobuf::functions::append_simple_submit_response_payload(response_message.add_payload(), "TODO", NSCAPI::returnOK, "Message send successfully");
	}
	return NSCAPI::isSuccess;

}

int SMTPClient::clp_handler_impl::exec(client::configuration::data_type data, const Plugin::ExecuteRequestMessage &request_message, Plugin::ExecuteResponseMessage &response_message) {
	NSC_LOG_ERROR_STD("SMTP does not support exec patterns");
	return NSCAPI::hasFailed;
}

//////////////////////////////////////////////////////////////////////////
// Protocol implementations
//

