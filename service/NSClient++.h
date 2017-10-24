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

#include "nsclient_core_interface.hpp"
#include "scheduler_handler.hpp"
#include "plugin_cache.hpp"
#include "plugin_manager.hpp"

#include <nsclient/logger/logger.hpp>
#include <service/system_service.hpp>

class NSClientT;
typedef service_helper::impl<NSClientT>::system_service NSClient;

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


class NSClientT : public nsclient::core::core_interface {
public:
	typedef boost::shared_ptr<NSCPlugin> plugin_type;
private:

	boost::timed_mutex internalVariables;

	std::string context_;

	std::string service_name_;
	nsclient::logging::logger_instance log_instance_;
	nsclient::core::path_instance path_;
	nsclient::core::plugin_mgr_instance plugins_;

	task_scheduler::scheduler scheduler_;

public:
	typedef std::multimap<std::string, std::string> plugin_alias_list_type;
	// c-tor, d-tor
	NSClientT();
	virtual ~NSClientT();

	// Service helper functions
	bool boot_init(const bool override_log = false);
	bool boot_load_all_plugins();
	bool boot_load_plugin(std::string plugin, bool boot = false);
	bool boot_start_plugins(bool boot);

	bool stop_unload_plugins_pre();
	bool stop_exit_pre();
	bool stop_exit_post();
	void set_settings_context(std::string context) { context_ = context; }

	// Service API
	static NSClient* get_global_instance();
	void handle_startup(std::string service_name);
	void handle_shutdown(std::string service_name);
#ifdef _WIN32
	void handle_session_change(unsigned long dwSessionId, bool logon);
#endif


	void ownMetricsFetcher(Plugin::MetricsMessage::Response *response);

	nsclient::logging::logger_instance get_logger() {
		return log_instance_;
	}
	nsclient::core::plugin_mgr_instance get_plugin_manager() {
		return plugins_;
	}
	nsclient::core::path_instance get_path() {
		return path_;
	}

	NSCAPI::errorReturn register_submission_listener(unsigned int plugin_id, const char* channel);

	NSCAPI::errorReturn reload(const std::string module);
	bool do_reload(const std::string module);

	nsclient::core::plugin_cache* get_plugin_cache() {
		return plugins_->get_plugin_cache();
	}


	struct service_controller {
		std::string service;
		service_controller(std::string service) : service(service) {}
		service_controller(const service_controller & other) : service(other.service) {}
		service_controller& operator=(const service_controller & other) {
			service = other.service;
			return *this;
		}
		void stop();
		void start();
		std::string get_service_name() {
			return service;
		}
		bool is_started();
	};

	service_controller get_service_control();

	void reloadPlugins();
	void unloadPlugins();
	std::string describeCommand(std::string command);
	std::list<std::string> getAllCommandNames();
	void registerCommand(unsigned int id, std::string cmd, std::string desc);

 	plugin_type find_plugin(const unsigned int plugin_id);
	void load_plugin(const boost::filesystem::path &file, std::string alias);


	void process_metrics();

	void preboot_load_all_plugin_files();

};

