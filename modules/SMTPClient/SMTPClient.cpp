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

#include "SMTPClient.h"

#include <str/xtos.hpp>
#include <nscapi/macros.hpp>

#include <boost/make_shared.hpp>

#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_core_helper.hpp>

#include "smtp.hpp"

#include "smtp_client.hpp"
#include "smtp_handler.hpp"

/**
 * Default c-tor
 * @return
 */
SMTPClient::SMTPClient() : client_("graphite", boost::make_shared<smtp_client::smtp_client_handler>(), boost::make_shared<smtp_handler::options_reader_impl>()) {}

/**
 * Default d-tor
 * @return
 */
SMTPClient::~SMTPClient() {}

bool SMTPClient::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode) {
	std::wstring template_string, sender, recipient;
	try {
		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias("SMTP", alias, "client");
		client_.set_path(settings.alias().get_settings_path("targets"));

		settings.alias().add_path_to_settings()
			("SMTP CLIENT SECTION", "Section for SMTP passive check module.")
			("handlers", sh::fun_values_path(boost::bind(&SMTPClient::add_command, this, _1, _2)),
				"CLIENT HANDLER SECTION", "",
				"CLIENT HANDLER", "For more configuration options add a dedicated section")

			("targets", sh::fun_values_path(boost::bind(&SMTPClient::add_target, this, _1, _2)),
				"REMOTE TARGET DEFINITIONS", "",
				"TARGET", "For more configuration options add a dedicated section")
			;

		settings.alias().add_key_to_settings()
			("channel", sh::string_key(&channel_, "SMTP"),
				"CHANNEL", "The channel to listen to.")

			;

		settings.register_all();
		settings.notify();

		client_.finalize(get_settings_proxy());

		nscapi::core_helper core(get_core(), get_id());
		core.register_channel(channel_);
	} catch (const nscapi::nscapi_exception &e) {
		NSC_LOG_ERROR_EXR("load", e);
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

void SMTPClient::add_target(std::string key, std::string arg) {
	try {
		client_.add_target(get_settings_proxy(), key, arg);
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to add target: " + key, e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to add target: " + key);
	}
}

void SMTPClient::add_command(std::string key, std::string arg) {
	try {
		nscapi::core_helper core(get_core(), get_id());
		std::string k = client_.add_command(key, arg);
		if (!k.empty())
			core.register_command(k.c_str(), "SMTP relay for: " + key);
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to add command: " + key, e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to add command: " + key);
	}
}

/**
 * Unload (terminate) module.
 * Attempt to stop the background processing thread.
 * @return true if successfully, false if not (if not things might be bad)
 */
bool SMTPClient::unloadModule() {
	client_.clear();
	return true;
}

void SMTPClient::query_fallback(const Plugin::QueryRequestMessage &request_message, Plugin::QueryResponseMessage &response_message) {
	client_.do_query(request_message, response_message);
}

bool SMTPClient::commandLineExec(const int target_mode, const Plugin::ExecuteRequestMessage &request, Plugin::ExecuteResponseMessage &response) {
	if (target_mode == NSCAPI::target_module)
		return client_.do_exec(request, response, "_submit");
	return false;
}

void SMTPClient::handleNotification(const std::string &, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage *response_message) {
	client_.do_submit(request_message, *response_message);
}