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

#include <types.hpp>

#include <string>
#include <functional>

#include <NSCAPI.h>
#include <nscapi/nscapi_plugin_wrapper.hpp>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <list>

#include <utf8.hpp>

#include "plugin_instance.hpp"

#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>

using namespace System;
using namespace System::IO;
using namespace System::Reflection;
using namespace System::Collections::Generic;
using namespace System::Runtime::InteropServices;

using namespace Plugin;

std::string to_nstring(System::String^ s) {
	pin_ptr<const wchar_t> pinString = PtrToStringChars(s);
	return utf8::cvt<std::string>(std::wstring(pinString));
}

std::string to_nstring(protobuf_data^ request) {
	char *buffer = new char[request->Length + 1];
	memset(buffer, 0, request->Length + 1);
	Marshal::Copy(request, 0, IntPtr(buffer), (int)request->Length);
	std::string ret(buffer, (std::string::size_type)request->Length);
	delete[] buffer;
	return ret;
}

protobuf_data^ to_pbd(const std::string &buffer) {
	protobuf_data^ arr = gcnew protobuf_data(buffer.size());
	Marshal::Copy(IntPtr(const_cast<char*>(buffer.c_str())), arr, 0, arr->Length);
	return arr;
}

System::String^ to_mstring(const std::string &s) {
	return gcnew System::String(utf8::cvt<std::wstring>(s).c_str());
}
System::String^ to_mstring(const std::wstring &s) {
	return gcnew System::String(s.c_str());
}

array<String^>^ to_mlist(std::list<std::string> list) {
	array<System::String^>^ ret = gcnew array<System::String^>(list.size());
	typedef std::list<std::string>::const_iterator iter_t;
	int j = 0;

	for (iter_t i = list.begin(); i != list.end(); ++i)
		ret[j++] = gcnew System::String(utf8::cvt<std::wstring>(*i).c_str());
	return ret;
};

nscapi::core_wrapper* CoreImpl::get_core() {
	if (manager == NULL)
		throw gcnew System::Exception("Uninitialized core");
	return manager->get_core();
}

CoreImpl::CoreImpl(plugin_manager_interface *manager) : manager(manager) {}

NSCP::Core::Result^ CoreImpl::query(protobuf_data^ request) {
	NSCP::Core::Result^ ret = gcnew NSCP::Core::Result();
	std::string response;
	ret->result = get_core()->query(to_nstring(request), response);
	ret->data = to_pbd(response);
	return ret;
}
NSCP::Core::Result^ CoreImpl::exec(String^ target, protobuf_data^ request) {
	NSCP::Core::Result^ ret = gcnew NSCP::Core::Result();
	std::string response;
	ret->result = get_core()->exec_command(to_nstring(target), to_nstring(request), response);
	ret->data = to_pbd(response);
	return ret;
}
NSCP::Core::Result^ CoreImpl::submit(String^ channel, protobuf_data^ request) {
	NSCP::Core::Result^ ret = gcnew NSCP::Core::Result();
	std::string response;
	ret->result = get_core()->submit_message(to_nstring(channel), to_nstring(request), response);
	ret->data = to_pbd(response);
	return ret;
}
bool CoreImpl::reload(String^ module) {
	return get_core()->reload(to_nstring(module)) == NSCAPI::api_return_codes::isSuccess;
}
NSCP::Core::Result^ CoreImpl::settings(protobuf_data^ request) {
	NSCP::Core::Result^ ret = gcnew NSCP::Core::Result();
	std::string response;
	ret->result = get_core()->settings_query(to_nstring(request), response);
	ret->data = to_pbd(response);
	return ret;
}
NSCP::Core::Result^ CoreImpl::registry(protobuf_data^ request) {
	RegistryRequestMessage^ msg = RegistryRequestMessage::ParseFrom(request);
	for (int i = 0; i < msg->PayloadCount; i++) {
		if (msg->GetPayload(i)->HasRegistration) {
			RegistryRequestMessage::Types::Request::Types::Registration^ reg = msg->GetPayload(i)->Registration;
			if (reg->Type == Registry::Types::ItemType::QUERY) {
				std::string command = to_nstring(reg->Name);
				manager->register_command(command, (*internal_instance));
			}
		}
	}
	NSCP::Core::Result^ ret = gcnew NSCP::Core::Result();
	std::string response;
	ret->result = get_core()->registry_query(to_nstring(request), response);
	ret->data = to_pbd(response);
	return ret;
}
void CoreImpl::log(protobuf_data^ request) {
	get_core()->log(to_nstring(request));
}

//////////////////////////////////////////////////////////////////////////

bool internal_plugin_instance::load_dll(internal_plugin_instance_ptr self, plugin_manager_interface *manager, std::string alias, int plugin_id) {
	gcroot<NSCP::Core::IPluginFactory^> factory;
	try {
		System::Reflection::Assembly^ dllAssembly = System::Reflection::Assembly::LoadFrom(to_mstring(dll));
		typeInstance = dllAssembly->GetType(to_mstring(type));
		if (!typeInstance) {
			NSC_LOG_ERROR_STD("Failed to load plugin factory (" + type + ") from " + dll);
			factory = nullptr;
			plugin = nullptr;
			return false;
		}
		factory = (NSCP::Core::IPluginFactory^)Activator::CreateInstance(typeInstance);
		if (!factory) {
			NSC_LOG_ERROR_STD("No factory for: " + dll);
			return false;
		}
		CoreImpl^ core = gcnew CoreImpl(manager);
		core->set_instance(self);
		instance = gcnew NSCP::Core::PluginInstance(plugin_id, to_mstring(alias));
		plugin = factory->create(core, instance);
	} catch (System::Exception ^e) {
		NSC_LOG_ERROR_STD("Failed to create instance of " + dll + "/" + type + ": " + to_nstring(e->ToString()));
		return false;
	}
	return true;
}

bool internal_plugin_instance::load_plugin(int mode) {
	if (!plugin) {
		NSC_LOG_ERROR_STD("Plugin failed to load");
		return false;
	}
	plugin->load(mode);
	return true;
}

bool internal_plugin_instance::unload_plugin() {
	if (!plugin) {
		NSC_LOG_ERROR_STD("Plugin failed to load");
		return false;
	}
	plugin->unload();
	return true;
}

int internal_plugin_instance::onCommand(std::string command, std::string request, std::string &response) {
	NSCP::Core::IQueryHandler^ handler = plugin->getQueryHandler();
	if (!handler->isActive())
		return NSCAPI::cmd_return_codes::returnIgnored;
	NSCP::Core::Result^ result = handler->onQuery(to_mstring(command), to_pbd(request));
	response = to_nstring(result->data);
	return result->result;
}

int internal_plugin_instance::onSubmit(std::wstring channel, std::string request, std::string &response) {
	NSCP::Core::ISubmissionHandler ^handler = plugin->getSubmissionHandler();
	if (!handler->isActive())
		return NSCAPI::cmd_return_codes::returnIgnored;
	NSCP::Core::Result^ result = handler->onSubmission(to_mstring(channel), to_pbd(request));
	response = to_nstring(result->data);
	return result->result;
}