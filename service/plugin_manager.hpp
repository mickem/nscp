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

#include "master_plugin_list.hpp"
#include "commands.hpp"
#include "channels.hpp"
#include "routers.hpp"
#include "scheduler_handler.hpp"
#include "plugin_cache.hpp"
#include "path_manager.hpp"

#include <nsclient/logger/logger.hpp>
#include <nscapi/nscapi_protobuf.hpp>

#include <settings/settings_core.hpp>

#include <boost/shared_ptr.hpp>

/**
 * @ingroup NSClient++
 * Main NSClient++ core class. This is the service core and as such is responsible for pretty much everything.
 * It also acts as a broker for all plugins and other sub threads and such.
 *
 * @version 1.0
 * first version
 *
 * @date 02-12-2005
 *
 * @author mickem
 *
 * @par license
 * This code is absolutely free to use and modify. The code is provided "as is" with
 * no expressed or implied warranty. The author accepts no liability if it causes
 * any damage to your computer, causes your pet to fall ill, increases baldness
 * or makes your car start emitting strange noises when you start it up.
 * This code has no bugs, just undocumented features!
 *
 * @todo Plugininfy the socket somehow ?
 * It is technically possible to make the socket a plug-in but would it be a good idea ?
 *
 * @bug
 *
 */

namespace nsclient {
	namespace core {
		class plugin_manager {
		public:
			typedef boost::shared_ptr<NSCPlugin> plugin_type;
		private:

			boost::filesystem::path basePath;
			nsclient::logging::logger_instance log_instance_;
			nsclient::commands commands_;
			nsclient::channels channels_;
			nsclient::simple_plugins_list metricsFetchers;
			nsclient::simple_plugins_list metricsSubmitetrs;
			nsclient::core::plugin_cache plugin_cache_;
			nsclient::event_subscribers event_subscribers_;

			nsclient::core::master_plugin_list plugin_list_;
			nsclient::core::path_instance path_;

		public:
			typedef std::multimap<std::string, std::string> plugin_alias_list_type;
			plugin_manager(nsclient::core::path_instance path_, nsclient::logging::logger_instance log_instance);
			virtual ~plugin_manager();

			NSCAPI::errorReturn send_notification(const char* channel, std::string &request, std::string &response);
			NSCAPI::nagiosReturn execute_query(const std::string &request, std::string &response);
			::Plugin::QueryResponseMessage execute_query(const ::Plugin::QueryRequestMessage &);
			std::wstring execute(std::wstring password, std::wstring cmd, std::list<std::wstring> args);
			int simple_exec(std::string command, std::vector<std::string> arguments, std::list<std::string> &resp);
			int simple_query(std::string module, std::string command, std::vector<std::string> arguments, std::list<std::string> &resp);
			NSCAPI::nagiosReturn exec_command(const char* target, std::string request, std::string &response);
			void register_submission_listener(unsigned int plugin_id, const char* channel);
			NSCAPI::nagiosReturn emit_event(const std::string &request);

			NSCAPI::errorReturn reload(const std::string module);
			bool do_reload(const std::string module);


			void register_command_alias(unsigned long id, std::string cmd, std::string desc) {
				commands_.register_alias(id, cmd, desc);
			}
			nsclient::core::plugin_cache* get_plugin_cache() {
				return &plugin_cache_;
			}

			nsclient::commands* get_commands() {
				return &commands_;
			}
			nsclient::channels* get_channels() {
				return &channels_;
			}
			nsclient::event_subscribers* get_event_subscribers() {
				return &event_subscribers_;
			}

			void loadPlugins(NSCAPI::moduleLoadMode mode);
			void unloadPlugins();
			std::string describeCommand(std::string command);
			std::list<std::string> getAllCommandNames();
			void registerCommand(unsigned int id, std::string cmd, std::string desc);

			plugin_type find_plugin(const unsigned int plugin_id);
			void remove_plugin(const std::string name);
			void load_plugin(const boost::filesystem::path &file, std::string alias);
			unsigned int add_plugin(unsigned int plugin_id);
			std::string get_plugin_module_name(unsigned int plugin_id);
			plugin_alias_list_type find_all_plugins(settings::instance_ptr settings, boost::filesystem::path pluginPath);
			plugin_alias_list_type find_all_active_plugins(settings::instance_ptr settings);


			bool boot_load_plugin(std::string plugin, bool boot = false);

			std::list<std::string> list_commands();

			void process_metrics();

			typedef boost::function<int(plugin_type)> run_function;
			int load_and_run(std::string module, run_function fun, std::list<std::string> &errors);
			plugin_type addPlugin(boost::filesystem::path file, std::string alias);

			bool reload_plugin(const std::string module);

		private:
			nsclient::logging::logger_instance get_logger() {
				return log_instance_;
			}

		};

		typedef boost::shared_ptr<plugin_manager> plugin_mgr_instance;
	}
}

