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

NSC_WRAPPERS_MAIN();

class CheckNSCP : public nscapi::impl::SimpleCommand, public nscapi::impl::simple_plugin, public nscapi::impl::simple_log_handler {
private:
	MutexHandler mutex_;
	std::wstring crashFolder;
	typedef std::list<std::string> error_list;
	error_list errors_;
public:
	CheckNSCP();
	virtual ~CheckNSCP();
	// Module calls
	bool loadModule();
	bool loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();
	std::wstring getConfigurationMeta();


	std::wstring getModuleName() {
		return _T("Check NSCP");
	}
	nscapi::plugin_wrapper::module_version getModuleVersion() {
		nscapi::plugin_wrapper::module_version version = {0, 0, 1 };
		return version;
	}
	std::wstring getModuleDescription() {
		return _T("Checkes the state of " SZAPPNAME);
	}

	bool hasCommandHandler();
	bool hasMessageHandler();
	void handleMessage(int msgType, const std::string file, int line, std::string message);
	NSCAPI::nagiosReturn handleCommand(const strEx::wci_string command, std::list<std::wstring> arguments, std::wstring &message, std::wstring &perf);
	std::string render(int msgType, const std::string file, int line, std::string message);
	NSCAPI::nagiosReturn check_nscp( std::list<std::wstring> arguments, std::wstring & msg, std::wstring & perf );
	int get_crashes(std::wstring &last_crash);
	int get_errors(std::wstring &last_error);
};