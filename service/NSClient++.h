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

#include <service/system_service.hpp>

#include "NSCPlugin.h"
#include "commands.hpp"
#include "channels.hpp"
#include "routers.hpp"
#include <nsclient/logger.hpp>
#include "scheduler_handler.hpp"

#include <nscapi/nscapi_protobuf.hpp>

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
class NSClientT {
	struct plugin_cache_item {
		std::string name;
		std::string title;
		std::string desc;
	};
	typedef std::list<plugin_cache_item> plugin_cache_list;
public:
	typedef boost::shared_ptr<NSCPlugin> plugin_type;
	struct plugin_info_type {
		std::wstring dll;
		std::wstring name;
		std::wstring version;
		std::wstring description;
	};
	typedef std::list<plugin_info_type> plugin_info_list;
private:

	plugin_cache_list  plugin_cache_;

	class NSException {
		std::wstring what_;
	public:
		NSException(std::wstring what) : what_(what) {}
		std::wstring what() {
			return what_;
		}
	};

	typedef std::vector<plugin_type> pluginList;
	pluginList plugins_;
	boost::filesystem::path basePath;
	boost::filesystem::path tempPath;
	boost::timed_mutex internalVariables;
	boost::shared_mutex m_mutexRW;

	std::string context_;

	bool enable_shared_session_;
	nsclient::commands commands_;
	nsclient::channels channels_;
	nsclient::routers routers_;
	nsclient::simple_plugins_list metricsFetchers;
	nsclient::simple_plugins_list metricsSubmitetrs;
	unsigned int next_plugin_id_;
	std::string service_name_;

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
	//void service_on_session_changed(DWORD dwSessionId, bool logon, DWORD dwEventType);

	// Service API
	static NSClient* get_global_instance();
	void handle_startup(std::string service_name);
	void handle_shutdown(std::string service_name);
#ifdef _WIN32
	void handle_session_change(unsigned long dwSessionId, bool logon);
#endif


	void ownMetricsFetcher(Plugin::MetricsMessage::Response *response);

	// Member functions
	boost::filesystem::path getBasePath();
	boost::filesystem::path getTempPath();

	NSCAPI::errorReturn reroute(std::string &channel, std::string &buffer);
	NSCAPI::errorReturn send_notification(const char* channel, std::string &request, std::string &response);
	NSCAPI::nagiosReturn injectRAW(std::string &request, std::string &response);
	NSCAPI::nagiosReturn inject(std::string command, std::string arguments, std::string &msg, std::string & perf);
	std::wstring execute(std::wstring password, std::wstring cmd, std::list<std::wstring> args);
	int simple_exec(std::string command, std::vector<std::string> arguments, std::list<std::string> &resp);
	int simple_query(std::string module, std::string command, std::vector<std::string> arguments, std::list<std::string> &resp);
	NSCAPI::nagiosReturn exec_command(const char* target, std::string request, std::string &response);
	NSCAPI::errorReturn register_submission_listener(unsigned int plugin_id, const char* channel);
	NSCAPI::errorReturn register_routing_listener(unsigned int plugin_id, const char* channel);
	NSCAPI::errorReturn settings_query(const char *request_buffer, const unsigned int request_buffer_len, char **response_buffer, unsigned int *response_buffer_len);
	NSCAPI::errorReturn registry_query(const char *request_buffer, const unsigned int request_buffer_len, char **response_buffer, unsigned int *response_buffer_len);

	NSCAPI::errorReturn reload(const std::string module);
	bool do_reload(const std::string module);

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

	//plugin_type loadPlugin(const boost::filesystem::path plugin, std::wstring alias);
	void loadPlugins(NSCAPI::moduleLoadMode mode);
	void reloadPlugins();
	void unloadPlugins();
	std::string describeCommand(std::string command);
	std::list<std::string> getAllCommandNames();
	void registerCommand(unsigned int id, std::string cmd, std::string desc);
	//void startTrayIcons();
	//void startTrayIcon(DWORD dwSessionId);

	void listPlugins();
	plugin_type find_plugin(const unsigned int plugin_id);
	plugin_type find_plugin(const std::string plugin_id);
	plugin_info_list get_all_plugins();
	std::string get_plugin_module_name(unsigned int plugin_id);
	plugin_alias_list_type find_all_plugins(bool active);
	std::list<std::string> list_commands();

	std::string getFolder(std::string key);
	std::string expand_path(std::string file);

	void process_metrics();

	typedef boost::function<int(plugin_type)> run_function;
	int load_and_run(std::string module, run_function fun, std::list<std::string> &errors);



public:
	void preboot_load_all_plugin_files();

private:
	plugin_type addPlugin(boost::filesystem::path file, std::string alias);
};

extern NSClient *mainClient;	// Global core instance forward declaration.
