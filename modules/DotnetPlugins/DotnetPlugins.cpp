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

const std::wstring settings_path = _T("/modules/dotnet");
const std::string module_path = "${exe-path}/modules/dotnet";
const std::wstring factory_key = _T("factory class");
const std::wstring factory_default = _T("NSCP.Plugin.PluginFactory");


bool DotnetPlugins::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {
// 	root_path = utf8::cvt<std::wstring>(get_core()->expand_path(utf8::cvt<std::string>(module_path)));
// 	get_core()->settings_register_path(get_id(), settings_path, _T("DOTNET MODULES"), _T("List all dot net modules loaded by the DotNetplugins module here"), false);
// 
// 	std::list<std::wstring> keys = get_core()->getSettingsSection(settings_path);
// 	BOOST_FOREACH(std::wstring key, keys) {
// 		get_core()->settings_register_key(get_id(), settings_path + _T("/") + key, factory_key, NSCAPI::key_string, _T("DOTNET FACTORY"), _T("The class to instasitate in the dot-net plugin"), factory_default, true);
// 	}
// 	if (mode == NSCAPI::normalStart) {
// 		BOOST_FOREACH(std::wstring key, keys) {
// 			load(key, get_core()->getSettingsString(settings_path, key, _T("")));
// 		}
// 	}
	return true;
}
bool DotnetPlugins::unloadModule() {
// 	BOOST_FOREACH(plugin_type p, plugins)
// 		p->unload();
// 	plugins.clear();
	return true;
}


void DotnetPlugins::load(std::wstring key, std::wstring val) {
// 	try {
// 		std::wstring alias = key;
// 		std::wstring plugin = val;
// 		if (val.empty())
// 			plugin = key;
// 		std::wstring factory = get_core()->getSettingsString(settings_path + _T("/") + alias, _T("factory class"), _T("NSCP.Plugin.PluginFactory"));
// 		plugin_type instance = plugin_instance::create(this, factory, plugin, root_path + _T("\\") + plugin);
// 		plugins.push_back(instance);
// 		instance->load(alias, 1);
// 	} catch(System::Exception ^e) {
// 		NSC_LOG_ERROR_STD("Failed to load module: " + to_nsstring(e->ToString()));
// 	} catch(const std::exception &e) {
// 		NSC_LOG_ERROR_STD("Failed to load module: " + utf8::utf8_from_native(e.what()));
// 	} catch(...) {
// 		NSC_LOG_ERROR_STD("CLR failed to load!");
// 	}
}
bool DotnetPlugins::settings_register_key(std::wstring path, std::wstring key, NSCAPI::settings_type type, std::wstring title, std::wstring description, std::wstring defaultValue, bool advanced) {
// 	get_core()->settings_register_key(id_, path, key, type, title, description, defaultValue, advanced);
	return true;
}
bool DotnetPlugins::settings_register_path(std::wstring path, std::wstring title, std::wstring description, bool advanced) {
// 	get_core()->settings_register_path(id_, path, title, description, advanced);
	return true;
}

bool DotnetPlugins::register_command(std::wstring command, plugin_instance::plugin_type plugin, std::wstring description){
// 	commands[command] = plugin;
// 	get_core()->registerCommand(id_, command, description);
	return true;
}
bool DotnetPlugins::register_channel(std::wstring channel, plugin_instance::plugin_type plugin) {
// 	channels[channel] = plugin;
// 	get_core()->registerSubmissionListener(id_, channel);
	return true;
}
nscapi::core_wrapper* DotnetPlugins::get_core() {
	return nscapi::plugin_singleton->get_core();
}

bool DotnetPlugins::hasCommandHandler() {
	return true;
}
bool DotnetPlugins::hasMessageHandler() {
	return false;
}
bool DotnetPlugins::hasNotificationHandler() {
	return false;
}


NSCAPI::nagiosReturn DotnetPlugins::handleRAWCommand(const std::string &char_command, const std::string &request, std::string &response) {
// 	std::wstring command(char_command);
// 	try {
// 		commands_type::const_iterator cit = commands.find(command);
// 		if (cit == commands.end())
// 			return NSCAPI::returnIgnored;
// 		return cit->second->onCommand(command, request, response);
// 	} catch(System::Exception ^e) {
// 		NSC_LOG_ERROR_STD("Failed to execute command " + utf8::cvt<std::string>(command) + ": " + to_nsstring(e->ToString()));
// 	} catch (const std::exception &e) {
// 		NSC_LOG_ERROR_STD("Failed to execute command " + utf8::cvt<std::string>(command), e);
// 	}
	return NSCAPI::returnIgnored;
}

NSCAPI::nagiosReturn DotnetPlugins::handleRAWNotification(const std::string &channel, std::string &request, std::string &response) {
// 	try {
// 		commands_type::const_iterator cit = channels.find(channel);
// 		if (cit == channels.end())
// 			return NSCAPI::returnIgnored;
// 		return cit->second->onSubmit(channel, request, response);
// 	} catch(System::Exception ^e) {
// 		NSC_LOG_ERROR_STD("Failed to execute command " + utf8::cvt<std::string>(channel) + ": " + to_nsstring(e->ToString()));
// 	} catch (const std::exception &e) {
// 		NSC_LOG_ERROR_STD("Failed to execute command " + utf8::cvt<std::string>(channel), e);
// 	}
	return NSCAPI::returnIgnored;
}

NSCAPI::nagiosReturn DotnetPlugins::commandRAWLineExec(const std::string &command, const std::string &request, std::string &response) {
	return NSCAPI::returnIgnored;
}

#pragma managed(push, off)
BOOL APIENTRY DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved) {
	return TRUE; 
}
#pragma managed(pop)
nscapi::helper_singleton* nscapi::plugin_singleton = new nscapi::helper_singleton();

NSC_WRAPPERS_MAIN_DEF(DotnetPlugins, "dotnet");
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF();
NSC_WRAPPERS_CLI_DEF();
NSC_WRAPPERS_HANDLE_NOTIFICATION_DEF();
