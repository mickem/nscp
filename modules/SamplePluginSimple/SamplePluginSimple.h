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
// Wrapper which declares the C API functions
NSC_WRAPPERS_MAIN()

// Your implementation class can derive from various helper implementations
// simple_plugin			- Hides ID handling in your plugin and allows you to register and access the various cores.
// simple_command_handler	- Provides a "nagios plugin" like command handler interface (so you wont have to deal with google protocl buffers)
// There is a bunch of others as well for wrapping the other APIs
class SamplePluginSimple : public nscapi::impl::simple_command_handler, public nscapi::impl::simple_plugin {
private:

public:
	SamplePluginSimple();
	virtual ~SamplePluginSimple();

	static std::wstring getModuleName() {
		return _T("Sample plugin");
	}
	static nscapi::plugin_wrapper::module_version getModuleVersion() {
		nscapi::plugin_wrapper::module_version version = {0, 3, 0 };
		return version;
	}
	static std::wstring getModuleDescription() {
		return _T("A sample plugin to display how to make plugins...");
	}

	// Declare exposed API methods (C++ versions)
	bool loadModule();
	bool loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();
	bool hasCommandHandler();
	NSCAPI::nagiosReturn handleCommand(const std::wstring &target, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &message, std::wstring &perf);

};