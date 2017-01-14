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

#pragma once

#include <NSCAPI.h>
#include <nscapi/nscapi_plugin_wrapper.hpp>

#include <map>

extern "C" int NSModuleHelperInit(unsigned int id, nscapi::core_api::lpNSAPILoader f);
extern "C" int NSLoadModule();
extern "C" int NSLoadModuleEx(unsigned int plugin_id, char* alias, int mode);
extern "C" void NSDeleteBuffer(char**buffer);
extern "C" int NSGetModuleName(char* buf, int buflen);
extern "C" int NSGetModuleDescription(char* buf, int buflen);
extern "C" int NSGetModuleVersion(int *major, int *minor, int *revision);
extern "C" NSCAPI::boolReturn NSHasCommandHandler(unsigned int plugin_id);
extern "C" NSCAPI::boolReturn NSHasMessageHandler(unsigned int plugin_id);
extern "C" void NSHandleMessage(unsigned int plugin_id, const char* data, unsigned int len);
extern "C" NSCAPI::nagiosReturn NSHandleCommand(unsigned int plugin_id, const char* request_buffer, const unsigned int request_buffer_len, char** reply_buffer, unsigned int *reply_buffer_len);
extern "C" int NSUnloadModule(unsigned int plugin_id);

#include "plugin_instance.hpp"

class DotnetPlugins : public plugin_manager_interface {
private:

	typedef std::list<internal_plugin_instance_ptr> plugins_type;
	std::string root_path;
	int id_;
	plugins_type plugins;
public:
	inline unsigned int get_id() {
		return id_;
	}
	inline void set_id(unsigned int id) {
		id_ = id;
	}

	typedef std::map<std::string, internal_plugin_instance_ptr> commands_type;
	commands_type commands;
	commands_type channels;

public:
	static std::string getModuleName() {
		return "DotnetPlugin";
	}
	static nscapi::module_version getModuleVersion() {
		nscapi::module_version version = { 0, 3, 0 };
		return version;
	}
	static std::string getModuleDescription() {
		return "Plugin to load and manage plugins written in dot net.";
	}

	bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();

	bool hasCommandHandler();
	bool hasMessageHandler();
	bool hasNotificationHandler();

	NSCAPI::nagiosReturn handleRAWCommand(const std::string &request, std::string &response);
	NSCAPI::nagiosReturn handleRAWNotification(const std::string &channel, std::string &request, std::string &response);
	NSCAPI::nagiosReturn commandRAWLineExec(const int target_type, const std::string &request, std::string &response);
	void DotnetPlugins::handleMessageRAW(std::string data);

	bool register_command(std::string command, internal_plugin_instance_ptr plugin);
	bool register_channel(std::wstring channel, internal_plugin_instance_ptr plugin);

	bool settings_register_key(std::wstring path, std::wstring key, NSCAPI::settings_type type, std::wstring title, std::wstring description, std::wstring defaultValue, bool advanced);
	bool settings_register_path(std::wstring path, std::wstring title, std::wstring description, bool advanced);
	nscapi::core_wrapper* get_core();

	bool settings_query(const std::string &request_json, std::string &response_json);
	bool registry_query(const std::string &request_json, std::string &response_json);

private:
	void load(std::string key, std::string factory, std::string val);
	int registry_reg_module(const std::string module);
};
