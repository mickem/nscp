/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "DotnetPlugins.h"
#include "plugin_instance.hpp"

#include <NSCAPI.h>
#include <nscapi/nscapi_plugin_wrapper.hpp>
#include <nscapi/nscapi_core_wrapper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>

#include <utf8.hpp>
#include <tchar.h>

#include <boost/foreach.hpp>

#include <stack>
#include <iostream>
#include <string>
#include <functional>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "Vcclr.h"

typedef DotnetPlugins plugin_impl_class;
static nscapi::plugin_instance_data<plugin_impl_class> plugin_instance;
extern int NSModuleHelperInit(unsigned int, nscapi::core_api::lpNSAPILoader f) {
	return nscapi::basic_wrapper_static<plugin_impl_class>::NSModuleHelperInit(f);
}
extern int NSLoadModuleEx(unsigned int id, char* alias, int mode) {
	try {
		nscapi::basic_wrapper_static<plugin_impl_class>::set_alias("dotnet", alias);
		nscapi::basic_wrapper<plugin_impl_class> wrapper(plugin_instance.get(id));
		return wrapper.NSLoadModuleExNoExcept(id, alias, mode);
	} catch (System::Exception^ e) {
		NSC_LOG_ERROR("Exception in NSLoadModuleEx: " + to_nstring(e->Message));
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("NSLoadModuleEx", e);
	} catch (...) {
		NSC_LOG_CRITICAL("Unknown exception in: NSLoadModuleEx");
	}
	return NSCAPI::api_return_codes::hasFailed;
}
extern int NSLoadModule() {
	return nscapi::basic_wrapper_static<plugin_impl_class>::NSLoadModule();
}
extern int NSGetModuleName(char* buf, int buflen) { return nscapi::basic_wrapper_static<plugin_impl_class>::NSGetModuleName(buf, buflen); }
extern int NSGetModuleDescription(char* buf, int buflen) { return nscapi::basic_wrapper_static<plugin_impl_class>::NSGetModuleDescription(buf, buflen); }
extern int NSGetModuleVersion(int *major, int *minor, int *revision) { return nscapi::basic_wrapper_static<plugin_impl_class>::NSGetModuleVersion(major, minor, revision); }
extern int NSUnloadModule(unsigned int id) {
	int ret;
	{
		nscapi::basic_wrapper<plugin_impl_class> wrapper(plugin_instance.get(id));
		ret = wrapper.NSUnloadModule();
	}
	plugin_instance.erase(id);
	return ret;
}
extern void NSDeleteBuffer(char**buffer) { nscapi::basic_wrapper_static<plugin_impl_class>::NSDeleteBuffer(buffer); }

extern void NSHandleMessage(unsigned int id, const char* request_buffer, unsigned int request_buffer_len) {
	nscapi::message_wrapper<plugin_impl_class> wrapper(plugin_instance.get(id));
	return wrapper.NSHandleMessage(request_buffer, request_buffer_len);
}
extern NSCAPI::boolReturn NSHasMessageHandler(unsigned int id) {
	nscapi::message_wrapper<plugin_impl_class> wrapper(plugin_instance.get(id));
	return wrapper.NSHasMessageHandler();
}
extern NSCAPI::nagiosReturn NSHandleCommand(unsigned int id, const char* request_buffer, const unsigned int request_buffer_len, char** reply_buffer, unsigned int *reply_buffer_len) {
	nscapi::command_wrapper<plugin_impl_class> wrapper(plugin_instance.get(id));
	return wrapper.NSHandleCommand(request_buffer, request_buffer_len, reply_buffer, reply_buffer_len);
}
extern NSCAPI::boolReturn NSHasCommandHandler(unsigned int id) {
	nscapi::command_wrapper<plugin_impl_class> wrapper(plugin_instance.get(id));
	return wrapper.NSHasCommandHandler();
}
extern int NSCommandLineExec(unsigned int id, int target_mode, char *request_buffer, unsigned int request_len, char **response_buffer, unsigned int *response_len) {
	nscapi::cliexec_wrapper<plugin_impl_class> wrapper(plugin_instance.get(id));
	return wrapper.NSCommandLineExec(target_mode, request_buffer, request_len, response_buffer, response_len);
}
extern int NSHandleNotification(unsigned int id, const char* channel, const char* buffer, unsigned int buffer_len, char** response_buffer, unsigned int *response_buffer_len) {
	nscapi::submission_wrapper<plugin_impl_class> wrapper(plugin_instance.get(id));
	return wrapper.NSHandleNotification(channel, buffer, buffer_len, response_buffer, response_buffer_len);
}
extern NSCAPI::boolReturn NSHasNotificationHandler(unsigned int id) {
	nscapi::submission_wrapper<plugin_impl_class> wrapper(plugin_instance.get(id));
	return wrapper.NSHasNotificationHandler();
}

const std::string settings_path = "/modules/dotnet";
const std::string module_path = "${exe-path}/modules/dotnet";
const std::string factory_key = "factory class";
const std::string factory_default = "NSCP.Plugin.PluginFactory";

using namespace Plugin;

int DotnetPlugins::registry_reg_module(const std::string module) {
	RegistryRequestMessage::Builder^ message_builder = RegistryRequestMessage::CreateBuilder();
	RegistryRequestMessage::Types::Request::Types::Registration::Builder^ query_builder = RegistryRequestMessage::Types::Request::Types::Registration::CreateBuilder();
	query_builder->SetName(to_mstring(module));
	query_builder->SetPluginId(get_id());
	query_builder->SetType(Registry::Types::ItemType::MODULE);
	message_builder->AddPayload(RegistryRequestMessage::Types::Request::CreateBuilder()->SetRegistration(query_builder->Build())->Build());
	System::IO::MemoryStream^ stream = gcnew System::IO::MemoryStream();
	message_builder->Build()->WriteTo(stream);
	std::string response_buffer;
	if (!get_core()->registry_query(to_nstring(stream->ToArray()), response_buffer)) {
		NSC_LOG_ERROR("Failed to register: " + module);
		return 0;
	}
	RegistryResponseMessage^ response_message = RegistryResponseMessage::ParseFrom(to_pbd(response_buffer));
	if (response_message->GetPayload(0)->Result->Code != Common::Types::Result::Types::StatusCodeType::STATUS_OK) {
		NSC_LOG_ERROR("Failed to register: " + module);
		return 0;
	}
	return response_message->GetPayload(0)->Registration->ItemId;
}

bool DotnetPlugins::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {
	root_path = get_core()->expand_path(utf8::cvt<std::string>(module_path));
	NSCP::Helpers::SettingsHelper^ settings = gcnew NSCP::Helpers::SettingsHelper(gcnew CoreImpl(this), get_id());

	settings->registerPath(to_mstring(settings_path), "DOT NET MODULES", "Modules written in dotnet/CLR", false);

	for each (System::String^ s in settings->getKeys("/modules/dotnet")) {
		settings->registerKey(to_mstring(settings_path), s, 0, "DOT NET Module", "dotnet plugin", "", false);
		std::string v = to_nstring(settings->getString(to_mstring(settings_path), s, ""));
		std::string factory = to_nstring(settings->getString(to_mstring(settings_path + "/" + to_nstring(s)), to_mstring(factory_key), to_mstring(factory_default)));
		if (mode == NSCAPI::normalStart) {
			load(to_nstring(s), factory, v);
		}
	}
	return true;
}
bool DotnetPlugins::unloadModule() {
	BOOST_FOREACH(internal_plugin_instance_ptr p, plugins) {
		unsigned int id = p->get_instance()->PluginID;
		p->unload_plugin();
		plugin_instance.erase(id);
	}
	plugins.clear();
	return true;
}

bool file_exists(const TCHAR * file) {
	WIN32_FIND_DATA FindFileData;
	HANDLE handle = FindFirstFile(file, &FindFileData);
	bool found = handle != INVALID_HANDLE_VALUE;
	if (found)
		FindClose(handle);
	return found;
}
void DotnetPlugins::load(std::string key, std::string factory, std::string val) {
	try {
		std::string alias = key;
		std::string plugin = val;
		if (val == "enabled") {
			plugin = alias;
			alias = std::string();
		}
		if (val.empty())
			plugin = alias;
		std::string ppath = root_path + "\\" + plugin;
		if (!file_exists(utf8::cvt<std::wstring>(ppath).c_str())) {
			ppath = root_path + "\\" + plugin + ".dll";
			if (!file_exists(utf8::cvt<std::wstring>(ppath).c_str())) {
				NSC_LOG_ERROR("Plugin not found: " + plugin);
				return;
			}
		}
		int id = registry_reg_module(key);
		plugin_instance.add_alias(get_id(), id);
		internal_plugin_instance_ptr instance(new internal_plugin_instance(ppath, factory));
		plugins.push_back(instance);
		instance->load_dll(instance, this, alias, id);
		instance->load_plugin(1); // TODO: Fix correct load level
	} catch (System::Exception ^e) {
		NSC_LOG_ERROR_STD("Failed to load module: " + to_nstring(e->ToString()));
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_STD("Failed to load module: " + utf8::utf8_from_native(e.what()));
	} catch (...) {
		NSC_LOG_ERROR_STD("CLR failed to load!");
	}
}
bool DotnetPlugins::register_command(std::string command, internal_plugin_instance_ptr plugin) {
	commands[command] = plugin;
	return true;
}
bool DotnetPlugins::register_channel(std::wstring channel, internal_plugin_instance_ptr plugin) {
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

NSCAPI::nagiosReturn DotnetPlugins::handleRAWCommand(const std::string &request, std::string &response) {
	QueryRequestMessage^ msg = QueryRequestMessage::ParseFrom(to_pbd(request));
	std::string command = to_nstring(msg->GetPayload(0)->Command);
	try {
		commands_type::const_iterator cit = commands.find(command);
		if (cit == commands.end())
			return NSCAPI::cmd_return_codes::returnIgnored;
		return cit->second->onCommand(command, request, response);
	} catch (System::Exception ^e) {
		NSC_LOG_ERROR_STD("Failed to execute command " + command + ": " + to_nstring(e->ToString()));
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_STD("Failed to execute command " + command, e);
	}
	return NSCAPI::cmd_return_codes::hasFailed;
}

void DotnetPlugins::handleMessageRAW(std::string data) {}

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
	return NSCAPI::cmd_return_codes::returnIgnored;
}

NSCAPI::nagiosReturn DotnetPlugins::commandRAWLineExec(const int target_type, const std::string &request, std::string &response) {
	return NSCAPI::cmd_return_codes::returnIgnored;
}

#pragma managed(push, off)
BOOL APIENTRY DllMain(HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved) {
	return TRUE;
}
#pragma managed(pop)
nscapi::helper_singleton* nscapi::plugin_singleton = new nscapi::helper_singleton();