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


LUAScript gLUAScript;

BOOL APIENTRY DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	NSCModuleWrapper::wrapDllMain(hModule, ul_reason_for_call);
	return TRUE;
}

LUAScript::LUAScript() {
}
LUAScript::~LUAScript() {
}


bool LUAScript::loadModule() {
	//LUA Scripts
	std::list<std::wstring> commands = NSCModuleHelper::getSettingsSection(LUA_SCRIPT_SECTION_TITLE);
	std::list<std::wstring>::const_iterator it;
	for (it = commands.begin(); it != commands.end(); ++it) {
		loadScript((*it));
	}
	return true;
}

void LUAScript::register_command(script_wrapper::lua_script* script, std::wstring command, std::wstring function) {
	NSC_LOG_MESSAGE(_T("Script loading: ") + script->get_script() + _T(": ") + command);
	strEx::blindstr bstr = command.c_str();
	commands_[bstr] = lua_func(script, function);
}

bool LUAScript::loadScript(const std::wstring file) {
	try {
		script_wrapper::lua_script *script = new script_wrapper::lua_script(file);
		script->pre_load(this);
		scripts_.push_back(script);
		return true;
	} catch (script_wrapper::LUAException e) {
		NSC_LOG_ERROR_STD(_T("Could not load script: ") + file + _T(", ") + e.getMessage());
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Could not load script: (Unknown exception) ") + file);
		//assert(false);
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



NSCAPI::nagiosReturn LUAScript::handleCommand(const strEx::blindstr command, const unsigned int argLen, TCHAR **char_args, std::wstring &msg, std::wstring &perf) {
	if (command == _T("LuaReload")) {
		return reload(msg)?NSCAPI::returnOK:NSCAPI::returnCRIT;
	}
	cmd_list::const_iterator cit = commands_.find(command);
	if (cit == commands_.end())
		return NSCAPI::returnIgnored;
	return (*cit).second.handleCommand(this, command, argLen, char_args, msg, perf);
}


NSC_WRAPPERS_MAIN_DEF(gLUAScript);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gLUAScript);
