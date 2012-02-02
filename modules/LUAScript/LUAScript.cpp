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

#include <settings/client/settings_client.hpp>


LUAScript::LUAScript() : registry(new lua_wrappers::lua_registry()) {
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

		root_ = get_core()->getBasePath();

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




		BOOST_FOREACH(script_container &script, scripts_) {
			try {
				instances_.push_back(script_wrapper::lua_script::create_instance(get_core(), get_id(), registry, script.alias, script.script.string()));
			} catch (const lua_wrappers::LUAException &e) {
				NSC_LOG_ERROR_STD(_T("Could not load script ") + script.to_wstring() + _T(": ") + e.getMessage());
			} catch (const std::exception &e) {
				NSC_LOG_ERROR_STD(_T("Could not load script ") + script.to_wstring() + _T(": ") + utf8::to_unicode(e.what()));
			}
		}

		// 	} catch (nrpe::server::nrpe_exception &e) {
		// 		NSC_LOG_ERROR_STD(_T("Exception caught: ") + e.what());
		// 		return false;
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Exception caught: ") + utf8::to_unicode(e.what()));
		return false;
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Exception caught: <UNKNOWN EXCEPTION>"));
		return false;
	}

// 	std::list<std::wstring>::const_iterator it;
// 	for (it = commands.begin(); it != commands.end(); ++it) {
// 		loadScript((*it));
// 	}
	return true;
}

boost::optional<boost::filesystem::wpath> LUAScript::find_file(std::wstring file) {
	std::list<boost::filesystem::wpath> checks;
	checks.push_back(file);
	checks.push_back(root_ / _T("scripts") / _T("lua") / file);
	checks.push_back(root_ / _T("scripts") / file);
	checks.push_back(root_ / _T("lua") / file);
	checks.push_back(root_ / file);
	BOOST_FOREACH(boost::filesystem::wpath c, checks) {
		NSC_DEBUG_MSG_STD(_T("Looking for: ") + c.string());
		if (boost::filesystem::exists(c))
			return boost::optional<boost::filesystem::wpath>(c);
	}
	NSC_LOG_ERROR(_T("Script not found: ") + file);
	return boost::optional<boost::filesystem::wpath>();
}

bool LUAScript::loadScript(std::wstring alias, std::wstring file) {
	try {
		if (file.empty()) {
			file = alias;
			alias = _T("");
		}

		boost::optional<boost::filesystem::wpath> ofile = find_file(file);
		if (!ofile)
			return false;
		script_container::push(scripts_, alias, *ofile);
		NSC_DEBUG_MSG_STD(_T("Adding script: ") + ofile->string() + _T(" as ") + alias + _T(")"));
		return true;
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Could not load script: (Unknown exception) ") + file);
	}
	return false;
}


bool LUAScript::unloadModule() {
	instances_.clear();
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
	registry->clear();
	BOOST_FOREACH(script_instance i, instances_) {
		try {
			i->reload();
		} catch (const lua_wrappers::LUAException &e) {
			error = true;
			message += _T("Exception when reloading script: ") + i->get_wscript() + _T(": ") + e.getMessage();
			NSC_LOG_ERROR_STD(_T("Exception when reloading script: ") + i->get_wscript() + _T(": ") + e.getMessage());
		} catch (...) {
			error = true;
			message += _T("Unhandeled Exception when reloading script: ") + i->get_wscript();
			NSC_LOG_ERROR_STD(_T("Unhandeled Exception when reloading script: ") + i->get_wscript());
		}
	}
	if (!error)
		message = _T("LUA scripts Reloaded...");
	return !error;
}



NSCAPI::nagiosReturn LUAScript::handleCommand(const std::wstring &target, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &message, std::wstring &perf) {
	if (command == _T("luareload")) {
		return reload(message)?NSCAPI::returnOK:NSCAPI::returnCRIT;
	}
	if (!registry->has_command(command))
		return NSCAPI::returnIgnored;
	return registry->on_query(target, command, arguments, message, perf);
		// const std::wstring &target, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &message, std::wstring &perf
}

NSCAPI::nagiosReturn LUAScript::commandLineExec(const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &result) {
	if (!registry->has_exec(command))
		return NSCAPI::returnIgnored;
	return registry->on_exec(command, arguments, result);
}

NSCAPI::nagiosReturn LUAScript::handleSimpleNotification(const std::wstring channel, const std::wstring source, const std::wstring command, NSCAPI::nagiosReturn code, std::wstring msg, std::wstring perf) {
	if (!registry->has_submit(channel))
		return NSCAPI::returnIgnored;
	return registry->on_submission(channel, source, command, code, msg, perf);
}




NSC_WRAP_DLL();
NSC_WRAPPERS_MAIN_DEF(LUAScript);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF();
NSC_WRAPPERS_CLI_DEF();
NSC_WRAPPERS_HANDLE_NOTIFICATION_DEF();
