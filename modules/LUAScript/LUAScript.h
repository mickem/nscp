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
NSC_WRAPPERS_MAIN();
#include <config.h>
#include <strEx.h>
#include <utils.h>
#include <checkHelpers.hpp>
#include "script_wrapper.hpp"

class LUAScript : public nscapi::impl::SimpleCommand, public script_wrapper::lua_handler, public nscapi::impl::simple_plugin {
private:

	class lua_func {
	public:
		lua_func(script_wrapper::lua_script* script_, std::wstring function_) : script(script_), function(function_) {}
		lua_func() : script(NULL) {}
		script_wrapper::lua_script* script;
		std::wstring function;

		NSCAPI::nagiosReturn handleCommand(lua_handler *handler, std::wstring command, std::list<std::wstring> arguments, std::wstring &msg, std::wstring &perf) const {
			return script->handleCommand(handler, function, command, arguments, msg, perf);
		}
	};

	typedef std::map<std::wstring,lua_func> cmd_list;
	typedef std::list<script_wrapper::lua_script*> script_list;

	cmd_list commands_;
	script_list scripts_;
	std::wstring root_;

public:
	LUAScript();
	virtual ~LUAScript();
	// Module calls
	bool loadModule();
	bool loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode);

	bool unloadModule();
	bool reload(std::wstring &msg);

	std::wstring getModuleName() {
		return _T("LUAScript");
	}
	std::wstring getModuleDescription() {
		return _T("LUAScript...");
	}
	nscapi::plugin_wrapper::module_version getModuleVersion() {
		nscapi::plugin_wrapper::module_version version = {0, 0, 1 };
		return version;
	}

	bool hasCommandHandler();
	bool hasMessageHandler();
	bool loadScript(const std::wstring script);
	NSCAPI::nagiosReturn handleCommand(const std::wstring command, std::list<std::wstring> arguments, std::wstring &message, std::wstring &perf);
	//NSCAPI::nagiosReturn RunLUA(const unsigned int argLen, TCHAR **char_args, std::wstring &message, std::wstring &perf);
	//NSCAPI::nagiosReturn extract_return(Lua_State &L, int arg_count,  std::wstring &message, std::wstring &perf);

	//script_wrapper::lua_handler
	void register_command(script_wrapper::lua_script* script, std::wstring command, std::wstring function);

private:
	typedef checkHolders::CheckContainer<checkHolders::MaxMinBoundsDiscSize> PathContainer;
	typedef checkHolders::CheckContainer<checkHolders::MaxMinPercentageBoundsDiskSize> DriveContainer;
};
