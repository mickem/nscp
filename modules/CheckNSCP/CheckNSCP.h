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
#pragma once

#include <config.h>

#include <string>

//#include <NSCAPI.h>
#include <nscapi/plugin.hpp>

#include <boost/thread/thread.hpp>
#include <boost/thread/locks.hpp>

NSC_WRAPPERS_MAIN();

class CheckNSCP : public nscapi::impl::simple_command, public nscapi::impl::simple_plugin, public nscapi::impl::simple_log_handler {
private:
	boost::timed_mutex mutex_;
	std::wstring crashFolder;
	typedef std::list<std::string> error_list;
	error_list errors_;
public:
	// Module calls
	bool loadModule();
	bool loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();
	std::wstring getConfigurationMeta();


	static std::wstring getModuleName() {
		return _T("Check NSCP");
	}
	static nscapi::plugin_wrapper::module_version getModuleVersion() {
		nscapi::plugin_wrapper::module_version version = {0, 0, 1 };
		return version;
	}
	static std::wstring getModuleDescription() {
		return _T("Checkes the state of the agent");
	}

	bool hasCommandHandler();
	bool hasMessageHandler();
	void handleMessage(int msgType, const std::string file, int line, std::string message);
	NSCAPI::nagiosReturn handleCommand(const std::wstring &target, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &message, std::wstring &perf);
	std::string render(int msgType, const std::string file, int line, std::string message);
	NSCAPI::nagiosReturn check_nscp( std::list<std::wstring> arguments, std::wstring & msg, std::wstring & perf );
	int get_crashes(std::wstring &last_crash);
	int get_errors(std::wstring &last_error);
};