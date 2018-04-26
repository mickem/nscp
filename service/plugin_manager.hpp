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
#include <boost/filesystem/path.hpp>
#include <boost/optional.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/enable_shared_from_this.hpp>

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

		class core_exception : public std::exception {
			std::string what_;
		public:
			core_exception(std::string error) throw() : what_(error.c_str()) {}
			virtual ~core_exception() throw() {};

			virtual const char* what() const throw() {
				return what_.c_str();
			}
		};


		class plugin_manager : public boost::enable_shared_from_this<plugin_manager> {
		public:
			typedef boost::shared_ptr<nsclient::core::plugin_interface> plugin_type;
		private:

			boost::filesystem::path plugin_path_;

			nsclient::logging::logger_instance log_instance_;
			nsclient::commands commands_;
			nsclient::channels channels_;
			nsclient::simple_plugins_list metrics_fetchers_;
			nsclient::simple_plugins_list metrics_submitetrs_;
			nsclient::core::plugin_cache plugin_cache_;
			nsclient::event_subscribers event_subscribers_;
			nsclient::core::master_plugin_list plugin_list_;
			nsclient::core::path_instance path_;

		public:
			plugin_manager(nsclient::core::path_instance path_, nsclient::logging::logger_instance log_instance);
			virtual ~plugin_manager();

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

			void set_path(boost::filesystem::path path);

			void load_active_plugins();
			void load_all_plugins();
			bool load_single_plugin(std::string plugin, std::string alias = "", bool start = false);
			void start_plugins(NSCAPI::moduleLoadMode mode);
			void stop_plugins();
			plugin_type only_load_module(std::string module, std::string alias, bool &loaded);


			plugin_type find_plugin(const unsigned int plugin_id);
			bool remove_plugin(const std::string name);
			unsigned int clone_plugin(unsigned int plugin_id);
			bool reload_plugin(const std::string module);


			typedef boost::function<int(plugin_type)> run_function;
			int load_and_run(std::string module, run_function fun, std::list<std::string> &errors);
			NSCAPI::errorReturn send_notification(const char* channel, std::string &request, std::string &response);
			NSCAPI::nagiosReturn execute_query(const std::string &request, std::string &response);
			::Plugin::QueryResponseMessage execute_query(const ::Plugin::QueryRequestMessage &);
			std::wstring execute(std::wstring password, std::wstring cmd, std::list<std::wstring> args);
			int simple_exec(std::string command, std::vector<std::string> arguments, std::list<std::string> &resp);
			int simple_query(std::string module, std::string command, std::vector<std::string> arguments, std::list<std::string> &resp);
			NSCAPI::nagiosReturn exec_command(const char* target, std::string request, std::string &response);
			void register_submission_listener(unsigned int plugin_id, const char* channel);
			NSCAPI::nagiosReturn emit_event(const std::string &request);

			bool is_enabled(const std::string module);
			void process_metrics(Plugin::Common::MetricsBundle bundle);

			bool enable_plugin(std::string name);
			bool disable_plugin(std::string name);

		private:
			typedef std::multimap<std::string, std::string> plugin_alias_list_type;

			boost::optional<boost::filesystem::path> find_file(std::string file_name);
			bool contains_plugin(nsclient::core::plugin_manager::plugin_alias_list_type &ret, std::string alias, std::string plugin);
			std::string get_plugin_module_name(unsigned int plugin_id);

			plugin_type add_plugin(std::string file_name, std::string alias);

			plugin_alias_list_type find_all_plugins();
			plugin_alias_list_type find_all_active_plugins();
			nsclient::logging::logger_instance get_logger() {
				return log_instance_;
			}
			struct plugin_status {
				std::string alias;
				std::string plugin;
				bool enabled;

				plugin_status(std::string alias, std::string plugin, bool enabled)
					: alias(alias)
					, plugin(plugin)
					, enabled(enabled) {}
				plugin_status(std::string plugin)
					: alias("")
					, plugin(plugin)
					, enabled(true) {}
				plugin_status(const plugin_status &other)
					: alias(other.alias)
					, plugin(other.plugin)
					, enabled(other.enabled) {}

				plugin_status& operator=(const plugin_status &other) {
					this->alias = other.alias;
					this->plugin = other.plugin;
					this->enabled = other.enabled;
				}
			};
			plugin_status parse_plugin(std::string key);



			static std::string get_plugin_file(std::string key) {
#ifdef WIN32
				return key + ".dll";
#else
				return "lib" + key + ".so";
#endif
			}
		public:
			static bool is_module(const boost::filesystem::path file) {
#ifdef WIN32
				return boost::ends_with(file.string(), ".dll");
#else
				return boost::ends_with(file.string(), ".so");
#endif
			}
			static boost::filesystem::path get_filename(boost::filesystem::path folder, std::string module);
			static std::string file_to_module(const boost::filesystem::path &file) {
				std::string str = file.string();
#ifdef WIN32
				if (boost::ends_with(str, ".dll"))
					str = str.substr(0, str.size() - 4);
#else
				if (boost::ends_with(str, ".so"))
					str = str.substr(0, str.size() - 3);
				if (boost::starts_with(str, "lib"))
					str = str.substr(3);
#endif
				return str;
			}
		};

		typedef boost::shared_ptr<plugin_manager> plugin_mgr_instance;


	}
}

