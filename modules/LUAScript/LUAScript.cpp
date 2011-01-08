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
#include <strEx.h>
#include <time.h>
#include <filter_framework.hpp>
#include <error.hpp>
#include <file_helpers.hpp>


LUAScript gLUAScript;

LUAScript::LUAScript() {
}
LUAScript::~LUAScript() {
}

namespace sh = nscapi::settings_helper;

bool LUAScript::loadModule() {
	return false;
}
bool LUAScript::loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode) {
	//std::wstring appRoot = file_helpers::folders::get_local_appdata_folder(SZAPPNAME);
	try {

		sh::settings_registry settings(nscapi::plugin_singleton->get_core());
		settings.set_alias(alias, _T("lua"));

		settings.alias().add_path_to_settings()
			(_T("LUA SCRIPT SECTION"), _T("Section for the LUAScripts module."))

			(_T("scripts"), sh::fun_values_path(boost::bind(&LUAScript::loadScript, this, _1)), 
			_T("LUA SCRIPTS SECTION"), _T("A list of scripts available to run from the LuaSCript module."))
			;

		settings.register_all();
		settings.notify();

// 		if (!scriptDirectory_.empty()) {
// 			addAllScriptsFrom(scriptDirectory_);
// 		}
 		root_ = get_core()->getBasePath();

		// 	} catch (nrpe::server::nrpe_exception &e) {
		// 		NSC_LOG_ERROR_STD(_T("Exception caught: ") + e.what());
		// 		return false;
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Exception caught: <UNKNOWN EXCEPTION>"));
		return false;
	}
	return true;

// 	std::list<std::wstring>::const_iterator it;
// 	for (it = commands.begin(); it != commands.end(); ++it) {
// 		loadScript((*it));
// 	}
	return true;
}

void LUAScript::register_command(script_wrapper::lua_script* script, std::wstring command, std::wstring function) {
	NSC_LOG_MESSAGE(_T("Script loading: ") + script->get_script() + _T(": ") + command);
	strEx::wci_string bstr = command.c_str();
	commands_[bstr] = lua_func(script, function);
}

bool LUAScript::loadScript(const std::wstring file) {
	try {
		std::wstring file_ = file;

		if (!file_helpers::checks::exists(file_)) {
			file_ = root_ + file;
			if (!file_helpers::checks::exists(file_)) {
				NSC_LOG_ERROR(_T("Script not found: ") + file + _T(" (") + file_ + _T(")"));
				return false;
			}
		}
		NSC_DEBUG_MSG_STD(_T("Loading script: ") + file + _T(" (") + file_ + _T(")"));
		script_wrapper::lua_script *script = new script_wrapper::lua_script(file_);
		script->pre_load(this);
		scripts_.push_back(script);
		return true;
	} catch (script_wrapper::LUAException e) {
		NSC_LOG_ERROR_STD(_T("Could not load script: ") + file + _T(", ") + e.getMessage());
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Could not load script: (Unknown exception) ") + file);
	}
	return false;
}


bool LUAScript::unloadModule() {
	for (script_list::const_iterator cit = scripts_.begin(); cit != scripts_.end() ; ++cit) {
		delete (*cit);
	}
	scripts_.clear();
	return true;
}

bool LUAScript::hasCommandHandler() {
	return true;
}
bool LUAScript::hasMessageHandler() {
	return false;
}


bool LUAScript::reload(std::wstring &message) {
	bool error = false;
	commands_.clear();
	for (script_list::const_iterator cit = scripts_.begin(); cit != scripts_.end() ; ++cit) {
		try {
			(*cit)->reload(this);
		} catch (script_wrapper::LUAException e) {
			error = true;
			message += _T("Exception when reloading script: ") + (*cit)->get_script() + _T(": ") + e.getMessage();
			NSC_LOG_ERROR_STD(_T("Exception when reloading script: ") + (*cit)->get_script() + _T(": ") + e.getMessage());
		} catch (...) {
			error = true;
			message += _T("Unhandeled Exception when reloading script: ") + (*cit)->get_script();
			NSC_LOG_ERROR_STD(_T("Unhandeled Exception when reloading script: ") + (*cit)->get_script());
		}
	}
	if (!error)
		message = _T("LUA scripts Reloaded...");
	return !error;
}



NSCAPI::nagiosReturn LUAScript::handleCommand(const strEx::wci_string command, std::list<std::wstring> arguments, std::wstring &message, std::wstring &perf) {
	if (command == _T("LuaReload")) {
		return reload(message)?NSCAPI::returnOK:NSCAPI::returnCRIT;
	}
	cmd_list::const_iterator cit = commands_.find(command);
	if (cit == commands_.end())
		return NSCAPI::returnIgnored;
	return (*cit).second.handleCommand(this, command.c_str(), arguments, message, perf);
}


NSC_WRAP_DLL();
NSC_WRAPPERS_MAIN_DEF(gLUAScript);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gLUAScript);
