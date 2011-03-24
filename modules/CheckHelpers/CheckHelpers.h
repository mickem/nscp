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

class CheckHelpers : public nscapi::impl::SimpleCommand, nscapi::impl::simple_plugin {
private:

public:
	CheckHelpers();
	virtual ~CheckHelpers();
	// Module calls
	bool loadModule();
	bool loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();


	std::wstring getModuleName() {
		return _T("Helper function");
	}
	nscapi::plugin_wrapper::module_version getModuleVersion() {
		nscapi::plugin_wrapper::module_version version = {0, 3, 0 };
		return version;
	}
	std::wstring getModuleDescription() {
		return _T("Various helper function to extend other checks.\nThis is also only supported through NRPE.");
	}

	bool hasCommandHandler();
	bool hasMessageHandler();
	NSCAPI::nagiosReturn handleCommand(const std::wstring command, std::list<std::wstring> arguments, std::wstring &message, std::wstring &perf);

	// Check commands
	NSCAPI::nagiosReturn checkMultiple(const std::list<std::wstring> arguments, std::wstring &message, std::wstring &perf);
	NSCAPI::nagiosReturn checkSimpleStatus(NSCAPI::nagiosReturn status, const std::list<std::wstring> arguments, std::wstring &message, std::wstring &perf);
	NSCAPI::nagiosReturn checkSimpleStatus(NSCAPI::nagiosReturn status, const unsigned int argLen, TCHAR **char_args, std::wstring &msg, std::wstring &perf);
	NSCAPI::nagiosReturn timeout(std::list<std::wstring> arguments, std::wstring &msg, std::wstring &perf);
	NSCAPI::nagiosReturn negate(std::list<std::wstring> arguments, std::wstring &msg, std::wstring &perf);
};