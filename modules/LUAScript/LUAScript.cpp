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

#include "LUAScript.h"

#include <boost/program_options.hpp>

#include <strEx.h>
#include <time.h>
#include <error/error.hpp>
#include <file_helpers.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>

#include <nscapi/nscapi_settings_helper.hpp>

namespace sh = nscapi::settings_helper;
namespace po = boost::program_options;

bool LUAScript::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode) {
	try {
		root_ = get_base_path();
		nscp_runtime_.reset(new scripts::nscp::nscp_runtime_impl(get_id(), get_core()));
		lua_runtime_.reset(new lua::lua_runtime(utf8::cvt<std::string>(root_.string())));
		scripts_.reset(new scripts::script_manager<lua::lua_traits>(lua_runtime_, nscp_runtime_, get_id(), utf8::cvt<std::string>(alias)));

		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias(alias, "lua");

		settings.alias().add_path_to_settings()
			("LUA SCRIPT SECTION", "Section for the LUAScripts module.")

			("scripts", sh::fun_values_path(boost::bind(&LUAScript::loadScript, this, _1, _2)),
				"LUA SCRIPTS SECTION", "A list of scripts available to run from the LuaSCript module.",
				"SCRIPT DEFENTION", "For more configuration options add a dedicated section")
			;

		settings.register_all();
		settings.notify();

		// 		if (!scriptDirectory_.empty()) {
		// 			addAllScriptsFrom(scriptDirectory_);
		// 		}

		scripts_->load_all();
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("load", e);
		return false;
	} catch (...) {
		NSC_LOG_ERROR_EX("load");
		return false;
	}

	return true;
}

bool LUAScript::loadScript(std::string alias, std::string file) {
	try {
		if (file.empty()) {
			file = alias;
			alias = "";
		}

		boost::optional<boost::filesystem::path> ofile = lua::lua_script::find_script(root_, file);
		if (!ofile)
			return false;
		NSC_DEBUG_MSG_STD("Adding script: " + ofile->string());
		scripts_->add(alias, ofile->string());
		return true;
	} catch (...) {
		NSC_LOG_ERROR_EX("load script");
	}
	return false;
}

bool LUAScript::unloadModule() {
	if (scripts_) {
		scripts_->unload_all();
		scripts_.reset();
	}
	return true;
}

void LUAScript::query_fallback(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response, const Plugin::QueryRequestMessage &request_message) {
	std::string response_buffer;
	boost::optional<scripts::command_definition<lua::lua_traits> > cmd = scripts_->find_command(scripts::nscp::tags::query_tag, request.command());
	if (!cmd) {
		cmd = scripts_->find_command(scripts::nscp::tags::simple_query_tag, request.command());
		if (!cmd)
			return nscapi::protobuf::functions::set_response_bad(*response, "Failed to find command: " + request.command());
		return lua_runtime_->on_query(request.command(), cmd->information, cmd->function, true, request, response, request_message);
	}
	return lua_runtime_->on_query(request.command(), cmd->information, cmd->function, false, request, response, request_message);
}

bool LUAScript::commandLineExec(const int target_mode, const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response, const Plugin::ExecuteRequestMessage &request_message) {
	if (request.command() != "lua-script" && request.command() != "lua-run"
		&& request.command() != "run" && request.command() != "execute" && request.command() != "") {
		boost::optional<scripts::command_definition<lua::lua_traits> > cmd = scripts_->find_command(scripts::nscp::tags::simple_exec_tag, request.command());
		if (cmd) {
			lua_runtime_->on_exec(request.command(), cmd->information, cmd->function, true, request, response, request_message);
		}
		return false;
	}

	try {
		po::options_description desc = nscapi::program_options::create_desc(request);
		std::string file;
		desc.add_options()
			("script", po::value<std::string>(&file), "The script to run")
			("file", po::value<std::string>(&file), "The script to run")
			;
		boost::program_options::variables_map vm;
		nscapi::program_options::unrecognized_map script_options;
		if (!nscapi::program_options::process_arguments_unrecognized(vm, script_options, desc, request, *response))
			return true;

		boost::optional<boost::filesystem::path> ofile = lua::lua_script::find_script(root_, file);
		if (!ofile) {
			nscapi::protobuf::functions::set_response_bad(*response, "Script not found: " + file);
			return true;
		}
		scripts::script_information<lua::lua_traits> *instance = scripts_->add_and_load("exec", (*ofile).string());
		lua_runtime_->exec_main(instance, script_options, response);
		return true;
	} catch (const std::exception &e) {
		nscapi::protobuf::functions::set_response_bad(*response, "Failed to execute script " + utf8::utf8_from_native(e.what()));
		return true;
	} catch (...) {
		nscapi::protobuf::functions::set_response_bad(*response, "Failed to execute script.");
		return true;
	}
}

void LUAScript::handleNotification(const std::string &channel, const Plugin::QueryResponseMessage::Response &request, Plugin::SubmitResponseMessage::Response *response, const Plugin::SubmitRequestMessage &request_message) {
	//	return scripts_.on_submission(command, request, result);
}