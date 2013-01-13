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

#include <settings/client/settings_client.hpp>


LUAScript::LUAScript() {
}
LUAScript::~LUAScript() {
}

namespace sh = nscapi::settings_helper;
namespace po = boost::program_options;

bool LUAScript::loadModule() {
	return false;
}
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

		boost::optional<boost::filesystem::path> ofile = lua::lua_script::find_script(root_, file);
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

bool LUAScript::hasCommandHandler() {
	return true;
}
bool LUAScript::hasMessageHandler() {
	return false;
}

NSCAPI::nagiosReturn LUAScript::handleRAWCommand(const wchar_t* char_command, const std::string &request, std::string &response) {
	std::string command = utf8::cvt<std::string>(char_command);
	boost::optional<scripts::command_definition<lua::lua_traits> > cmd = scripts_->find_command(scripts::nscp::tags::query_tag, command);
	if (!cmd) {
		cmd = scripts_->find_command(scripts::nscp::tags::simple_query_tag, command);
		if (!cmd) {
			NSC_LOG_ERROR(std::wstring(_T("Failed to find command: ")) + char_command);
			return NSCAPI::returnIgnored;
		}
		return lua_runtime_->on_query(command, cmd->information, cmd->function, true, request, response);
	}
	return lua_runtime_->on_query(command, cmd->information, cmd->function, false, request, response);
}



NSCAPI::nagiosReturn LUAScript::execute_and_load(std::list<std::wstring> args, std::wstring &message) {
	try {
		po::options_description desc("Options for the following commands: (exec, execute)");
		boost::program_options::variables_map vm;
		std::wstring file;
		desc.add_options()
			("help", "Display help")
			("script", po::wvalue<std::wstring>(&file), "The script to run")
			("file", po::wvalue<std::wstring>(&file), "The script to run")
			;

		std::vector<std::wstring> vargs(args.begin(), args.end());
		po::wparsed_options parsed = po::basic_command_line_parser<wchar_t>(vargs).options(desc).run();
		po::store(parsed, vm);
		po::notify(vm);

		if (vm.count("help") > 0) {
			std::stringstream ss;
			ss << desc;
			message = utf8::to_unicode(ss.str());
			return NSCAPI::returnUNKNOWN;
		}

		boost::optional<boost::filesystem::path> ofile = lua::lua_script::find_script(root_, file);
		if (!ofile) {
			message = _T("Script not found: ") + file;
			NSC_LOG_ERROR_STD(message);
			return NSCAPI::returnUNKNOWN;
		}

		scripts_->add_and_load("exec", utf8::cvt<std::string>((*ofile).string()));
		return NSCAPI::returnOK;
	} catch (const std::exception &e) {
		message = _T("Failed to execute script ") + utf8::to_unicode(e.what());
		NSC_LOG_ERROR_STD(message);
		return NSCAPI::returnUNKNOWN;
	} catch (...) {
		message = _T("Failed to execute script ");
		NSC_LOG_ERROR_STD(message);
		return NSCAPI::returnUNKNOWN;
	}
}

NSCAPI::nagiosReturn LUAScript::commandLineExec(const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &result) {
	if (command == _T("help")) {
		std::list<std::wstring> args;
		args.push_back(_T("--help"));
		std::wstring result;
		int ret = execute_and_load(args, result);
		return ret;
	} else if (command == _T("lua-execute") || command == _T("lua-run")
		|| command == _T("run") || command == _T("execute") || command == _T("exec") || command == _T("")) {
			return execute_and_load(arguments, result);
	}
	return NSCAPI::returnUNKNOWN;
//	return scripts_.on_exec(command, request, result);
}

NSCAPI::nagiosReturn LUAScript::handleSimpleNotification(const std::wstring channel, const std::wstring source, const std::wstring command, NSCAPI::nagiosReturn code, std::wstring msg, std::wstring perf) {
	return NSCAPI::returnUNKNOWN;
//	return scripts_.on_submission(command, request, result);
}




NSC_WRAP_DLL()
NSC_WRAPPERS_MAIN_DEF(LUAScript, _T("lua"))
NSC_WRAPPERS_IGNORE_MSG_DEF()
NSC_WRAPPERS_HANDLE_CMD_DEF()
NSC_WRAPPERS_CLI_DEF()
NSC_WRAPPERS_HANDLE_NOTIFICATION_DEF()
