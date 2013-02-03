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
#include "LUAScript.h"

#include <boost/program_options.hpp>

#include <strEx.h>
#include <time.h>
#include <error.hpp>
#include <file_helpers.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>

#include <settings/client/settings_client.hpp>

namespace sh = nscapi::settings_helper;
namespace po = boost::program_options;


bool LUAScript::loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode) {
	try {

		root_ = get_core()->getBasePath();
		nscp_runtime_.reset(new scripts::nscp::nscp_runtime_impl(get_id(), get_core()));
		lua_runtime_.reset(new lua::lua_runtime(utf8::cvt<std::string>(root_.string())));
		scripts_.reset(new scripts::script_manager<lua::lua_traits>(lua_runtime_, nscp_runtime_, get_id(), utf8::cvt<std::string>(alias)));

		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias(alias, _T("lua"));

		settings.alias().add_path_to_settings()
			(_T("LUA SCRIPT SECTION"), _T("Section for the LUAScripts module."))

			(_T("scripts"), sh::fun_values_path(boost::bind(&LUAScript::loadScript, this, _1, _2)), 
			_T("LUA SCRIPTS SECTION"), _T("A list of scripts available to run from the LuaSCript module."))
			;

		settings.register_all();
		settings.notify();

// 		if (!scriptDirectory_.empty()) {
// 			addAllScriptsFrom(scriptDirectory_);
// 		}

		scripts_->load_all();
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Exception caught: ") + utf8::to_unicode(e.what()));
		return false;
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Exception caught: <UNKNOWN EXCEPTION>"));
		return false;
	}

	return true;
}

bool LUAScript::loadScript(std::wstring alias, std::wstring file) {
	try {
		if (file.empty()) {
			file = alias;
			alias = _T("");
		}

		boost::optional<boost::filesystem::path> ofile = lua::lua_script::find_script(root_, utf8::cvt<std::string>(file));
		if (!ofile)
			return false;
		NSC_DEBUG_MSG_STD(_T("Adding script: ") + ofile->wstring() + _T(" as ") + alias + _T(")"));
		scripts_->add(utf8::cvt<std::string>(alias), ofile->string());
		return true;
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Could not load script: (Unknown exception) ") + file);
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
		lua_runtime_->on_query(request.command(), cmd->information, cmd->function, true, request, response, request_message);
	}
	return lua_runtime_->on_query(request.command(), cmd->information, cmd->function, false, request, response, request_message);
}

void LUAScript::commandLineExec(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response, const Plugin::ExecuteRequestMessage &request_message) {
	if (request.command() != "lua-execute" && request.command() != "lua-run"
		&& request.command() != "run" && request.command() != "execute" && request.command() != "exec" && request.command() != "") {
		return;
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
			return;

		boost::optional<boost::filesystem::path> ofile = lua::lua_script::find_script(root_, file);
		if (!ofile)
			return nscapi::protobuf::functions::set_response_bad(*response, "Script not found: " + file);
		scripts_->add_and_load("exec", utf8::cvt<std::string>((*ofile).string()));
	} catch (const std::exception &e) {
		return nscapi::protobuf::functions::set_response_bad(*response, "Failed to execute script " + utf8::utf8_from_native(e.what()));
	} catch (...) {
		return nscapi::protobuf::functions::set_response_bad(*response, "Failed to execute script.");
	}
}

void LUAScript::handleNotification(const std::string &channel, const Plugin::QueryResponseMessage::Response &request, Plugin::SubmitResponseMessage::Response *response, const Plugin::SubmitRequestMessage &request_message) {
//	return scripts_.on_submission(command, request, result);
}
