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

#include "CheckMKClient.h"

#include "check_mk_client.hpp"
#include "check_mk_handler.hpp"

#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>

#include <time.h>
#include <str/xtos.hpp>

#include <boost/make_shared.hpp>

/**
 * Default c-tor
 * @return
 */
CheckMKClient::CheckMKClient() : client_("check_mk", boost::make_shared<check_mk_client::check_mk_client_handler>(), boost::make_shared<check_mk_handler::options_reader_impl>()) {}

/**
 * Default d-tor
 * @return
 */
CheckMKClient::~CheckMKClient() {}

bool CheckMKClient::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode) {
	std::map<std::wstring, std::wstring> commands;

	try {
		root_ = get_base_path();
		nscp_runtime_.reset(new scripts::nscp::nscp_runtime_impl(get_id(), get_core()));
		lua_runtime_.reset(new lua::lua_runtime(utf8::cvt<std::string>(root_.string())));
		lua_runtime_->register_plugin(boost::shared_ptr<check_mk::check_mk_plugin>(new check_mk::check_mk_plugin()));
		scripts_.reset(new scripts::script_manager<lua::lua_traits>(lua_runtime_, nscp_runtime_, get_id(), utf8::cvt<std::string>(alias)));

		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias("check_mk", alias, "client");
		client_.set_path(settings.alias().get_settings_path("targets"));

		settings.alias().add_path_to_settings()
			("CHECK MK CLIENT SECTION", "Section for check_mk active/passive check module.")

			("handlers", sh::fun_values_path(boost::bind(&CheckMKClient::add_command, this, _1, _2)),
				"CLIENT HANDLER SECTION", "",
				"CLIENT", "For more configuration options add a dedicated section")

			("targets", sh::fun_values_path(boost::bind(&CheckMKClient::add_target, this, _1, _2)),
				"REMOTE TARGET DEFINITIONS", "",
				"TARGET", "For more configuration options add a dedicated section")

			("scripts", sh::fun_values_path(boost::bind(&CheckMKClient::add_script, this, _1, _2)),
				"REMOTE TARGET DEFINITIONS", "",
				"SCRIPT", "For more configuration options add a dedicated section")
			;

		settings.alias().add_key_to_settings()
			("channel", sh::string_key(&channel_, "CheckMK"),
				"CHANNEL", "The channel to listen to.")

			;

		settings.register_all();
		settings.notify();

		client_.finalize(get_settings_proxy());

		if (scripts_->empty()) {
			add_script("default", "default_check_mk.lua");
		}

		nscapi::core_helper core(get_core(), get_id());
		core.register_channel(channel_);

		scripts_->load_all();
	} catch (nsclient::nsclient_exception &e) {
		NSC_LOG_ERROR_EXR("NSClient API exception: ", e);
		return false;
	} catch (std::exception &e) {
		NSC_LOG_ERROR_EXR("loading", e);
		return false;
	} catch (...) {
		NSC_LOG_ERROR_EX("loading");
		return false;
	}
	return true;
}

bool CheckMKClient::add_script(std::string alias, std::string file) {
	try {
		if (file.empty()) {
			file = alias;
			alias = "";
		}

		boost::optional<boost::filesystem::path> ofile = lua::lua_script::find_script(root_, file);
		if (!ofile)
			return false;
		scripts_->add(alias, ofile->string());
		return true;
	} catch (...) {
		NSC_LOG_ERROR("Could not load script: " + file);
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
// Settings helpers
//

void CheckMKClient::add_target(std::string key, std::string arg) {
	try {
		client_.add_target(get_settings_proxy(), key, arg);
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to add target: " + key, e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to add target: " + key);
	}
}

void CheckMKClient::add_command(std::string key, std::string arg) {
	try {
		nscapi::core_helper core(get_core(), get_id());
		std::string k = client_.add_command(key, arg);
		if (!k.empty())
			core.register_command(k.c_str(), "NSCA relay for: " + key);
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
bool CheckMKClient::unloadModule() {
	client_.clear();
	scripts_.reset();
	lua_runtime_.reset();
	nscp_runtime_.reset();
	return true;
}

void CheckMKClient::query_fallback(const Plugin::QueryRequestMessage &request_message, Plugin::QueryResponseMessage &response_message) {
	client_.do_query(request_message, response_message);
}

bool CheckMKClient::commandLineExec(const int target_mode, const Plugin::ExecuteRequestMessage &request, Plugin::ExecuteResponseMessage &response) {
	if (target_mode == NSCAPI::target_module)
		return client_.do_exec(request, response, "submit_");
	return false;
}

void CheckMKClient::handleNotification(const std::string &, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage *response_message) {
	client_.do_submit(request_message, *response_message);
}