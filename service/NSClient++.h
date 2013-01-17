
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


#include <types.hpp>
#include <service/system_service.hpp>

#include "NSCPlugin.h"
#include "commands.hpp"
#include "channels.hpp"
#include "routers.hpp"
#include <nsclient/logger.hpp>

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

	class NSException {
		std::wstring what_;
	public:
		NSException(std::wstring what) : what_(what){}
		std::wstring what() {
			return what_;
		}
	};

	typedef std::vector<plugin_type> pluginList;
	pluginList plugins_;
	boost::filesystem::wpath basePath;
	boost::filesystem::wpath tempPath;
	boost::timed_mutex internalVariables;
	boost::shared_mutex m_mutexRW;

	std::wstring context_;

	bool enable_shared_session_;
	nsclient::commands commands_;
	nsclient::channels channels_;
	nsclient::routers routers_;
	unsigned int next_plugin_id_;
	std::wstring service_name_;


public:
	typedef std::multimap<std::wstring,std::wstring> plugin_alias_list_type;
	// c-tor, d-tor
	NSClientT();
	virtual ~NSClientT();

	// Service helper functions
	bool boot_init(std::wstring log_level = _T(""));
	bool boot_load_all_plugins();
	bool boot_load_plugin(std::wstring plugin);
	bool boot_start_plugins(bool boot);

	bool stop_unload_plugins_pre();
	bool stop_exit_pre();
	bool stop_exit_post();
	void set_settings_context(std::wstring context) { context_ = context; }
	void service_on_session_changed(DWORD dwSessionId, bool logon, DWORD dwEventType);

	// Service API
	static NSClient* get_global_instance();
	void handle_startup(std::wstring service_name);
	void handle_shutdown(std::wstring service_name);
#ifdef _WIN32
	void handle_session_change(unsigned long dwSessionId, bool logon);
#endif


	// Member functions
	boost::filesystem::wpath getBasePath();
	boost::filesystem::wpath getTempPath();

	NSCAPI::errorReturn reroute(std::wstring &channel, std::string &buffer);
	NSCAPI::errorReturn send_notification(const wchar_t* channel, std::string &request, std::string &response);
	NSCAPI::nagiosReturn injectRAW(const wchar_t* command, std::string &request, std::string &response);
	NSCAPI::nagiosReturn inject(std::wstring command, std::wstring arguments, std::wstring &msg, std::wstring & perf);
	std::wstring execute(std::wstring password, std::wstring cmd, std::list<std::wstring> args);
	int simple_exec(std::wstring command, std::vector<std::wstring> arguments, std::list<std::wstring> &resp);
	int simple_query(std::wstring module, std::wstring command, std::vector<std::wstring> arguments, std::list<std::wstring> &resp);
	NSCAPI::nagiosReturn exec_command(const wchar_t* target, const wchar_t* raw_command, std::string &request, std::string &response);
	NSCAPI::errorReturn register_submission_listener(unsigned int plugin_id, const wchar_t* channel);
	NSCAPI::errorReturn register_routing_listener(unsigned int plugin_id, const wchar_t* channel);

	NSCAPI::errorReturn reload(const std::wstring module);
	bool do_reload(const bool delay, const std::wstring module);

	struct service_controller {
		std::wstring service;
		service_controller(std::wstring service) : service(service) {}
		service_controller(const service_controller & other) : service(other.service) {}
		service_controller& operator=(const service_controller & other) {
			service = other.service;
			return *this;
		}
		void stop();
		void start();
		std::wstring get_service_name() {
			return service;
		}
		bool is_started();
	};

	service_controller get_service_control();

	//plugin_type loadPlugin(const boost::filesystem::wpath plugin, std::wstring alias);
	void loadPlugins(NSCAPI::moduleLoadMode mode);
	void unloadPlugins();
	std::wstring describeCommand(std::wstring command);
	std::list<std::wstring> getAllCommandNames();
	void registerCommand(unsigned int id, std::wstring cmd, std::wstring desc);
	void startTrayIcons();
	void startTrayIcon(DWORD dwSessionId);

	void listPlugins();
	plugin_info_list get_all_plugins();
	plugin_alias_list_type find_all_plugins(bool active);
	std::list<std::wstring> list_commands();

	std::wstring getFolder(std::wstring key);
	std::wstring expand_path(std::wstring file);

	typedef boost::function<int(plugin_type)> run_function;
	int load_and_run(std::wstring module, run_function fun, std::list<std::wstring> &errors);

	public:
		void preboot_load_all_plugin_files();

	private:
		plugin_type addPlugin(boost::filesystem::wpath file, std::wstring alias);
};


extern NSClient mainClient;	// Global core instance forward declaration.
