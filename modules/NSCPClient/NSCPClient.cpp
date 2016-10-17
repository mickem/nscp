/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "NSCPClient.h"

#include <boost/filesystem.hpp>
#include <boost/make_shared.hpp>

#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>
#include <settings/config.hpp>

#include "nscp_client.hpp"
#include "nscp_handler.hpp"

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