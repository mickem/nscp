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

#include "smtp.hpp"

namespace sh = nscapi::settings_helper;

const std::wstring SMTPClient::command_prefix = _T("smtp");
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

/**
 * Load (initiate) module.
 * Start the background collector thread and let it run until unloadModule() is called.
 * @return true
 */
bool SMTPClient::loadModule() {
	return false;
}

bool SMTPClient::loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode) {

	std::wstring template_string, sender, recipient;
	try {
		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias(_T("SMTP"), alias, _T("client"));
		target_path = settings.alias().get_settings_path(_T("targets"));

		settings.alias().add_path_to_settings()
			(_T("SMTP CLIENT SECTION"), _T("Section for SMTP passive check module."))
			(_T("handlers"), sh::fun_values_path(boost::bind(&SMTPClient::add_command, this, _1, _2)), 
			_T("CLIENT HANDLER SECTION"), _T(""))

			(_T("targets"), sh::fun_values_path(boost::bind(&SMTPClient::add_target, this, _1, _2)), 
			_T("REMOTE TARGET DEFINITIONS"), _T(""))
			;

		settings.alias().add_key_to_settings()
			(_T("channel"), sh::wstring_key(&channel_, _T("SMTP")),
			_T("CHANNEL"), _T("The channel to listen to."))

			;

		settings.register_all();
		settings.notify();

		targets.add_missing(get_settings_proxy(), target_path, _T("default"), _T(""), true);


		get_core()->registerSubmissionListener(get_id(), channel_);
		register_command(_T("smtp_query"), _T("Check remote SMTP host"));
		register_command(_T("smtp_submit"), _T("Submit (via query) remote SMTP host"));
		register_command(_T("smtp_forward"), _T("Forward query to remote SMTP host"));
		register_command(_T("smtp_exec"), _T("Execute (via query) remote SMTP host"));
		register_command(_T("smtp_help"), _T("Help on using SMTP Client"));
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

void SMTPClient::add_target(std::wstring key, std::wstring arg) {
	try {
		targets.add(get_settings_proxy(), target_path , key, arg);
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Failed to add target: ") + key + _T(", ") + utf8::to_unicode(e.what()));
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to add target: ") + key);
	}
}

void SMTPClient::add_command(std::wstring name, std::wstring args) {
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
bool SMTPClient::unloadModule() {
	return true;
}



NSCAPI::nagiosReturn SMTPClient::handleRAWCommand(const wchar_t* char_command, const std::string &request, std::string &result) {
	std::wstring cmd = client::command_line_parser::parse_command(char_command, command_prefix);

	Plugin::QueryRequestMessage message;
	message.ParseFromString(request);

	client::configuration config;
	setup(config, message.header());

	return commands.process_query(cmd, config, message, result);
}

NSCAPI::nagiosReturn SMTPClient::commandRAWLineExec(const wchar_t* char_command, const std::string &request, std::string &result) {
	std::wstring cmd = client::command_line_parser::parse_command(char_command, command_prefix);

	Plugin::ExecuteRequestMessage message;
	message.ParseFromString(request);

	client::configuration config;
	setup(config, message.header());

	return commands.process_exec(cmd, config, message, result);
}

NSCAPI::nagiosReturn SMTPClient::handleRAWNotification(const wchar_t* channel, std::string request, std::string &result) {
	Plugin::SubmitRequestMessage message;
	message.ParseFromString(request);

	client::configuration config;
	setup(config, message.header());

	return client::command_line_parser::do_relay_submit(config, message, result);
}

//////////////////////////////////////////////////////////////////////////
// Parser setup/Helpers
//

void SMTPClient::add_local_options(po::options_description &desc, client::configuration::data_type data) {

 	desc.add_options()
		("sender", po::value<std::string>()->notifier(boost::bind(&nscapi::functions::destination_container::set_string_data, &data->recipient, "sender", _1)), 
			"Length of payload (has to be same as on the server)")

		("recipient", po::value<std::string>()->notifier(boost::bind(&nscapi::functions::destination_container::set_string_data, &data->recipient, "recipient", _1)), 
			"Length of payload (has to be same as on the server)")

		("template", po::value<std::string>()->notifier(boost::bind(&nscapi::functions::destination_container::set_string_data, &data->recipient, "template", _1)), 
		"Do not initial an ssl handshake with the server, talk in plaintext.")
 		;
}

void SMTPClient::setup(client::configuration &config, const ::Plugin::Common_Header& header) {
	boost::shared_ptr<clp_handler_impl> handler = boost::shared_ptr<clp_handler_impl>(new clp_handler_impl(this));
	add_local_options(config.local, config.data);

	config.data->recipient.id = header.recipient_id();
	std::wstring recipient = utf8::cvt<std::wstring>(config.data->recipient.id);
	if (!targets.has_object(recipient)) {
		recipient = _T("default");
	}
	nscapi::targets::optional_target_object opt = targets.find_object(recipient);

	if (opt) {
		nscapi::targets::target_object t = *opt;
		nscapi::functions::destination_container def = t.to_destination_container();
		config.data->recipient.apply(def);
	}
	config.data->host_self.id = "self";
	//config.data->host_self.host = hostname_;

	config.target_lookup = handler;
	config.handler = handler;
}

SMTPClient::connection_data SMTPClient::parse_header(const ::Plugin::Common_Header &header, client::configuration::data_type data) {
	nscapi::functions::destination_container recipient;
	nscapi::functions::parse_destination(header, header.recipient_id(), recipient, true);
	return connection_data(recipient, data->recipient);
}

//////////////////////////////////////////////////////////////////////////
// Parser implementations
int SMTPClient::clp_handler_impl::query(client::configuration::data_type data, const Plugin::QueryRequestMessage &request_message, std::string &reply) {
	NSC_LOG_ERROR_STD(_T("SMTP does not support query patterns"));
	return NSCAPI::hasFailed;
}

int SMTPClient::clp_handler_impl::submit(client::configuration::data_type data, const Plugin::SubmitRequestMessage &request_message, std::string &reply) {
	const ::Plugin::Common_Header& request_header = request_message.header();
	connection_data con = parse_header(request_header, data);
	std::wstring channel = utf8::cvt<std::wstring>(request_message.channel());

	Plugin::SubmitResponseMessage response_message;
	nscapi::functions::make_return_header(response_message.mutable_header(), request_header);

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
		nscapi::functions::append_simple_submit_response_payload(response_message.add_payload(), "TODO", NSCAPI::returnOK, "Message send successfully");
	}
	response_message.SerializeToString(&reply);
	return NSCAPI::isSuccess;

}

int SMTPClient::clp_handler_impl::exec(client::configuration::data_type data, const Plugin::ExecuteRequestMessage &request_message, std::string &reply) {
	NSC_LOG_ERROR_STD(_T("SMTP does not support exec patterns"));
	return NSCAPI::hasFailed;
}

//////////////////////////////////////////////////////////////////////////
// Protocol implementations
//

NSC_WRAP_DLL()
NSC_WRAPPERS_MAIN_DEF(SMTPClient)
NSC_WRAPPERS_IGNORE_MSG_DEF()
NSC_WRAPPERS_HANDLE_CMD_DEF()
NSC_WRAPPERS_CLI_DEF()
NSC_WRAPPERS_HANDLE_NOTIFICATION_DEF()

