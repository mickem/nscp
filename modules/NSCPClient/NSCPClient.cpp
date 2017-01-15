/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "NSCPClient.h"
#include "nscp_client.hpp"
#include "nscp_handler.hpp"

#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>

#include <boost/filesystem.hpp>
#include <boost/make_shared.hpp>


namespace sh = nscapi::settings_helper;

/**
 * Default c-tor
 * @return
 */
NSCPClient::NSCPClient() : client_("nscp", boost::make_shared<nscp_client::nscp_client_handler<> >(), boost::make_shared<nscp_handler::options_reader_impl>()) {}

/**
 * Default d-tor
 * @return
 */
NSCPClient::~NSCPClient() {}

bool NSCPClient::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode) {
	try {
		client_.clear();
		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias("NSCP", alias, "client");

		client_.set_path(settings.alias().get_settings_path("targets"));

		settings.alias().add_path_to_settings()
			("NSCP CLIENT SECTION", "Section for NSCP active/passive check module.")

			("handlers", sh::fun_values_path(boost::bind(&NSCPClient::add_command, this, _1, _2)),
				"CLIENT HANDLER SECTION", "",
				"TARGET", "For more configuration options add a dedicated section")

			("targets", sh::fun_values_path(boost::bind(&NSCPClient::add_target, this, _1, _2)),
				"REMOTE TARGET DEFINITIONS", "",
				"TARGET", "For more configuration options add a dedicated section")
			;

		settings.alias().add_key_to_settings()
			("channel", sh::string_key(&channel_, "NSCP"),
				"CHANNEL", "The channel to listen to.")

			;

		settings.register_all();
		settings.notify();

		client_.finalize(get_settings_proxy());

		nscapi::core_helper core(get_core(), get_id());
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

void NSCPClient::add_target(std::string key, std::string arg) {
	try {
		client_.add_target(get_settings_proxy(), key, arg);
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to add target: " + key, e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to add target: " + key);
	}
}

void NSCPClient::add_command(std::string name, std::string args) {
	try {
		nscapi::core_helper core(get_core(), get_id());
		std::string key = client_.add_command(name, args);
		if (!key.empty())
			core.register_command(key.c_str(), "NSCP relay for: " + name);
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
bool NSCPClient::unloadModule() {
	return true;
}

void NSCPClient::query_fallback(const Plugin::QueryRequestMessage &request_message, Plugin::QueryResponseMessage &response_message) {
	client_.do_query(request_message, response_message);
}

bool NSCPClient::commandLineExec(const int target_mode, const Plugin::ExecuteRequestMessage &request, Plugin::ExecuteResponseMessage &response) {
	BOOST_FOREACH(const Plugin::ExecuteRequestMessage::Request &payload, request.payload()) {
		if (payload.arguments_size() == 0 || payload.arguments(0) == "help") {
			Plugin::ExecuteResponseMessage::Response *rp = response.add_payload();
			nscapi::protobuf::functions::set_response_bad(*rp, "Usage: nscp nscp --help");
			return true;
		}
	}
	if (target_mode == NSCAPI::target_module)
		return client_.do_exec(request, response, "check_nscp");
	return false;
}

void NSCPClient::handleNotification(const std::string &, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage *response_message) {
	client_.do_submit(request_message, *response_message);
}



//////////////////////////////////////////////////////////////////////////
// Parser setup/Helpers
//
