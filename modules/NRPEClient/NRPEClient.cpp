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

#include <time.h>
#include <strEx.h>

#include <settings/client/settings_client.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_core_helper.hpp>

namespace sh = nscapi::settings_helper;

/**
 * Default c-tor
 * @return 
 */
NRPEClient::NRPEClient() {}

/**
 * Default d-tor
 * @return 
 */
NRPEClient::~NRPEClient() {}

bool NRPEClient::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {

	try {

		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias("NRPE", alias, "client");
		target_path = settings.alias().get_settings_path("targets");

		settings.alias().add_path_to_settings()
			("NRPE CLIENT SECTION", "Section for NRPE active/passive check module.")

			("handlers", sh::fun_values_path(boost::bind(&NRPEClient::add_command, this, _1, _2)), 
			"CLIENT HANDLER SECTION", "")

			("targets", sh::fun_values_path(boost::bind(&NRPEClient::add_target, this, _1, _2)), 
			"REMOTE TARGET DEFINITIONS", "")
			;

		settings.alias().add_key_to_settings()
			("channel", sh::string_key(&channel_, "NRPE"),
			"CHANNEL", "The channel to listen to.")

			;

		settings.register_all();
		settings.notify();

		nscapi::core_helper::core_proxy core(get_core(), get_id());
		targets.add_samples(get_settings_proxy(), target_path);
		targets.ensure_default(get_settings_proxy(), target_path);
		core.register_channel(channel_);
	} catch (std::exception &e) {
		NSC_LOG_ERROR_EXR("loading", e);
		return false;
	} catch (...) {
		NSC_LOG_ERROR_EX("loading");
		return false;
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
// Settings helpers
//

void NRPEClient::add_target(std::string key, std::string arg) {
	try {
		targets.add(get_settings_proxy(), target_path , key, arg);
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to add target: " + key, e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to add target: " + key);
	}
}

void NRPEClient::add_command(std::string name, std::string args) {
	try {
		nscapi::core_helper::core_proxy core(get_core(), get_id());
		std::string key = commands.add_command(name, args);
		if (!key.empty())
			core.register_command(key.c_str(), "NRPE relay for: " + name);
	} catch (boost::program_options::validation_error &e) {
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
bool NRPEClient::unloadModule() {
	return true;
}

struct client_handler : public socket_helpers::client::client_handler {
	void log_debug(std::string file, int line, std::string msg) const {
		if (GET_CORE()->should_log(NSCAPI::log_level::debug)) {
			GET_CORE()->log(NSCAPI::log_level::debug, file, line, msg);
		}
	}
	void log_error(std::string file, int line, std::string msg) const {
		if (GET_CORE()->should_log(NSCAPI::log_level::error)) {
			GET_CORE()->log(NSCAPI::log_level::error, file, line, msg);
		}
	}
};

void NRPEClient::query_fallback(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response, const Plugin::QueryRequestMessage &request_message) {
	client::configuration config(nrpe_client::command_prefix, 
		boost::shared_ptr<nrpe_client::clp_handler_impl>(new nrpe_client::clp_handler_impl(boost::shared_ptr<socket_helpers::client::client_handler>(new client_handler()))), 
		boost::shared_ptr<nrpe_client::target_handler>(new nrpe_client::target_handler(targets)));
	nrpe_client::setup(config, request_message.header());
	commands.parse_query(nrpe_client::command_prefix, nrpe_client::default_command, request.command(), config, request, *response, request_message);
}

void NRPEClient::nrpe_forward(const std::string &command, Plugin::QueryRequestMessage &request, Plugin::QueryResponseMessage *response) {
	client::configuration config(nrpe_client::command_prefix, 
		boost::shared_ptr<nrpe_client::clp_handler_impl>(new nrpe_client::clp_handler_impl(boost::shared_ptr<socket_helpers::client::client_handler>(new client_handler()))), 
		boost::shared_ptr<nrpe_client::target_handler>(new nrpe_client::target_handler(targets)));
	nrpe_client::setup(config, request.header());
	commands.forward_query(config, request, *response);
}

bool NRPEClient::commandLineExec(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response, const Plugin::ExecuteRequestMessage &request_message) {
	client::configuration config(nrpe_client::command_prefix, 
		boost::shared_ptr<nrpe_client::clp_handler_impl>(new nrpe_client::clp_handler_impl(boost::shared_ptr<socket_helpers::client::client_handler>(new client_handler()))), 
		boost::shared_ptr<nrpe_client::target_handler>(new nrpe_client::target_handler(targets)));
	nrpe_client::setup(config, request_message.header());
	return commands.parse_exec(nrpe_client::command_prefix, nrpe_client::default_command, request.command(), config, request, *response, request_message);
}

void NRPEClient::handleNotification(const std::string &channel, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage *response_message) {
	client::configuration config(nrpe_client::command_prefix);
	config.target_lookup = boost::shared_ptr<nrpe_client::target_handler>(new nrpe_client::target_handler(targets)); 
	config.handler = boost::shared_ptr<nrpe_client::clp_handler_impl>(new nrpe_client::clp_handler_impl(boost::shared_ptr<socket_helpers::client::client_handler>(new client_handler())));
	nrpe_client::setup(config, request_message.header());
	commands.forward_submit(config, request_message, *response_message);
}


//////////////////////////////////////////////////////////////////////////
// Parser setup/Helpers
//

