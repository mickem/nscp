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
#include "SamplePluginSimple.h"

// Empty construcor is called by the infrastructure when our plugin is loaded (once for each instance).
SamplePluginSimple::SamplePluginSimple() {}
SamplePluginSimple::~SamplePluginSimple() {}

// This is to support legacy plugins and will be removed in the near future.
bool SamplePluginSimple::loadModule() {
	return false;
}
// This is the load operation called when your plugin is offically loaded.
// The alias is mainly used to differentiate if the plugin is loaded more then once.
// A good approach is to use this (if it is not empty) when loading settings values to allow multiple instance to co-exist nicely.
// The mode is a bit undefined the idea is to allow the plugin to be loaded in offline mode.
// It is generally used to "not start servers" now.
// The load call should be fairly quick as this is done (currently) in serial.
// The main goal is to load your settings, register your self and start any servers
bool SamplePluginSimple::loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode) {
	try {
		// register a command handler
		register_command(_T("check_sample"), _T("A Sample command which does nothing..."));
	} catch (std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Exception: ") + utf8::cvt<std::wstring>(e.what()));
		return false;
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to register command."));
		return false;
	}
	return true;
}
// Called when the plugin is unloaded
// kill off any active server instance here, unregister your self and freeup all memory.
// Notice reload is currently implemented as unload/load so it is very possible that your plugin will be loaded and unloaded multiple times.
// Do not assume it is safe to forget freeing up memory and destroying resources here
bool SamplePluginSimple::unloadModule() {
	return true;
}
// This is called at startup to register your plugin with the command handler.
// Return true if you can handle commands
bool SamplePluginSimple::hasCommandHandler() {
	return true;
}
// Called when ANY of your commands are executed.
// Notice that message and perf are references meaning anything you update them with will be returned.
NSCAPI::nagiosReturn SamplePluginSimple::handleCommand(const std::wstring &target, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &message, std::wstring &perf) {
	if (command == _T("check_sample")) {
		message = _T("Yaaay: Hello World!");
		return NSCAPI::returnOK;
	}
	return NSCAPI::returnIgnored;
}

// Macros to create wrappers around the C API towards you C++ class.
// If you want to implement more features you can add more "parts" of the API.
// For instance NSC_WRAPPERS_ROUTING_DEF will add routing support and call routing handlers in your class.
NSC_WRAP_DLL()
NSC_WRAPPERS_MAIN_DEF(SamplePluginSimple, _T("sample"))
NSC_WRAPPERS_IGNORE_MSG_DEF()
NSC_WRAPPERS_HANDLE_CMD_DEF()
