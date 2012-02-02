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
//#include <config.h>
#include <strEx.h>
#include <utils.h>
#include <checkHelpers.hpp>
#include "script_wrapper.hpp"

#include <boost/optional.hpp>

#include <scripts/functions.hpp>


class LUAScript : public nscapi::impl::simple_command_handler, public nscapi::impl::simple_plugin {
private:

	boost::shared_ptr<lua_wrappers::lua_registry> registry;
	script_container::list_type scripts_;
	typedef boost::shared_ptr<script_wrapper::lua_script> script_instance;
	typedef std::list<script_instance> script_list;
	script_list instances_;
	boost::filesystem::wpath root_;

public:
	LUAScript();
	virtual ~LUAScript();
	// Module calls
	bool loadModule();
	bool loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode);

	bool unloadModule();
	bool reload(std::wstring &msg);

	static std::wstring getModuleName() {
		return _T("LUAScript");
	}
	static std::wstring getModuleDescription() {
		return _T("LUAScript...");
	}
	static nscapi::plugin_wrapper::module_version getModuleVersion() {
		nscapi::plugin_wrapper::module_version version = {0, 0, 1 };
		return version;
	}

	bool hasCommandHandler();
	bool hasMessageHandler();
	boost::optional<boost::filesystem::wpath> find_file(std::wstring file);
	bool loadScript(std::wstring alias, std::wstring file);
	NSCAPI::nagiosReturn handleCommand(const std::wstring &target, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &message, std::wstring &perf);
	//NSCAPI::nagiosReturn RunLUA(const unsigned int argLen, wchar_t **char_args, std::wstring &message, std::wstring &perf);
	//NSCAPI::nagiosReturn extract_return(Lua_State &L, int arg_count,  std::wstring &message, std::wstring &perf);

};
