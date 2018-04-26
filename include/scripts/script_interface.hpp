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

#include <string>
#include <list>
#include <map>

#include <boost/shared_ptr.hpp>
#include <boost/optional.hpp>
#include <boost/foreach.hpp>

#include <NSCAPI.h>

namespace scripts {
	struct sample_trait {
		struct user_data {
			std::string foo;
		};
		typedef user_data user_data_type;

		struct function {
			std::string name;
			std::string object;
			void * instance;
		};
		typedef function function_type;
	};

	struct settings_provider;
	struct core_provider;
	template<class script_trait>
	class regitration_provider;
	template<class script_trait>
	struct script_information {
		int plugin_id;
		int script_id;
		std::string plugin_alias;
		std::string script_alias;
		std::string script;
		typename script_trait::user_data_type user_data;
		virtual ~script_information() {}
		virtual boost::shared_ptr<settings_provider> get_settings_provider() = 0;
		virtual boost::shared_ptr<core_provider> get_core_provider() = 0;
		virtual void register_command(const std::string type, const std::string &command, const std::string &description, typename script_trait::function_type function) = 0;
	};

	struct core_provider {
		virtual bool submit_simple_message(const std::string channel, const std::string command, const NSCAPI::nagiosReturn code, const std::string & message, const std::string & perf, std::string & response) = 0;
		virtual NSCAPI::nagiosReturn simple_query(const std::string &command, const std::list<std::string> & argument, std::string & msg, std::string & perf) = 0;
		virtual bool exec_simple_command(const std::string target, const std::string command, const std::list<std::string> &argument, std::list<std::string> & result) = 0;
		virtual bool exec_command(const std::string target, const std::string &request, std::string &response) = 0;
		virtual bool query(const std::string &request, std::string &response) = 0;
		virtual bool submit(const std::string target, const std::string &request, std::string &response) = 0;
		virtual bool reload(const std::string module) = 0;
		virtual void log(NSCAPI::log_level::level, const std::string file, int line, const std::string message) = 0;
	};

	struct settings_provider {
		virtual std::list<std::string> get_section(std::string section) = 0;
		virtual std::string get_string(std::string path, std::string key, std::string value) = 0;
		virtual void set_string(std::string path, std::string key, std::string value) = 0;
		virtual bool get_bool(std::string path, std::string key, bool value) = 0;
		virtual void set_bool(std::string path, std::string key, bool value) = 0;
		virtual int get_int(std::string path, std::string key, int value) = 0;
		virtual void set_int(std::string path, std::string key, int value) = 0;
		virtual void save() = 0;

		virtual void register_path(std::string path, std::string title, std::string description, bool advanced) = 0;
		virtual void register_key(std::string path, std::string key, std::string type, std::string title, std::string description, std::string defaultValue) = 0;

	};

	template<class script_trait>
	struct script_runtime_interface {
		virtual void load(scripts::script_information<script_trait> *info) = 0;
		virtual void unload(scripts::script_information<script_trait> *info) = 0;
		virtual void create_user_data(scripts::script_information<script_trait> *info) = 0;
	};

	struct nscp_runtime_interface {
		virtual void register_command(const std::string type, const std::string &command, const std::string &description) = 0;
		virtual boost::shared_ptr<settings_provider> get_settings_provider() = 0;
		virtual boost::shared_ptr<core_provider> get_core_provider() = 0;
	};

	template<class script_trait>
	struct command_definition {
		command_definition() {}
		command_definition(script_information<script_trait> *information) : information(information) {}
		command_definition(const command_definition &other)
			: function(other.function), information(other.information), type(other.type), command(other.command) {}
		command_definition& operator=(const command_definition &other) {
			function = other.function;
			information = other.information;
			type = other.type;
			command = other.command;
			return *this;
		}

		typename script_trait::function_type function;
		script_information<script_trait> *information;
		std::string type;
		std::string command;
	};

	template<class script_trait>
	struct script_manager;

	template<class script_trait>
	struct script_information_impl : script_information<script_trait> {
		script_manager<script_trait> *reg;
		boost::shared_ptr<settings_provider> settings;
		boost::shared_ptr<core_provider> core;

		script_information_impl(script_manager<script_trait> *reg, boost::shared_ptr<settings_provider> settings, boost::shared_ptr<core_provider> core)
			: reg(reg)
			, settings(settings)
			, core(core) {}
		virtual boost::shared_ptr<settings_provider> get_settings_provider() {
			return settings;
		}
		virtual boost::shared_ptr<core_provider> get_core_provider() {
			return core;
		}

		void register_command(const std::string type, const std::string &command, const std::string &description, typename script_trait::function_type function) {
			reg->register_command(this, type, command, description, function);
		}
	};

	template<class script_trait>
	struct script_manager {
	private:
		boost::shared_ptr<script_runtime_interface<script_trait> > script_runtime;
		boost::shared_ptr<nscp_runtime_interface> nscp_runtime;
		int plugin_id;
		int script_id;
		std::string plugin_alias;
		typedef std::map<int, script_information<script_trait>* > script_list_type;
		typedef std::map<std::string, command_definition<script_trait> > command_list_type;
		script_list_type scripts_;
		command_list_type commands;

	public:

		script_manager(boost::shared_ptr<script_runtime_interface<script_trait> > script_runtime_, boost::shared_ptr<nscp_runtime_interface> nscp_runtime, int plugin_id, std::string plugin_alias)
			: script_runtime(script_runtime_)
			, nscp_runtime(nscp_runtime)
			, plugin_id(plugin_id)
			, script_id(0)
			, plugin_alias(plugin_alias) {}
		script_information<script_trait>* add(std::string alias, std::string script) {
			script_information<script_trait> *info = new script_information_impl<script_trait>(this, nscp_runtime->get_settings_provider(), nscp_runtime->get_core_provider());
			info->plugin_alias = plugin_alias;
			info->plugin_id = plugin_id;
			info->script = script;
			info->script_alias = alias;
			info->script_id = script_id++;
			script_runtime->create_user_data(info);
			scripts_[info->script_id] = info;
			return info;
		}

		script_information<script_trait>* add_and_load(std::string alias, std::string script) {
			script_information<script_trait> *instance = add(alias, script);
			script_runtime->load(instance);
			return instance;
		}

		void load_all() {
			// TODO: locked
			BOOST_FOREACH(typename script_list_type::value_type &entry, scripts_) {
				script_runtime->load(entry.second);
			}
		}
		void unload_all() {
			// TODO: locked
			BOOST_FOREACH(typename script_list_type::value_type &entry, scripts_) {
				script_information<script_trait> * info = entry.second;
				script_runtime->unload(info);
				delete info;
			}
			scripts_.clear();
		}

		void register_command(script_information<script_trait> *information, const std::string type, const std::string &command, const std::string &description, typename script_trait::function_type function) {
			// TODO: locked
			command_definition<script_trait> cmd(information);
			cmd.function = function;
			cmd.command = command;
			cmd.type = type;
			commands[type + "$$" + command] = cmd;
			nscp_runtime->register_command(type, command, description);
		}

		boost::optional<command_definition<script_trait> > find_command(std::string type, std::string command) {
			// TODO: locked
			typename command_list_type::const_iterator it = commands.find(type + "$$" + command);
			if (it == commands.end()) {
				return boost::optional<command_definition<script_trait> >();
			}
			return boost::optional<command_definition<script_trait> >((*it).second);
		}
		/*
				virtual NSCAPI::errorReturn execute_command(const std::string &type, const std::string &command, const std::string &request, std::string &response) {
					boost::optional<command_definition<script_trait> > cmd = find_command(type, command);
					if (!cmd)
						return NSCAPI::returnIgnored;
					nscp_runtime->execute(type, command, description);
				}
		*/
		bool empty() const {
			return scripts_.empty();
		}
	};
}