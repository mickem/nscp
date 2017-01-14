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

#include "NSClient++.h"
#include "core_api.h"
#include "cli_parser.hpp"
#include "../libs/settings_manager/settings_manager_impl.h"

#include "logger/nsclient_logger.hpp"
#include "settings_query_handler.hpp"
#include "registry_query_handler.hpp"


#include <settings/config.hpp>
#include <nscapi/functions.hpp>
#include <parsers/expression/expression.hpp>
#include <nscapi/nscapi_settings_helper.hpp>
#include <version.hpp>

#include <timer.hpp>
#include <file_helpers.hpp>
#include <str/format.hpp>
#include <settings/settings_core.hpp>
#include <config.h>

#include <boost/unordered_set.hpp>


#ifdef WIN32
#include <Userenv.h>
#include <Lmcons.h>
#include <shellapi.h>
#include <ServiceCmd.h>
#include <com_helpers.hpp>
com_helper::initialize_com com_helper_;
#endif

#ifdef USE_BREAKPAD
#ifdef WIN32
#include <breakpad/exception_handler_win32.hpp>
// Used for breakpad crash handling
static ExceptionManager *g_exception_manager = NULL;
#endif
#endif


NSClient *mainClient = NULL;	// Global core instance.

/**
 * Application startup point
 *
 * @param argc Argument count
 * @param argv[] Argument array
 * @param envp[] Environment array
 * @return exit status
 */
int nscp_main(int argc, char* argv[]);

#ifdef WIN32
int wmain(int argc, wchar_t* argv[], wchar_t* envp[]) {
	char **wargv = new char*[argc];
	for (int i = 0; i < argc; i++) {
		std::string s = utf8::cvt<std::string>(argv[i]);
		wargv[i] = new char[s.length() + 10];
		strncpy(wargv[i], s.c_str(), s.size() + 1);
	}
	int ret = nscp_main(argc, wargv);
	for (int i = 0; i < argc; i++) {
		delete[] wargv[i];
	}
	delete[] wargv;
	return ret;
}
#else
int main(int argc, char* argv[]) {
	return nscp_main(argc, argv);
}
#endif

int nscp_main(int argc, char* argv[]) {
	mainClient = new NSClient();
	srand((unsigned)time(NULL));
	cli_parser parser(mainClient);
	int exit = parser.parse(argc, argv);
	delete mainClient;
	return exit;
}

std::list<std::string> NSClientT::list_commands() {
	return commands_.list_all();
}

bool contains_plugin(NSClientT::plugin_alias_list_type &ret, std::string alias, std::string plugin) {
	std::pair<std::string, std::string> v;
	BOOST_FOREACH(v, ret.equal_range(alias)) {
		if (v.second == plugin)
			return true;
	}
	return false;
}



//////////////////////////////////////////////////////////////////////////
// Service functions

struct nscp_settings_provider : public settings_manager::provider_interface {

	nsclient::logging::logger_instance log_instance_;
	nscp_settings_provider(nsclient::logging::logger_instance log_instance) : log_instance_(log_instance) {}
	virtual ~nscp_settings_provider() {}

	virtual std::string expand_path(std::string file) {
		return mainClient->expand_path(file);
	}
	nsclient::logging::logger_instance get_logger() const {
		return log_instance_;
	}
};

nscp_settings_provider *provider_ = NULL;

NSClientT::NSClientT()
	: next_plugin_id_(0)
	, service_name_(DEFAULT_SERVICE_NAME)
	, log_instance_(new nsclient::logging::impl::nsclient_logger())
	, commands_(log_instance_)
	, channels_(log_instance_)
	, metricsFetchers(log_instance_)
	, metricsSubmitetrs(log_instance_)
	, plugin_cache_(log_instance_)
	, event_subscribers_(log_instance_)

{
	provider_ = new nscp_settings_provider(log_instance_);
	log_instance_->startup();
}

NSClientT::~NSClientT() {
	try {
		delete provider_;
		log_instance_->destroy();
	} catch (...) {
		std::cerr << "UNknown exception raised: When destroying logger" << std::endl;
	}
}

NSClientT::plugin_alias_list_type NSClientT::find_all_plugins(bool active) {
	plugin_alias_list_type ret;

	settings::string_list list = settings_manager::get_settings()->get_keys(MAIN_MODULES_SECTION);
	BOOST_FOREACH(std::string plugin, list) {
		std::string alias;
		try {
			alias = settings_manager::get_settings()->get_string(MAIN_MODULES_SECTION, plugin, "");
		} catch (settings::settings_exception e) {
			LOG_DEBUG_CORE_STD("Exception looking for module: " + e.reason());
		}
		if (plugin == "enabled" || plugin == "1") {
			plugin = alias;
			alias = "";
		} else if (alias == "enabled" || alias == "1") {
			alias = "";
		} else if ((active && plugin == "disabled") || (active && alias == "disabled"))
			continue;
		else if ((active && plugin == "0") || (active && alias == "0"))
			continue;
		else if (plugin == "disabled" || plugin == "0") {
			plugin = alias;
			alias = "";
		} else if (alias == "disabled" || alias == "0") {
			alias = "";
		}
		if (!alias.empty()) {
			std::string tmp = plugin;
			plugin = alias;
			alias = tmp;
		}
		if (alias.empty()) {
			LOG_DEBUG_CORE_STD("Found: " + plugin);
		} else {
			LOG_DEBUG_CORE_STD("Found: " + plugin + " as " + alias);
		}
		if (plugin.length() > 4 && plugin.substr(plugin.length() - 4) == ".dll")
			plugin = plugin.substr(0, plugin.length() - 4);
		ret.insert(plugin_alias_list_type::value_type(alias, plugin));
	}
	if (!active) {
		boost::filesystem::path pluginPath = expand_path("${module-path}");
		boost::filesystem::directory_iterator end_itr; // default construction yields past-the-end
		for (boost::filesystem::directory_iterator itr(pluginPath); itr != end_itr; ++itr) {
			if (!is_directory(itr->status())) {
				boost::filesystem::path file = itr->path().filename();
				if (NSCPlugin::is_module(pluginPath / file)) {
					const std::string module = NSCPlugin::file_to_module(file);
					if (!contains_plugin(ret, "", module))
						ret.insert(plugin_alias_list_type::value_type("", module));
				}
			}
		}
	}
	return ret;
}

void NSClientT::preboot_load_all_plugin_files() {
	boost::filesystem::path pluginPath;
	{
		try {
			pluginPath = expand_path("${module-path}");
		} catch (std::exception &e) {
			LOG_CRITICAL_CORE(std::string("Failed to load plugins: ") + e.what() + " for " + expand_path("${module-path}"));
			return;
		}
		BOOST_FOREACH(plugin_alias_list_type::value_type v, find_all_plugins(false)) {
			if (v.second == "NSCPDOTNET.dll" || v.second == "NSCPDOTNET" || v.second == "NSCP.Core")
				continue;
			try {
				addPlugin(pluginPath / v.second, v.first);
			} catch (const NSPluginException &e) {
				if (e.file().find("FileLogger") != std::string::npos) {
					LOG_DEBUG_CORE_STD("Failed to register plugin: " + e.reason());
				} else {
					LOG_ERROR_CORE("Failed to register plugin " + v.second + ": " + e.reason());
				}
			} catch (...) {
				LOG_CRITICAL_CORE_STD("Failed to register plugin key: " + v.second);
			}
		}
	}
}

namespace sh = nscapi::settings_helper;

/**
 * Initialize the program
 * @param boot true if we shall boot all plugins
 * @param attachIfPossible is true we will attach to a running instance.
 * @return success
 * @author mickem
 */
bool NSClientT::boot_init(const bool override_log) {
#ifdef WIN32
	SetErrorMode(SEM_FAILCRITICALERRORS);
#endif

	LOG_DEBUG_CORE(utf8::cvt<std::string>(SERVICE_NAME) + " Loading settings and logger...");

	if (!settings_manager::init_settings(provider_, context_)) {
		return false;
	}

	log_instance_->configure();

	LOG_DEBUG_CORE(utf8::cvt<std::string>(SERVICE_NAME) + " booting...");
	LOG_DEBUG_CORE("Booted settings subsystem...");

	bool crash_submit = false;
	bool crash_archive = false;
	bool crash_restart = false;
	std::string crash_url, crash_folder, crash_target, log_level;
	try {
		sh::settings_registry settings(settings_manager::get_proxy());

		settings.add_path()
			(MAIN_MODULES_SECTION, "MODULES", "A list of modules.")
			;

		settings.add_path_to_settings()
			("log", "LOG SETTINGS", "Section for configuring the log handling.")
			("crash", "CRASH HANDLER", "Section for configuring the crash handler.")
			;

		settings.add_key_to_settings("log")
			("level", sh::string_key(&log_level, "info"),
				"LOG LEVEL", "Log level to use. Available levels are error,warning,info,debug,trace")
			;

		settings.add_key_to_settings("crash")
			("submit", sh::bool_key(&crash_submit, false),
				"SUBMIT CRASHREPORTS", "Submit crash reports to nsclient.org (or your configured submission server)")

			("archive", sh::bool_key(&crash_archive, true),
				"ARCHIVE CRASHREPORTS", "Archive crash reports in the archive folder")

			("restart", sh::bool_key(&crash_restart, true),
				"RESTART", "Submit crash reports to nsclient.org (or your configured submission server)")

			("restart target", sh::string_key(&crash_target, utf8::cvt<std::string>(get_service_control().get_service_name())),
				"RESTART SERVICE NAME", "The url to submit crash reports to")

			("submit url", sh::string_key(&crash_url, CRASH_SUBMIT_URL),
				"SUBMISSION URL", "The url to submit crash reports to")

			("archive folder", sh::path_key(&crash_folder, CRASH_ARCHIVE_FOLDER),
				"CRASH ARCHIVE LOCATION", "The folder to archive crash dumps in")
			;

		settings.register_all();
		settings.notify();
	} catch (settings::settings_exception e) {
		LOG_ERROR_CORE_STD("Could not find settings: " + e.reason());
	}
	if (!override_log) {
		log_instance_->set_log_level(log_level);
	}

#ifdef USE_BREAKPAD
#ifdef WIN32
	if (!g_exception_manager) {
		g_exception_manager = new ExceptionManager(false);

		g_exception_manager->setup_app(APPLICATION_NAME, STRPRODUCTVER, STRPRODUCTDATE);

		if (crash_restart) {
			LOG_DEBUG_CORE("On crash: restart: " + crash_target);
			g_exception_manager->setup_restart(crash_target);
		}

		bool crashHandling = false;
		if (crash_submit) {
			g_exception_manager->setup_submit(false, crash_url);
			LOG_DEBUG_CORE("Submitting crash dumps to central server: " + crash_url);
			crashHandling = true;
		}
		if (crash_archive) {
			g_exception_manager->setup_archive(crash_folder);
			LOG_DEBUG_CORE("Archiving crash dumps in: " + crash_folder);
			crashHandling = true;
		}
		if (!crashHandling) {
			LOG_ERROR_CORE("No crash handling configured");
		} else {
			g_exception_manager->StartMonitoring();
		}
	}
#endif
#endif

#ifdef WIN32
	try {
		com_helper_.initialize();
	} catch (com_helper::com_exception e) {
		LOG_ERROR_CORE_STD("COM exception: " + e.reason());
		return false;
	} catch (...) {
		LOG_ERROR_CORE("Unknown exception iniating COM...");
		return false;
	}
#endif
	return true;
}
bool NSClientT::boot_load_all_plugins() {
	LOG_DEBUG_CORE("booting::loading plugins");
	try {
		boost::filesystem::path pluginPath = expand_path("${module-path}");
		if (!boost::filesystem::is_directory(pluginPath)) {
			LOG_ERROR_CORE("Failed to find modules folder: " + pluginPath.string());
			return false;
		}
		BOOST_FOREACH(const plugin_alias_list_type::value_type &v, find_all_plugins(true)) {
			std::string file = utf8::cvt<std::string>(NSCPlugin::get_plugin_file(v.second));
			std::string alias = v.first;
			boost::filesystem::path module = pluginPath / file;
			try {
				addPlugin(module, alias);
			} catch (const NSPluginException& e) {
				if (e.file().find("FileLogger") != std::string::npos) {
					LOG_DEBUG_CORE_STD("Failed to load " + module.string() + ": " + e.reason());
				} else {
					LOG_ERROR_CORE_STD("Failed to load " + module.string() + ": " + e.reason());
				}
			} catch (const std::exception &e) {
				LOG_ERROR_CORE_STD("exception loading plugin: " + file + utf8::utf8_from_native(e.what()));
				return false;
			} catch (...) {
				LOG_ERROR_CORE_STD("Unknown exception loading plugin: " + file);
				return false;
			}
		}
	} catch (const settings::settings_exception &e) {
		LOG_ERROR_CORE_STD("Settings exception when loading modules: " + e.reason());
		return false;
	} catch (...) {
		LOG_ERROR_CORE("Unknown exception when loading plugins");
		return false;
	}
	return true;
}

bool NSClientT::boot_load_plugin(std::string plugin, bool boot) {
	try {
		if (plugin.length() > 4 && plugin.substr(plugin.length() - 4) == ".dll")
			plugin = plugin.substr(0, plugin.length() - 4);

		std::string plugin_file = NSCPlugin::get_plugin_file(plugin);
		boost::filesystem::path pluginPath = expand_path("${module-path}");
		boost::filesystem::path file = pluginPath / plugin_file;
		NSClientT::plugin_type instance;
		if (boost::filesystem::is_regular(file)) {
			instance = addPlugin(file, "");
		} else {
			if (plugin_file == "CheckTaskSched1.dll" || plugin_file == "CheckTaskSched2.dll") {
				LOG_ERROR_CORE_STD("Your loading the CheckTaskSched1/2 which has been renamed into CheckTaskSched, please update your config");
				plugin_file = "CheckTaskSched.dll";
				boost::filesystem::path file = pluginPath / plugin_file;
				if (boost::filesystem::is_regular(file)) {
					instance = addPlugin(file, "");
				}
			} else {
				LOG_ERROR_CORE_STD("Failed to load: " + plugin + "File not found: " + file.string());
				return false;
			}
		}
		if (boot) {
			try {
				if (!instance->load_plugin(NSCAPI::normalStart)) {
					LOG_ERROR_CORE_STD("Plugin refused to load: " + instance->get_description());
				}
			} catch (NSPluginException e) {
				LOG_ERROR_CORE_STD("Could not load plugin: " + e.reason() + ": " + e.file());
			} catch (...) {
				LOG_ERROR_CORE_STD("Could not load plugin: " + instance->get_description());
			}
		}
		return true;
	} catch (const NSPluginException &e) {
		LOG_ERROR_CORE_STD("Module (" + e.file() + ") was not found: " + e.reason());
	} catch (const std::exception &e) {
		LOG_ERROR_CORE_STD("Module (" + plugin + ") was not found: " + utf8::utf8_from_native(e.what()));
	} catch (...) {
		LOG_ERROR_CORE_STD("Module (" + plugin + ") was not found...");
	}
	return false;
}
bool NSClientT::boot_start_plugins(bool boot) {
	try {
		loadPlugins(boot ? NSCAPI::normalStart : NSCAPI::dontStart);
	} catch (...) {
		LOG_ERROR_CORE("Unknown exception loading plugins");
		return false;
	}
	if (boot) {
		settings_manager::get_core()->register_key(0xffff, "/settings/core", "settings maintenance interval", settings::settings_core::key_string, "Maintenance interval", "How often settings shall reload config if it has changed", "5m", true, false);
		std::string smi = settings_manager::get_settings()->get_string("/settings/core", "settings maintenance interval", "5m");
		scheduler_.add_task(task_scheduler::schedule_metadata::SETTINGS, smi);
		settings_manager::get_core()->register_key(0xffff, "/settings/core", "metrics interval", settings::settings_core::key_string, "Maintenance interval", "How often to fetch metrics from modules", "5s", true, false);
		smi = settings_manager::get_settings()->get_string("/settings/core", "metrics interval", "5s");
		scheduler_.add_task(task_scheduler::schedule_metadata::METRICS, smi);
		scheduler_.start();
	}
	LOG_DEBUG_CORE(utf8::cvt<std::string>(APPLICATION_NAME " - " CURRENT_SERVICE_VERSION " Started!"));
	return true;
}

bool NSClientT::stop_unload_plugins_pre() {
	scheduler_.stop();
	LOG_DEBUG_CORE("Attempting to stop all plugins");
	try {
		LOG_DEBUG_CORE("Stopping all plugins");
		mainClient->unloadPlugins();
	} catch (NSPluginException e) {
		LOG_ERROR_CORE_STD("Exception raised when unloading non msg plguins: " + e.reason() + " in module: " + e.file());
	} catch (...) {
		LOG_ERROR_CORE("Unknown exception raised when unloading non msg plugins");
	}
	return true;
}
bool NSClientT::stop_exit_pre() {
#ifdef WIN32
	LOG_DEBUG_CORE("Stopping: COM helper");
	try {
		com_helper_.unInitialize();
	} catch (com_helper::com_exception e) {
		LOG_ERROR_CORE_STD("COM exception: " + e.reason());
	} catch (...) {
		LOG_ERROR_CORE("Unknown exception uniniating COM...");
	}
#endif
	LOG_DEBUG_CORE("Stopping: Settings instance");
	settings_manager::destroy_settings();
	return true;
}
bool NSClientT::stop_exit_post() {
	try {
		log_instance_->shutdown();
		google::protobuf::ShutdownProtobufLibrary();
	} catch (...) {
		LOG_ERROR_CORE("UNknown exception raised: When stopping");
	}
	return true;
}
//////////////////////////////////////////////////////////////////////////
// Member functions

/**
 * Unload all plug-ins (in reversed order)
 */
void NSClientT::unloadPlugins() {
	log_instance_->clear_subscribers();
	{
		boost::shared_lock<boost::shared_mutex> readLock(m_mutexRW, boost::get_system_time() + boost::posix_time::milliseconds(5000));
		if (!readLock.owns_lock()) {
			LOG_ERROR_CORE("FATAL ERROR: Could not get read-mutex.");
			return;
		}
		BOOST_FOREACH(plugin_type &p, plugins_) {
			if (!p)
				continue;
			try {
				LOG_DEBUG_CORE_STD("Unloading plugin: " + p->get_alias_or_name() + "...");
				p->unload_plugin();
			} catch (const NSPluginException &e) {
				LOG_ERROR_CORE_STD("Exception raised when unloading plugin: " + e.reason() + " in module: " + e.file());
			} catch (...) {
				LOG_ERROR_CORE("Unknown exception raised when unloading plugin");
			}
		}
	}
	{
		boost::unique_lock<boost::shared_mutex> writeLock(m_mutexRW, boost::get_system_time() + boost::posix_time::seconds(10));
		if (!writeLock.owns_lock()) {
			LOG_ERROR_CORE("FATAL ERROR: Could not get read-mutex.");
			return;
		}
		commands_.remove_all();
		channels_.remove_all();
		plugins_.clear();
	}
}
void NSClientT::reloadPlugins() {
	loadPlugins(NSCAPI::reloadStart);
	boot_load_all_plugins();
	loadPlugins(NSCAPI::normalStart);
	// TODO: Figure out changed set and remove/add delete/added modules.
	settings_manager::get_core()->set_reload(false);
}

bool NSClientT::do_reload(const std::string module) {
	if (module == "settings") {
		try {
			settings_manager::get_settings()->clear_cache();
			return true;
		} catch (const std::exception &e) {
			LOG_ERROR_CORE_STD("Exception raised when reloading: " + utf8::utf8_from_native(e.what()));
		} catch (...) {
			LOG_ERROR_CORE("Exception raised when reloading: UNKNOWN");
		}
	} else if (module == "service") {
		try {
			LOG_DEBUG_CORE_STD("Reloading all modules.");
			reloadPlugins();
			return true;
		} catch (const std::exception &e) {
			LOG_ERROR_CORE_STD("Exception raised when reloading: " + utf8::utf8_from_native(e.what()));
		} catch (...) {
			LOG_ERROR_CORE("Exception raised when reloading: UNKNOWN");
		}
	} else {
		boost::unique_lock<boost::shared_mutex> writeLock(m_mutexRW, boost::get_system_time() + boost::posix_time::seconds(5));
		if (!writeLock.owns_lock()) {
			LOG_ERROR_CORE("Failed to reload module (could not get write lock).");
			return false;
		}
		BOOST_FOREACH(plugin_type &p, plugins_) {
			if (p->get_alias() == module) {
				LOG_DEBUG_CORE_STD(std::string("Reloading: ") + p->get_alias_or_name());
				p->load_plugin(NSCAPI::reloadStart);
				return true;
			}
		}
	}
	return false;
}

NSCAPI::errorReturn NSClientT::reload(const std::string module) {
	try {
		std::string task = module;
		bool delayed = false;
		if (module.size() > 8 && module.substr(0, 8) == "delayed,") {
			task = module.substr(8);
			delayed = true;
		} else if (module.size() > 6 && module.substr(0, 6) == "delay,") {
			task = module.substr(6);
			delayed = true;
		} else if (module.size() > 6 && module.substr(0, 8) == "instant,") {
			task = module.substr(8);
			delayed = false;
		} else if (module == "service") {
			delayed = false;
		}
		if (delayed) {
			LOG_TRACE_CORE("Delayed reload");
			scheduler_.add_task(task_scheduler::schedule_metadata::RELOAD, "", task);
			return NSCAPI::api_return_codes::isSuccess;
		} else {
			LOG_TRACE_CORE("Instant reload");
			return do_reload(task) ? NSCAPI::api_return_codes::isSuccess : NSCAPI::api_return_codes::hasFailed;
		}

	} catch (const std::exception &e) {
		LOG_ERROR_CORE("Reload failed: " + utf8::utf8_from_native(e.what()));
		return NSCAPI::api_return_codes::hasFailed;
	} catch (...) {
		LOG_ERROR_CORE("Reload failed");
		return NSCAPI::api_return_codes::hasFailed;
	}
}

std::list<NSClientT::plugin_type> NSClientT::get_plugins_c() {
	std::list<plugin_type> ret;
	boost::shared_lock<boost::shared_mutex> readLock(m_mutexRW, boost::get_system_time() + boost::posix_time::milliseconds(5000));
	if (!readLock.owns_lock()) {
		LOG_ERROR_CORE("FATAL ERROR: Could not get read-mutex.");
		return ret;
	}
	BOOST_FOREACH(plugin_type &plugin, plugins_) {
		ret.push_back(plugin);
	}
	return ret;
}

void NSClientT::loadPlugins(NSCAPI::moduleLoadMode mode) {
	{
		std::set<long> broken;
		{
			BOOST_FOREACH(plugin_type plugin, get_plugins_c()) {
				LOG_DEBUG_CORE_STD("Loading plugin: " + plugin->get_description());
				try {
					if (!plugin->load_plugin(mode)) {
						LOG_ERROR_CORE_STD("Plugin refused to load: " + plugin->get_description());
						broken.insert(plugin->get_id());
					}
				} catch (const NSPluginException &e) {
					broken.insert(plugin->get_id());
					LOG_ERROR_CORE_STD("Could not load plugin: " + e.reason() + ": " + e.file());
				} catch (const std::exception &e) {
					broken.insert(plugin->get_id());
					LOG_ERROR_CORE_STD("Could not load plugin: " + plugin->get_alias() + ": " + e.what());
				} catch (...) {
					broken.insert(plugin->get_id());
					LOG_ERROR_CORE_STD("Could not load plugin: " + plugin->get_description());
				}
			}
		}
		if (broken.size() > 0) {
			boost::unique_lock<boost::shared_mutex> writeLock(m_mutexRW, boost::get_system_time() + boost::posix_time::seconds(5));
			if (!writeLock.owns_lock()) {
				LOG_ERROR_CORE("FATAL ERROR: Could not get write-mutex.");
				return;
			}
			for (pluginList::iterator it = plugins_.begin(); it != plugins_.end();) {
				if (broken.find((*it)->get_id()) != broken.end())
					it = plugins_.erase(it);
				else
					++it;
			}
		}
	}
}
/**
 * Load and add a plugin to various internal structures
 * @param plugin The plug-in instance to load. The pointer is managed by the
 */
NSClientT::plugin_type NSClientT::addPlugin(boost::filesystem::path file, std::string alias) {
	{
		LOG_DEBUG_CORE_STD(NSCPlugin::get_plugin_file(file.string()));
		if (alias.empty()) {
			LOG_DEBUG_CORE_STD("adding " + file.string());
		} else {
			LOG_DEBUG_CORE_STD("adding " + file.string() + " (" + alias + ")");
		}
		// Check if this is a duplicate plugin (if so return that instance)
		boost::unique_lock<boost::shared_mutex> writeLock(m_mutexRW, boost::get_system_time() + boost::posix_time::seconds(10));
		if (!writeLock.owns_lock()) {
			LOG_ERROR_CORE("FATAL ERROR: Could not get read-mutex.");
			return plugin_type();
		}

		BOOST_FOREACH(plugin_type plug, plugins_) {
			if (plug->is_duplicate(file, alias)) {
				LOG_DEBUG_CORE_STD("Found duplicate plugin returning old " + str::xtos(plug->get_id()));
				return plug;
			}
		}
	}

	plugin_type plugin(new NSCPlugin(next_plugin_id_++, file.normalize(), alias));
	plugin->load_dll();
	{
		boost::unique_lock<boost::shared_mutex> writeLock(m_mutexRW, boost::get_system_time() + boost::posix_time::seconds(10));
		if (!writeLock.owns_lock()) {
			LOG_ERROR_CORE("FATAL ERROR: Could not get read-mutex.");
			return plugin;
		}

		plugins_.insert(plugins_.end(), plugin);
		if (plugin->hasCommandHandler())
			commands_.add_plugin(plugin);
		if (plugin->hasNotificationHandler())
			channels_.add_plugin(plugin);
		if (plugin->hasMetricsFetcher())
			metricsFetchers.add_plugin(plugin);
		if (plugin->hasMetricsSubmitter())
			metricsSubmitetrs.add_plugin(plugin);
		if (plugin->hasMessageHandler()) {
			log_instance_->add_subscriber(plugin);
		}
		if (plugin->has_on_event()) {
			event_subscribers_.add_plugin(plugin);
		}
		settings_manager::get_core()->register_key(0xffff, MAIN_MODULES_SECTION, plugin->getModule(), settings::settings_core::key_string, plugin->getName(), plugin->getDescription(), "0", false, false);
	}
	plugin_cache_.add_plugin(plugin);
	return plugin;
}

std::string NSClientT::describeCommand(std::string command) {
	return commands_.describe(command).description;
}
std::list<std::string> NSClientT::getAllCommandNames() {
	return commands_.list_all();
}
void NSClientT::registerCommand(unsigned int id, std::string cmd, std::string desc) {
	return commands_.register_command(id, cmd, desc);
}

struct command_chunk {
	nsclient::commands::plugin_type plugin;
	Plugin::QueryRequestMessage request;
};


::Plugin::QueryResponseMessage NSClientT::execute_query(const ::Plugin::QueryRequestMessage &req) {
	::Plugin::QueryResponseMessage resp;
	std::string buffer;
	if (execute_query(req.SerializeAsString(), buffer) == NSCAPI::cmd_return_codes::isSuccess) {
		resp.ParseFromString(buffer);
	}
	return resp;
}
/**
 * Inject a command into the plug-in stack.
 *
 * @param command Command to inject
 * @param argLen Length of argument buffer
 * @param **argument Argument buffer
 * @param *returnMessageBuffer Message buffer
 * @param returnMessageBufferLen Length of returnMessageBuffer
 * @param *returnPerfBuffer Performance data buffer
 * @param returnPerfBufferLen Length of returnPerfBuffer
 * @return The command status
 */
NSCAPI::nagiosReturn NSClientT::execute_query(const std::string &request, std::string &response) {
	try {
		Plugin::QueryRequestMessage request_message;
		Plugin::QueryResponseMessage response_message;
		request_message.ParseFromString(request);

		typedef boost::unordered_map<int, command_chunk> command_chunk_type;
		command_chunk_type command_chunks;

		std::string missing_commands;

		if (request_message.header().has_command()) {
			std::string command = request_message.header().command();
			nsclient::commands::plugin_type plugin = commands_.get(command);
			if (plugin) {
				unsigned int id = plugin->get_id();
				command_chunks[id].plugin = plugin;
				command_chunks[id].request.CopyFrom(request_message);
			} else {
				str::format::append_list(missing_commands, command);
			}
		} else {
			for (int i = 0; i < request_message.payload_size(); i++) {
				::Plugin::QueryRequestMessage::Request *payload = request_message.mutable_payload(i);
				payload->set_command(commands_.make_key(payload->command()));
				nsclient::commands::plugin_type plugin = commands_.get(payload->command());
				if (plugin) {
					unsigned int id = plugin->get_id();
					if (command_chunks.find(id) == command_chunks.end()) {
						command_chunks[id].plugin = plugin;
						command_chunks[id].request.mutable_header()->CopyFrom(request_message.header());
					}
					command_chunks[id].request.add_payload()->CopyFrom(*payload);
				} else {
					str::format::append_list(missing_commands, payload->command());
				}
			}
		}

		if (command_chunks.size() == 0) {
			LOG_ERROR_CORE("Unknown command(s): " + missing_commands + " available commands: " + commands_.to_string());
			Plugin::QueryResponseMessage::Response *payload = response_message.add_payload();
			payload->set_command(missing_commands);
			nscapi::protobuf::functions::set_response_bad(*payload, "Unknown command(s): " + missing_commands);
			response = response_message.SerializeAsString();
			return NSCAPI::cmd_return_codes::isSuccess;
		}

		BOOST_FOREACH(command_chunk_type::value_type &v, command_chunks) {
			std::string local_response;
			int ret = v.second.plugin->handleCommand(v.second.request.SerializeAsString(), local_response);
			if (ret != NSCAPI::cmd_return_codes::isSuccess) {
				LOG_ERROR_CORE("Failed to execute command");
			} else {
				Plugin::QueryResponseMessage local_response_message;
				local_response_message.ParseFromString(local_response);
				if (!response_message.has_header()) {
					response_message.mutable_header()->CopyFrom(local_response_message.header());
				}
				for (int i = 0; i < local_response_message.payload_size(); i++) {
					response_message.add_payload()->CopyFrom(local_response_message.payload(i));
				}
			}
		}
		response = response_message.SerializeAsString();
	} catch (const std::exception &e) {
		LOG_ERROR_CORE("Failed to process command: " + utf8::utf8_from_native(e.what()));
		return NSCAPI::cmd_return_codes::hasFailed;
	} catch (...) {
		LOG_ERROR_CORE("Failed to process command: ");
		return NSCAPI::cmd_return_codes::hasFailed;
	}
	return NSCAPI::cmd_return_codes::isSuccess;
}

int NSClientT::load_and_run(std::string module, run_function fun, std::list<std::string> &errors) {
	int ret = -1;
	plugin_type match;
	{
		boost::shared_lock<boost::shared_mutex> readLock(m_mutexRW, boost::get_system_time() + boost::posix_time::seconds(5));
		if (!readLock.owns_lock()) {
			LOG_ERROR_CORE("FATAL ERROR: Could not get read-mutex.");
			return -1;
		}
		BOOST_FOREACH(plugin_type plugin, plugins_) {
			if (plugin && (module.empty() || plugin->getModule() == module)) {
				match = plugin;
			}
		}
	}
	if (match) {
		LOG_DEBUG_CORE_STD("Found module: " + match->get_alias_or_name() + "...");
		try {
			ret = fun(match);
		} catch (const NSPluginException &e) {
			errors.push_back("Could not execute command: " + e.reason() + " in " + e.file());
			return -1;
		}
	} else if (!module.empty()) {
		try {
			boost::filesystem::path pluginPath = expand_path("${module-path}");
			boost::filesystem::path file = NSCPlugin::get_filename(pluginPath, module);
			if (!boost::filesystem::is_regular(file)) {
				file = NSCPlugin::get_filename("./modules", module);
				LOG_DEBUG_CORE_STD("Found local plugin");
			}
			if (boost::filesystem::is_regular(file)) {
				plugin_type plugin = addPlugin(file, "");
				if (plugin) {
					LOG_DEBUG_CORE_STD("Loading plugin: " + plugin->get_alias_or_name() + "...");
					plugin->load_plugin(NSCAPI::dontStart);
					ret = fun(plugin);
				} else {
					errors.push_back("Failed to load: " + file.string());
					return 1;
				}
			} else {
				errors.push_back("File not found: " + file.string());
				return 1;
			}
		} catch (const NSPluginException &e) {
			errors.push_back("Module (" + e.file() + ") was not found: " + utf8::utf8_from_native(e.what()));
		} catch (const std::exception &e) {
			errors.push_back(std::string("Module (") + module + ") was not found: " + utf8::utf8_from_native(e.what()));
			return 1;
		} catch (...) {
			errors.push_back("Module (" + module + ") was not found...");
			return 1;
		}
	}
	return ret;
}

int exec_helper(NSClientT::plugin_type plugin, std::string command, std::vector<std::string> arguments, std::string request, std::list<std::string> *responses) {
	std::string response;
	if (!plugin || !plugin->has_command_line_exec())
		return -1;
	int ret = plugin->commandLineExec(true, request, response);
	if (ret != NSCAPI::cmd_return_codes::returnIgnored && !response.empty())
		responses->push_back(response);
	return ret;
}

int NSClientT::simple_exec(std::string command, std::vector<std::string> arguments, std::list<std::string> &resp) {
	std::string request;
	std::list<std::string> responses;
	std::list<std::string> errors;
	std::string module;
	std::string::size_type pos = command.find('.');
	if (pos != std::string::npos) {
		module = command.substr(0, pos);
		command = command.substr(pos + 1);
	}
	nscapi::protobuf::functions::create_simple_exec_request(module, command, arguments, request);
	int ret = load_and_run(module, boost::bind(&exec_helper, _1, command, arguments, request, &responses), errors);

	BOOST_FOREACH(std::string &r, responses) {
		try {
			ret = nscapi::protobuf::functions::parse_simple_exec_response(r, resp);
		} catch (std::exception &e) {
			resp.push_back("Failed to extract return message: " + utf8::utf8_from_native(e.what()));
			LOG_ERROR_CORE_STD("Failed to extract return message: " + utf8::utf8_from_native(e.what()));
			return NSCAPI::cmd_return_codes::hasFailed;
		}
	}
	BOOST_FOREACH(const std::string &e, errors) {
		LOG_ERROR_CORE_STD(e);
		resp.push_back(e);
	}
	return ret;
}
int query_helper(NSClientT::plugin_type plugin, std::string command, std::vector<std::string> arguments, std::string request, std::list<std::string> *responses) {
	return NSCAPI::cmd_return_codes::returnIgnored;
	// 	std::string response;
	// 	if (!plugin->hasCommandHandler())
	// 		return NSCAPI::returnIgnored;
	// 	int ret = plugin->handleCommand(command.c_str(), request, response);
	// 	if (ret != NSCAPI::returnIgnored && !response.empty())
	// 		responses->push_back(response);
	// 	return ret;
}

int NSClientT::simple_query(std::string module, std::string command, std::vector<std::string> arguments, std::list<std::string> &resp) {
	std::string request;
	std::list<std::string> responses;
	std::list<std::string> errors;
	nscapi::protobuf::functions::create_simple_query_request(command, arguments, request);
	int ret = load_and_run(module, boost::bind(&query_helper, _1, command, arguments, request, &responses), errors);

	nsclient::commands::plugin_type plugin = commands_.get(command);
	if (!plugin) {
		LOG_ERROR_CORE("No handler for command: " + command + " available commands: " + commands_.to_string());
		resp.push_back("No handler for command: " + command);
		return NSCAPI::query_return_codes::returnUNKNOWN;
	}
	std::string response;
	ret = plugin->handleCommand(request, response);
	try {
		std::string msg, perf;
		ret = nscapi::protobuf::functions::parse_simple_query_response(response, msg, perf);
		resp.push_back(msg + "|" + perf);
	} catch (std::exception &e) {
		resp.push_back("Failed to extract return message: " + utf8::utf8_from_native(e.what()));
		LOG_ERROR_CORE_STD("Failed to extract return message: " + utf8::utf8_from_native(e.what()));
		return NSCAPI::query_return_codes::returnUNKNOWN;
	}
	BOOST_FOREACH(const std::string &e, errors) {
		LOG_ERROR_CORE_STD(e);
		resp.push_back(e);
	}
	return ret;
}

NSCAPI::nagiosReturn NSClientT::exec_command(const char* raw_target, std::string request, std::string &response) {
	std::string target = raw_target;
	LOG_DEBUG_CORE_STD("Executing command is target for: " + target);
	bool match_any = false;
	bool match_all = false;
	if (target == "any")
		match_any = true;
	else if (target == "all" || target == "*")
		match_all = true;
	std::list<std::string> responses;
	bool found = false;
	{
		boost::shared_lock<boost::shared_mutex> readLock(m_mutexRW, boost::get_system_time() + boost::posix_time::seconds(5));
		if (!readLock.owns_lock()) {
			LOG_ERROR_CORE("FATAL ERROR: Could not get read-mutex.");
			return -1;
		}
		BOOST_FOREACH(plugin_type p, plugins_) {
			if (p && p->has_command_line_exec()) {
				IS_LOG_TRACE_CORE() {
					LOG_TRACE_CORE("Trying : " + p->get_alias_or_name());
				}
				try {
					if (match_all || match_any || p->get_alias() == target || p->get_alias_or_name().find(target) != std::string::npos) {
						std::string respbuffer;
						LOG_DEBUG_CORE_STD("Executing command in: " + p->getName());
						NSCAPI::nagiosReturn r = p->commandLineExec(!(match_all || match_any), request, respbuffer);
						if (r != NSCAPI::cmd_return_codes::returnIgnored && !respbuffer.empty()) {
							LOG_DEBUG_CORE_STD("Module handled execution request: " + p->getName());
							found = true;
							if (match_any) {
								response = respbuffer;
								return NSCAPI::exec_return_codes::returnOK;
							}
							responses.push_back(respbuffer);
						}
					}
				} catch (NSPluginException e) {
					LOG_ERROR_CORE_STD("Could not execute command: " + e.reason() + " in " + e.file());
				}
			}
		}
	}

	Plugin::ExecuteResponseMessage response_message;

	BOOST_FOREACH(std::string r, responses) {
		Plugin::ExecuteResponseMessage tmp;
		tmp.ParseFromString(r);
		for (int i = 0; i < tmp.payload_size(); i++) {
			Plugin::ExecuteResponseMessage::Response *r = response_message.add_payload();
			r->CopyFrom(tmp.payload(i));
		}
	}
	response_message.SerializeToString(&response);
	if (found)
		return NSCAPI::cmd_return_codes::isSuccess;
	return NSCAPI::cmd_return_codes::returnIgnored;
}

NSCAPI::errorReturn NSClientT::register_submission_listener(unsigned int plugin_id, const char* channel) {
	channels_.register_listener(plugin_id, channel);
	return NSCAPI::api_return_codes::isSuccess;
}

NSCAPI::errorReturn NSClientT::send_notification(const char* channel, std::string &request, std::string &response) {
	boost::shared_lock<boost::shared_mutex> readLock(m_mutexRW, boost::get_system_time() + boost::posix_time::milliseconds(5000));
	if (!readLock.owns_lock()) {
		LOG_ERROR_CORE("FATAL ERROR: Could not get read-mutex.");
		return NSCAPI::api_return_codes::hasFailed;
	}

	std::string schannel = channel;

	bool found = false;
	BOOST_FOREACH(std::string cur_chan, str::utils::split_lst(schannel, std::string(","))) {
		if (cur_chan == "noop") {
			found = true;
			nscapi::protobuf::functions::create_simple_submit_response(cur_chan, "TODO", Plugin::Common_Result_StatusCodeType_STATUS_OK, "seems ok", response);
			continue;
		}
		if (cur_chan == "log") {
			Plugin::SubmitRequestMessage msg;
			msg.ParseFromString(request);
			for (int i = 0; i < msg.payload_size(); i++) {
				LOG_INFO_CORE("Logging notification: " + nscapi::protobuf::functions::query_data_to_nagios_string(msg.payload(i)));
			}
			found = true;
			nscapi::protobuf::functions::create_simple_submit_response(cur_chan, "TODO", Plugin::Common_Result_StatusCodeType_STATUS_OK, "seems ok", response);
			continue;
		}
		try {
			BOOST_FOREACH(nsclient::plugin_type p, channels_.get(cur_chan)) {
				try {
					p->handleNotification(cur_chan.c_str(), request, response);
				} catch (...) {
					LOG_ERROR_CORE("Plugin throw exception: " + p->get_alias_or_name());
				}
				found = true;
			}
		} catch (nsclient::plugins_list_exception &e) {
			LOG_ERROR_CORE("No handler for channel: " + schannel + ": " + utf8::utf8_from_native(e.what()));
			return NSCAPI::api_return_codes::hasFailed;
		} catch (const std::exception &e) {
			LOG_ERROR_CORE("No handler for channel: " + schannel + ": " + utf8::utf8_from_native(e.what()));
			return NSCAPI::api_return_codes::hasFailed;
		} catch (...) {
			LOG_ERROR_CORE("No handler for channel: " + schannel);
			return NSCAPI::api_return_codes::hasFailed;
		}
	}
	if (!found) {
		LOG_ERROR_CORE("No handler for channel: " + schannel + " channels: " + channels_.to_string());
		return NSCAPI::api_return_codes::hasFailed;
	}
	return NSCAPI::api_return_codes::isSuccess;
}


NSCAPI::errorReturn NSClientT::emit_event(const std::string &request) {
	Plugin::EventMessage em; em.ParseFromString(request);
	BOOST_FOREACH(const Plugin::EventMessage::Request &r, em.payload()) {
		try {
			BOOST_FOREACH(nsclient::plugin_type p, event_subscribers_.get(r.event())) {
				try {
					p->on_event(request);
				} catch (const std::exception &e) {
					LOG_ERROR_CORE("Failed to emit event to " + p->get_alias_or_name() + ": " + utf8::utf8_from_native(e.what()));
				} catch (...) {
					LOG_ERROR_CORE("Failed to emit event to " + p->get_alias_or_name() + ": UNKNOWN EXCEPTION");
				}
			}
		} catch (nsclient::plugins_list_exception &e) {
			LOG_ERROR_CORE("No handler for event: " + utf8::utf8_from_native(e.what()));
			return NSCAPI::api_return_codes::hasFailed;
		} catch (const std::exception &e) {
			LOG_ERROR_CORE("No handler for event: " + utf8::utf8_from_native(e.what()));
			return NSCAPI::api_return_codes::hasFailed;
		} catch (...) {
			LOG_ERROR_CORE("No handler for event");
			return NSCAPI::api_return_codes::hasFailed;
		}
	}
	return NSCAPI::api_return_codes::isSuccess;
}
NSClientT::plugin_type NSClientT::find_plugin(const unsigned int plugin_id) {
	boost::shared_lock<boost::shared_mutex> readLock(m_mutexRW, boost::get_system_time() + boost::posix_time::milliseconds(5000));
	if (!readLock.owns_lock()) {
		LOG_ERROR_CORE("FATAL ERROR: Could not get read-mutex.");
		return plugin_type();
	}
	BOOST_FOREACH(plugin_type plugin, plugins_) {
		if (plugin->get_id() == plugin_id)
			return plugin;
	}
	return plugin_type();
}

void NSClientT::remove_plugin(const std::string name) {
	boost::unique_lock<boost::shared_mutex> writeLock(m_mutexRW, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!writeLock.owns_lock()) {
		LOG_ERROR_CORE("FATAL ERROR: Could not get write-mutex.");
	} else {
		for (pluginList::iterator it = plugins_.begin(); it != plugins_.end();) {
			if ((*it)->getModule() == name) {
				plugin_type instance = *it;
				unsigned int plugin_id = instance->get_id();
				commands_.remove_plugin(plugin_id);
				metricsFetchers.remove_plugin(plugin_id);
				metricsSubmitetrs.remove_plugin(plugin_id);
				it = plugins_.erase(it);
				instance->unload_plugin();
				instance->unload_dll();
				if (it == plugins_.end())
					break;
			} else {
				++it;
			}
		}
	}
}
void NSClientT::load_plugin(const boost::filesystem::path &file, std::string alias) {
	try {
		plugin_type instance = addPlugin(file, alias);
		if (!instance) {
			LOG_DEBUG_CORE_STD("Failed to load " + file.string());
			return;
		}
		instance->load_plugin(NSCAPI::normalStart);
	} catch (const NSPluginException& e) {
		if (e.file().find("FileLogger") != std::string::npos) {
			LOG_DEBUG_CORE_STD("Failed to load " + file.string() + ": " + e.reason());
		} else {
			LOG_ERROR_CORE_STD("Failed to load " + file.string() + ": " + e.reason());
		}
	}
}
unsigned int NSClientT::add_plugin(unsigned int plugin_id) {
	boost::shared_lock<boost::shared_mutex> readLock(m_mutexRW, boost::get_system_time() + boost::posix_time::milliseconds(5000));
	if (readLock.owns_lock()) {
		plugin_type match;
		BOOST_FOREACH(plugin_type plugin, plugins_) {
			if (plugin->get_id() == plugin_id) {
				match = plugin;
			}
		}
		if (match) {
			int new_id = next_plugin_id_++;
			commands_.add_plugin(new_id, match);
			return new_id;
		} else {
			LOG_ERROR_CORE("Plugin not found.");
			return -1;
		}
	} else {
		LOG_ERROR_CORE("FATAL ERROR: Could not get read-mutex.");
		return -1;
	}
}


NSClientT::plugin_type NSClientT::find_plugin(const std::string key_ic) {
	std::string key = boost::to_lower_copy(key_ic);
	boost::shared_lock<boost::shared_mutex> readLock(m_mutexRW, boost::get_system_time() + boost::posix_time::milliseconds(5000));
	if (!readLock.owns_lock()) {
		LOG_ERROR_CORE("FATAL ERROR: Could not get read-mutex.");
		return plugin_type();
	}
	BOOST_FOREACH(plugin_type plugin, plugins_) {
		std::string s = boost::to_lower_copy(plugin->get_alias());
		if (s == key)
			return plugin;
		s = boost::to_lower_copy(plugin->getModule());
		if (s == key)
			return plugin;
	}
	return plugin_type();
}

std::string NSClientT::get_plugin_module_name(unsigned int plugin_id) {
	plugin_type plugin = find_plugin(plugin_id);
	if (!plugin)
		return "";
	return plugin->get_alias_or_name();
}
void NSClientT::listPlugins() {
	boost::shared_lock<boost::shared_mutex> readLock(m_mutexRW, boost::get_system_time() + boost::posix_time::milliseconds(5000));
	if (!readLock.owns_lock()) {
		LOG_ERROR_CORE("FATAL ERROR: Could not get read-mutex.");
		return;
	}
	for (pluginList::iterator it = plugins_.begin(); it != plugins_.end(); ++it) {
		try {
			if ((*it)->isBroken()) {
				std::cout << (*it)->getModule() << ": " << "broken" << std::endl;
			} else {
				std::cout << (*it)->getModule() << ": " << (*it)->getName() << std::endl;
			}
		} catch (NSPluginException e) {
			LOG_ERROR_CORE_STD("Could not load plugin: " + e.file() + ": " + e.reason());
		}
	}
}
#ifdef WIN32
boost::filesystem::path get_selfpath() {
	wchar_t buff[4096];
	if (GetModuleFileName(NULL, buff, sizeof(buff) - 1)) {
		boost::filesystem::path p = std::wstring(buff);
		return p.parent_path();
	}
	return boost::filesystem::initial_path();
}
#else
boost::filesystem::path get_selfpath() {
	char buff[1024];
	ssize_t len = ::readlink("/proc/self/exe", buff, sizeof(buff) - 1);
	if (len != -1) {
		buff[len] = '\0';
		boost::filesystem::path p = std::string(buff);
		return p.parent_path();
	}
	return boost::filesystem::initial_path();
}
#endif
boost::filesystem::path NSClientT::getBasePath(void) {
	boost::unique_lock<boost::timed_mutex> lock(internalVariables, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!lock.owns_lock()) {
		LOG_ERROR_CORE("FATAL ERROR: Could not get mutex.");
		return "";
	}
	if (!basePath.empty())
		return basePath;
	basePath = get_selfpath();
	try {
		settings_manager::get_core()->set_base(basePath);
	} catch (settings::settings_exception e) {
		LOG_ERROR_CORE_STD("Failed to set settings file: " + e.reason());
	} catch (...) {
		LOG_ERROR_CORE("Failed to set settings file");
	}
	return basePath;
}

#ifdef WIN32
typedef DWORD(WINAPI *PFGetTempPath)(__in DWORD nBufferLength, __out  LPTSTR lpBuffer);
#endif
boost::filesystem::path NSClientT::getTempPath() {
	boost::unique_lock<boost::timed_mutex> lock(internalVariables, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!lock.owns_lock()) {
		LOG_ERROR_CORE("FATAL ERROR: Could not get mutex.");
		return "";
	}
	if (!tempPath.empty())
		return tempPath;
#ifdef WIN32
	unsigned int buf_len = 4096;
	HMODULE hKernel = ::LoadLibrary(L"kernel32");
	if (hKernel) {
		// Find PSAPI functions
		PFGetTempPath FGetTempPath = (PFGetTempPath)::GetProcAddress(hKernel, "GetTempPathW");
		if (FGetTempPath) {
			wchar_t* buffer = new wchar_t[buf_len + 1];
			if (FGetTempPath(buf_len, buffer)) {
				tempPath = buffer;
			}
			delete[] buffer;
		}
	}
#else
	tempPath = "/tmp";
#endif
	return tempPath;
}

// Service API
NSClient* NSClientT::get_global_instance() {
	return mainClient;
}
void NSClientT::handle_startup(std::string service_name) {
	LOG_DEBUG_CORE("Starting: " + utf8::cvt<std::string>(service_name));
	service_name_ = service_name;
	boot_init();
	boot_load_all_plugins();
	boot_start_plugins(true);
	LOG_DEBUG_CORE("Starting: DONE");
	/*
		DWORD dwSessionId = remote_processes::getActiveSessionId();
		if (dwSessionId != 0xFFFFFFFF)
			tray_starter::start(dwSessionId);
		else
			LOG_ERROR_STD(_T("Failed to start tray helper:" ) + error::lookup::last_error());
			*/
}
void NSClientT::handle_shutdown(std::string service_name) {
	stop_unload_plugins_pre();
	stop_exit_pre();
	stop_exit_post();
}

NSClientT::service_controller NSClientT::get_service_control() {
	return service_controller(service_name_);
}

void NSClientT::service_controller::stop() {
#ifdef WIN32
	serviceControll::StopNoWait(utf8::cvt<std::wstring>(get_service_name()));
#endif
}
void NSClientT::service_controller::start() {
#ifdef WIN32
	serviceControll::Start(utf8::cvt<std::wstring>(get_service_name()));
#endif
}
bool NSClientT::service_controller::is_started() {
#ifdef WIN32
	try {
		if (serviceControll::isStarted(utf8::cvt<std::wstring>(get_service_name()))) {
			return true;
		}
	} catch (...) {
		return false;
	}
#endif
	return false;
}

#ifdef WIN32
#ifndef CSIDL_COMMON_APPDATA
#define CSIDL_COMMON_APPDATA 0x0023
#endif
#ifndef CSIDL_APPDATA
#define CSIDL_APPDATA 0x001a
#endif
typedef BOOL(WINAPI *fnSHGetSpecialFolderPath)(HWND hwndOwner, LPTSTR lpszPath, int nFolder, BOOL fCreate);

__inline BOOL WINAPI _SHGetSpecialFolderPath(HWND hwndOwner, LPTSTR lpszPath, int nFolder, BOOL fCreate) {
	static fnSHGetSpecialFolderPath __SHGetSpecialFolderPath = NULL;
	if (!__SHGetSpecialFolderPath) {
		HMODULE hDLL = LoadLibrary(_T("shfolder.dll"));
		if (hDLL != NULL)
			__SHGetSpecialFolderPath = (fnSHGetSpecialFolderPath)GetProcAddress(hDLL, "SHGetSpecialFolderPathW");
	}
	if (__SHGetSpecialFolderPath)
		return __SHGetSpecialFolderPath(hwndOwner, lpszPath, nFolder, fCreate);
	return FALSE;
}
#endif

typedef std::map<std::string, std::string> paths_type;
paths_type paths;

std::string NSClientT::getFolder(std::string key) {
	paths_type::const_iterator p = paths.find(key);
	if (p != paths.end())
		return p->second;
	std::string default_value = getBasePath().string();
	if (key == "certificate-path") {
		default_value = CERT_FOLDER;
	} else if (key == "module-path") {
		default_value = MODULE_FOLDER;
	} else if (key == "web-path") {
		default_value = WEB_FOLDER;
	} else if (key == "scripts") {
		default_value = SCRIPTS_FOLDER;
	} else if (key == "log-path") {
		default_value = LOG_FOLDER;
	} else if (key == CACHE_FOLDER_KEY) {
		default_value = DEFAULT_CACHE_PATH;
	} else if (key == CRASH_ARCHIVE_FOLDER_KEY) {
		default_value = "${shared-path}/crash-dumps";
	} else if (key == "base-path") {
		default_value = getBasePath().string();
	} else if (key == "temp") {
		default_value = getTempPath().string();
	} else if (key == "shared-path" || key == "base-path" || key == "exe-path") {
		default_value = getBasePath().string();
	}
#ifdef WIN32
	else if (key == "common-appdata") {
		wchar_t buf[MAX_PATH + 1];
		if (_SHGetSpecialFolderPath(NULL, buf, CSIDL_COMMON_APPDATA, FALSE))
			default_value = utf8::cvt<std::string>(buf);
		else
			default_value = getBasePath().string();
	} else if (key == "appdata") {
		wchar_t buf[MAX_PATH + 1];
		if (_SHGetSpecialFolderPath(NULL, buf, CSIDL_APPDATA, FALSE))
			default_value = utf8::cvt<std::string>(buf);
		else
			default_value = getBasePath().string();
	}
#else
	else if (key == "etc") {
		default_value = "/etc";
	}
#endif
	try {
		if (settings_manager::get_core()->is_ready()) {
			std::string path = settings_manager::get_settings()->get_string(CONFIG_PATHS, key, default_value);
			settings_manager::get_core()->register_key(0xffff, CONFIG_PATHS, key, settings::settings_core::key_string, "Path for " + key, "", default_value, false, false);
			paths[key] = path;
			return path;
		} else {
			LOG_DEBUG_CORE("Settings not ready so we cant lookup: " + key);
		}
	} catch (const settings::settings_exception &e) {
		// TODO: Maybe this should be fixed!
		paths[key] = default_value;
	}
	return default_value;
}

std::string NSClientT::expand_path(std::string file) {
	try {
		if (file.empty())
			return file;
		parsers::simple_expression::result_type expr;
		parsers::simple_expression::parse(file, expr);

		std::string ret;
		BOOST_FOREACH(const parsers::simple_expression::entry &e, expr) {
			if (!e.is_variable)
				ret += e.name;
			else
				ret += expand_path(getFolder(e.name));
		}
		return ret;
	} catch (...) {
		LOG_ERROR_CORE("Failed to expand path: " + utf8::cvt<std::string>(file));
		return "";
	}
}

struct metrics_fetcher {
	Plugin::MetricsMessage result;
	std::string buffer;
	metrics_fetcher() {
		result.add_payload();
	}

	Plugin::MetricsMessage::Response* get_root() {
		return result.mutable_payload(0);
	}
	void add_bundle(const Plugin::Common::MetricsBundle &b) {
		get_root()->add_bundles()->CopyFrom(b);
	}
	void fetch(nsclient::plugin_type p) {
		std::string buffer;
		p->fetchMetrics(buffer);
		Plugin::MetricsMessage payload;
		payload.ParseFromString(buffer);
		BOOST_FOREACH(const Plugin::MetricsMessage::Response &r, payload.payload()) {
			BOOST_FOREACH(const Plugin::Common::MetricsBundle &b, r.bundles()) {
				add_bundle(b);
			}
		}
	}
	void render() {
		get_root()->mutable_result()->set_code(Plugin::Common_Result_StatusCodeType_STATUS_OK);
		buffer = result.SerializeAsString();
	}
	void digest(nsclient::plugin_type p) {
		p->submitMetrics(buffer);
	}
};

void NSClientT::ownMetricsFetcher(Plugin::MetricsMessage::Response *response) {
	Plugin::Common::MetricsBundle *bundle = response->add_bundles();
	bundle->set_key("workers");
	if (scheduler_.get_scheduler().has_metrics()) {
		boost::uint64_t taskes__ = scheduler_.get_scheduler().get_metric_executed();
		boost::uint64_t submitted__ = scheduler_.get_scheduler().get_metric_compleated();
		boost::uint64_t errors__ = scheduler_.get_scheduler().get_metric_errors();
		boost::uint64_t threads = scheduler_.get_scheduler().get_metric_threads();

		Plugin::Common::Metric *m = bundle->add_value();
		m->set_key("jobs");
		m->mutable_value()->set_int_data(taskes__);
		m = bundle->add_value();
		m->set_key("submitted");
		m->mutable_value()->set_int_data(submitted__);
		m = bundle->add_value();
		m->set_key("errors");
		m->mutable_value()->set_int_data(errors__);
		m = bundle->add_value();
		m->set_key("threads");
		m->mutable_value()->set_int_data(threads);
	} else {
		Plugin::Common::Metric *m = bundle->add_value();
		m->set_key("metrics.available");
		m->mutable_value()->set_string_data("false");
	}
}
void NSClientT::process_metrics() {
	metrics_fetcher f;

	metricsFetchers.do_all(boost::bind(&metrics_fetcher::fetch, &f, _1));
	ownMetricsFetcher(f.get_root());
	f.render();
	metricsSubmitetrs.do_all(boost::bind(&metrics_fetcher::digest, &f, _1));
}

#ifdef _WIN32
void NSClientT::handle_session_change(unsigned long dwSessionId, bool logon) {}
#endif
