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
#include "DotnetPlugins.h"

#include <strEx.h>

#include "Vcclr.h"


extern nscapi::helper_singleton* nscapi::plugin_singleton;

bool DotnetPlugin::loadModule() {
	return false;
}

const std::wstring settings_path = _T("/modules/dotnet");
const std::wstring module_path = _T("${exe-path}/modules/dotnet");
const std::wstring factory_key = _T("factory class");
const std::wstring factory_default = _T("NSCP.Plugin.PluginFactory");


bool DotnetPlugin::loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode) {

	try {
		root_path = get_core()->expand_path(module_path);
		get_core()->settings_register_path(settings_path, _T("DOTNET MODULES"), _T("List all dot net modules loaded by the DotNetplugins module here"), false);

		std::list<std::wstring> keys = get_core()->getSettingsSection(settings_path);
		BOOST_FOREACH(std::wstring key, keys) {
			get_core()->settings_register_key(settings_path + _T("/") + key, factory_key, NSCAPI::key_string, _T("DOTNET FACTORY"), _T("The class to instasitate in the dot-net plugin"), factory_default, true);
		}
		if (mode == NSCAPI::normalStart) {
			BOOST_FOREACH(std::wstring key, keys) {
				load(key, get_core()->getSettingsString(settings_path, key, _T("")));
			}
		}
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Exception caught: <UNKNOWN EXCEPTION>"));
		return false;
	}
	return true;
}
bool DotnetPlugin::unloadModule() {
	BOOST_FOREACH(plugin_type p, plugins)
		p->unload();
	plugins.clear();
	return true;
}


void DotnetPlugin::load(std::wstring key, std::wstring val) {
	try {
		std::wstring alias = key;
		std::wstring plugin = val;
		if (val.empty())
			plugin = key;
		std::wstring factory = get_core()->getSettingsString(settings_path + _T("/") + alias, _T("factory class"), _T("NSCP.Plugin.PluginFactory"));
		plugin_type instance = plugin_instance::create(this, factory, plugin, root_path + _T("\\") + plugin);
		plugins.push_back(instance);
		instance->load(alias, 1);
	} catch(System::Exception ^e) {
		NSC_LOG_ERROR_STD(_T("Failed to load module: ") + to_nstring(e->ToString()));
	} catch(const std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Failed to load module: ") + utf8::to_unicode(e.what()));
	} catch(...) {
		NSC_LOG_ERROR_STD(_T("CLR failed to load!"));
	}
}

bool DotnetPlugin::register_command(std::wstring command, plugin_instance::plugin_type plugin, std::wstring description){
	commands[command] = plugin;
	get_core()->registerCommand(id_, command, description);
	return true;
}
bool DotnetPlugin::register_channel(std::wstring channel, plugin_instance::plugin_type plugin) {
	channels[channel] = plugin;
	get_core()->registerSubmissionListener(id_, channel);
	return true;
}
nscapi::core_wrapper* DotnetPlugin::get_core() {
	return nscapi::plugin_singleton->get_core();
}

bool DotnetPlugin::hasCommandHandler() {
	return true;
}
bool DotnetPlugin::hasMessageHandler() {
	return false;
}
bool DotnetPlugin::hasNotificationHandler() {
	return false;
}


NSCAPI::nagiosReturn DotnetPlugin::handleRAWCommand(const wchar_t* char_command, const std::string &request, std::string &response) {
	std::wstring command(char_command);
	try {
		commands_type::const_iterator cit = commands.find(command);
		if (cit == commands.end())
			return NSCAPI::returnIgnored;
		return cit->second->onCommand(command, request, response);
	} catch(System::Exception ^e) {
		NSC_LOG_ERROR_STD(_T("Failed to execute command ") + command + _T(": ") + to_nstring(e->ToString()));
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Failed to execute command ") + command + _T(": ") + utf8::to_unicode(e.what()));
	}
	return NSCAPI::returnIgnored;
}

NSCAPI::nagiosReturn DotnetPlugin::handleRAWNotification(const std::wstring &channel, std::string &request, std::string &response) {
	try {
		commands_type::const_iterator cit = channels.find(channel);
		if (cit == channels.end())
			return NSCAPI::returnIgnored;
		return cit->second->onSubmit(channel, request, response);
	} catch(System::Exception ^e) {
		NSC_LOG_ERROR_STD(_T("Failed to execute command ") + channel + _T(": ") + to_nstring(e->ToString()));
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Failed to execute command ") + channel + _T(": ") + utf8::to_unicode(e.what()));
	}
	return NSCAPI::returnIgnored;
}

NSCAPI::nagiosReturn DotnetPlugin::commandRAWLineExec(const wchar_t* char_command, const std::string &request, std::string &response) {
	std::wstring command = char_command;
	return NSCAPI::returnIgnored;
}

#pragma managed(push, off)
BOOL APIENTRY DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved) {
	return TRUE; 
}
#pragma managed(pop)
nscapi::helper_singleton* nscapi::plugin_singleton = new nscapi::helper_singleton();

NSC_WRAPPERS_MAIN_DEF(DotnetPlugin);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF();
NSC_WRAPPERS_CLI_DEF();
NSC_WRAPPERS_HANDLE_NOTIFICATION_DEF();
