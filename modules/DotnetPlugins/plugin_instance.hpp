/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "Vcclr.h"

#include <clr/clr_scoped_ptr.hpp>

#include <string>
#include <boost/enable_shared_from_this.hpp>

typedef cli::array<System::Byte> protobuf_data;

std::string to_nstring(System::String^ s);
System::String^ to_mstring(const std::string &s);
std::string to_nstring(protobuf_data^ byteArray);
protobuf_data^ to_pbd(const std::string &buffer);

class plugin_manager_interface;
class internal_plugin_instance {
private:
	std::string dll;
	std::string type;
	gcroot<System::Type^> typeInstance;
	gcroot<NSCP::Core::IPlugin^> plugin;
	gcroot<NSCP::Core::PluginInstance^> instance;

public:

	internal_plugin_instance::internal_plugin_instance(std::string dll, std::string type) : dll(dll), type(type) {}

	bool load_dll(boost::shared_ptr<internal_plugin_instance> self, plugin_manager_interface *manager, std::string alias, int plugin_id);
	bool load_plugin(int mode);
	bool unload_plugin();

	int onCommand(std::string command, std::string request, std::string &response);
	int onSubmit(std::wstring channel, std::string request, std::string &response);

	virtual NSCP::Core::PluginInstance^ get_instance() {
		return instance;
	}
};

typedef boost::shared_ptr<internal_plugin_instance> internal_plugin_instance_ptr;

class plugin_manager_interface {
public:
	virtual bool register_command(std::string command, internal_plugin_instance_ptr plugin) = 0;
	virtual nscapi::core_wrapper* get_core() = 0;
};

ref class CoreImpl : public NSCP::Core::ICore {
private:
	plugin_manager_interface *manager;
	clr::clr_scoped_ptr<internal_plugin_instance_ptr> internal_instance;

	nscapi::core_wrapper* get_core();

public:
	CoreImpl(plugin_manager_interface *manager);

	virtual NSCP::Core::Result^ query(protobuf_data^ request);
	virtual NSCP::Core::Result^ exec(System::String^ target, protobuf_data^ request);
	virtual NSCP::Core::Result^ submit(System::String^ channel, protobuf_data^ request);
	virtual bool reload(System::String^ module);
	virtual NSCP::Core::Result^ settings(protobuf_data^ request);
	virtual NSCP::Core::Result^ registry(protobuf_data^ request);
	virtual void log(protobuf_data^ request);

	// Local
	virtual NSCP::Core::PluginInstance^ getInstance() {
		return (*internal_instance)->get_instance();
	}
	void set_instance(internal_plugin_instance_ptr newInstance) {
		internal_instance.reset(new internal_plugin_instance_ptr(newInstance));
	}
};

struct plugin_manager {
	virtual bool register_command(std::string command, internal_plugin_instance_ptr plugin) = 0;
	virtual nscapi::core_wrapper* get_core() = 0;
};