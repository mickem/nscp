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
NSC_WRAPPERS_MAIN()
NSC_WRAPPERS_CLI()
NSC_WRAPPERS_CHANNELS()

#include <boost/optional.hpp>
#include <boost/scoped_ptr.hpp>

//#include <config.h>
#include <strEx.h>
#include <utils.h>
#include <scripts/functions.hpp>
#include <scripts/script_interface.hpp>
#include <scripts/script_nscp.hpp>

#include <lua/lua_script.hpp>
#include <lua/lua_core.hpp>

class LUAScript : public nscapi::impl::simple_command_line_exec, public nscapi::impl::simple_submission_handler, public nscapi::impl::simple_plugin {
private:
	boost::scoped_ptr<scripts::script_manager<lua::lua_traits> > scripts_;
	boost::shared_ptr<lua::lua_runtime> lua_runtime_;
	boost::shared_ptr<scripts::nscp::nscp_runtime_impl> nscp_runtime_;
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
	bool hasNotificationHandler() { return true; }
	bool loadScript(std::wstring alias, std::wstring file);
	NSCAPI::nagiosReturn execute_and_load(std::list<std::wstring> args, std::wstring &message);
	NSCAPI::nagiosReturn handleSimpleNotification(const std::wstring channel, const std::wstring source, const std::wstring command, NSCAPI::nagiosReturn code, std::wstring msg, std::wstring perf);
	NSCAPI::nagiosReturn handleRAWCommand(const wchar_t* char_command, const std::string &request, std::string &response);
	NSCAPI::nagiosReturn commandLineExec(const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &result);
};
