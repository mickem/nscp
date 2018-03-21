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
#include "storage_manager.hpp"

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
private:

	boost::timed_mutex internalVariables;

	std::string context_;

	std::string service_name_;
	nsclient::logging::logger_instance log_instance_;
	nsclient::core::path_instance path_;
	nsclient::core::plugin_mgr_instance plugins_;
	nsclient::core::storage_manager_instance storage_manager_;

	task_scheduler::scheduler scheduler_;

public:
	typedef std::multimap<std::string, std::string> plugin_alias_list_type;
	// c-tor, d-tor
	NSClientT();
	virtual ~NSClientT();

	// Startup/Shutdown
	bool load_configuration(const bool override_log = false);
	bool boot_load_active_plugins();
	void boot_load_all_plugin_files();
	bool boot_load_single_plugin(std::string plugin);
	bool boot_start_plugins(bool boot);

	bool stop_nsclient();
	void set_settings_context(std::string context) { context_ = context; }


	NSCAPI::errorReturn reload(const std::string module);
	bool do_reload(const std::string module);

	// Service API
	static NSClient* get_global_instance();
	void handle_startup(std::string service_name);
	void handle_shutdown(std::string service_name);
#ifdef _WIN32
	void handle_session_change(unsigned long dwSessionId, bool logon);
#endif



	// Core API interface (get modules)
	nsclient::logging::logger_instance get_logger() {
		return log_instance_;
	}
	nsclient::core::plugin_mgr_instance get_plugin_manager() {
		return plugins_;
	}
	nsclient::core::path_instance get_path() {
		return path_;
	}
	nsclient::core::plugin_cache* get_plugin_cache() {
		return plugins_->get_plugin_cache();
	}
	nsclient::core::storage_manager_instance get_storage_manager() override {
		return storage_manager_;
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

	void process_metrics();

private:
	void reloadPlugins();
	void unloadPlugins();

	Plugin::Common::MetricsBundle ownMetricsFetcher();


};

