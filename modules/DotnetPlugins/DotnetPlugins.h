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
#include <nscapi/macros.hpp>

NSC_WRAPPERS_MAIN();

#include "plugin_instance.hpp"

class DotnetPlugins : public plugin_manager {
private:

	typedef boost::shared_ptr<plugin_instance> plugin_type;
	typedef std::list<plugin_type> plugins_type;
	std::wstring root_path;
	int id_;
	plugins_type plugins;
public:
	inline unsigned int get_id() {
		return id_;
	}
	inline void set_id(unsigned int id) {
		id_ = id;
	}

	typedef std::map<std::wstring, plugin_type> commands_type;
	commands_type commands;
	commands_type channels;

public:
	static std::string getModuleName() {
		return "DotnetPlugin";
	}
	static nscapi::plugin_wrapper::module_version getModuleVersion() {
		nscapi::plugin_wrapper::module_version version = {0, 3, 0 };
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
	NSCAPI::nagiosReturn commandRAWLineExec(const std::string &request, std::string &response);

	bool register_command(std::wstring command, plugin_instance::plugin_type plugin, std::wstring description);
	bool register_channel(std::wstring channel, plugin_instance::plugin_type plugin);
	bool settings_register_key(std::wstring path, std::wstring key, NSCAPI::settings_type type, std::wstring title, std::wstring description, std::wstring defaultValue, bool advanced);
	bool settings_register_path(std::wstring path, std::wstring title, std::wstring description, bool advanced);
	nscapi::core_wrapper* get_core();

	bool settings_query(const std::string &request_json, std::string &response_json);
	bool registry_query(const std::string &request_json, std::string &response_json);

	void registry_reg_command(const std::string command, const std::string description, int plugin_id);

private:
	void load(std::string key, std::string val);
	std::list<std::string> settings_get_list(const std::string path);
	void settings_reg_path(const std::string path, const std::string title, const std::string desc);
	void settings_reg_key(const std::string path, const std::string key, const std::string title, const std::string desc);
	std::string settings_get_string(const std::string path, const std::string key, const std::string value);
	int settings_get_int(const std::string path, const std::string key, const int value);
	int registry_reg_module(const std::string module);

};
