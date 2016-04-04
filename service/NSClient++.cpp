///////////////////////////////////////////////////////////////////////////
// NSClient++ Base Service
//
// Copyright (c) 2004 MySolutions NORDIC (http://www.medin.name)
//
// Date: 2004-03-13
// Author: Michael Medin (michael@medin.name)
//
// Part of this file is based on work by Bruno Vais (bvais@usa.net)
//
// This software is provided "AS IS", without a warranty of any kind.
// You are free to use/modify this code but leave this header intact.
//
//////////////////////////////////////////////////////////////////////////
#include "NSClient++.h"
#include <settings/settings_core.hpp>
#include <charEx.h>
#include <config.h>
#include <common.hpp>
#ifdef WIN32
#include <Userenv.h>
#include <Lmcons.h>
#include <shellapi.h>
#endif

#include <boost/unordered_set.hpp>
#include <timer.hpp>
#include <file_helpers.hpp>

#include "core_api.h"
#include "../libs/settings_manager/settings_manager_impl.h"
#include <settings/config.hpp>
#include <nscapi/functions.hpp>
#include <parsers/expression/expression.hpp>

#include <nscapi/nscapi_settings_helper.hpp>
#include "cli_parser.hpp"
#include <version.hpp>

#include <config.h>

#include <json_spirit.h>

#ifdef WIN32
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

#define LOG_CRITICAL_CORE(msg) { nsclient::logging::logger::get_logger()->fatal("core", __FILE__, __LINE__, msg);}
#define LOG_CRITICAL_CORE_STD(msg) LOG_CRITICAL_CORE(std::string(msg))
#define LOG_ERROR_CORE(msg) { nsclient::logging::logger::get_logger()->error("core", __FILE__, __LINE__, msg);}
#define LOG_ERROR_CORE_STD(msg) LOG_ERROR_CORE(std::string(msg))
#define LOG_INFO_CORE(msg) { nsclient::logging::logger::get_logger()->info("core", __FILE__, __LINE__, msg);}
#define LOG_INFO_CORE_STD(msg) LOG_INFO_CORE(std::string(msg))
#define LOG_DEBUG_CORE(msg) { nsclient::logging::logger::get_logger()->debug("core", __FILE__, __LINE__, msg);}
#define LOG_DEBUG_CORE_STD(msg) LOG_DEBUG_CORE(std::string(msg))
#define IS_LOG_DEBUG_CORE(msg) { if (nsclient::logging::logger::get_logger()->should_log(NSCAPI::log_level::debug))
#define LOG_TRACE_CORE(msg) { nsclient::logging::logger::get_logger()->trace("core", __FILE__, __LINE__, msg);}
#define LOG_TRACE_CORE_STD(msg) LOG_DEBUG_CORE(std::string(msg))
#define IS_LOG_TRACE_CORE(msg) if (nsclient::logging::logger::get_logger()->should_log(NSCAPI::log_level::trace))

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

NSClientT::NSClientT()
	: enable_shared_session_(false)
	, next_plugin_id_(0)
	, service_name_(DEFAULT_SERVICE_NAME) {
	nsclient::logging::logger::startup();
}

NSClientT::~NSClientT() {
	try {
		nsclient::logging::logger::destroy();
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

//////////////////////////////////////////////////////////////////////////
// Service functions

struct nscp_settings_provider : public settings_manager::provider_interface {
	virtual std::string expand_path(std::string file) {
		return mainClient->expand_path(file);
	}
	std::string get_data(std::string key) {
		// TODO
		return "";
	}
};

nscp_settings_provider provider;

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

	if (!settings_manager::init_settings(&provider, context_)) {
		return false;
	}

	nsclient::logging::logger::configure();

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
			("shared session", "SHRED SESSION", "Section for configuring the shared session.")
			("crash", "CRASH HANDLER", "Section for configuring the crash handler.")
			;

		settings.add_key_to_settings("log")
			("level", sh::string_key(&log_level, "info"),
				"LOG LEVEL", "Log level to use. Available levels are error,warning,info,debug,trace")
			;

		settings.add_key_to_settings("shared session")
			("enabled", sh::bool_key(&enable_shared_session_, false),
				"ENABLE THE SAHRED SESSION", "This is currently not added in 0.4.x")
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
	if (!override_log)
		nsclient::logging::logger::set_log_level(log_level);

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

	if (enable_shared_session_) {
		LOG_INFO_CORE("shared session not ported yet!...");
		// 		if (boot) {
		// 			LOG_INFO_CORE(_T("shared session not ported yet!..."));
		// 			try {
		// 				shared_server_.reset(new nsclient_session::shared_server_session(this));
		// 				if (!shared_server_->session_exists()) {
		// 					shared_server_->create_new_session();
		// 				} else {
		// 					LOG_ERROR_STD(_T("Session already exists cant create a new one!"));
		// 				}
		// 				startTrayIcons();
		// 			} catch (nsclient_session::session_exception e) {
		// 				LOG_ERROR_STD(_T("Failed to create new session: ") + e.what());
		// 				shared_server_.reset(NULL);
		// 			} catch (...) {
		// 				LOG_ERROR_STD(_T("Failed to create new session: Unknown exception"));
		// 				shared_server_.reset(NULL);
		// 			}
		// 		} else {
		// 			LOG_INFO_CORE(_T("shared session not ported yet!..."));
		// 			try {
		// 				std::wstring id = _T("_attached_") + strEx::itos(GetCurrentProcessId()) + _T("_");
		// 				shared_client_.reset(new nsclient_session::shared_client_session(id, this));
		// 				if (shared_client_->session_exists()) {
		// 					shared_client_->attach_to_session(id);
		// 				} else {
		// 					LOG_ERROR_STD(_T("No session was found cant attach!"));
		// 				}
		// 				LOG_ERROR_STD(_T("Session is: ") + shared_client_->get_client_id());
		// 			} catch (nsclient_session::session_exception e) {
		// 				LOG_ERROR_STD(_T("Failed to attach to session: ") + e.what());
		// 				shared_client_.reset(NULL);
		// 			} catch (...) {
		// 				LOG_ERROR_STD(_T("Failed to attach to session: Unknown exception"));
		// 				shared_client_.reset(NULL);
		// 			}
		// 		}
	}
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
/*
void NSClientT::startTrayIcons() {
// 	if (shared_server_.get() == NULL) {
// 		LOG_MESSAGE_STD(_T("No master session so tray icons not started"));
// 		return;
// 	}
// 	remote_processes::PWTS_SESSION_INFO list;
// 	DWORD count;
// 	if (!remote_processes::_WTSEnumerateSessions(WTS_CURRENT_SERVER_HANDLE , 0, 1, &list, &count)) {
// 		LOG_ERROR_STD(_T("Failed to enumerate sessions:" ) + error::lookup::last_error());
// 	} else {
// 		LOG_DEBUG_STD(_T("Found ") + strEx::itos(count) + _T(" sessions"));
// 		for (DWORD i=0;i<count;i++) {
// 			LOG_DEBUG_STD(_T("Found session: ") + strEx::itos(list[i].SessionId) + _T(" state: ") + strEx::itos(list[i].State));
// 			if (list[i].State == remote_processes::_WTS_CONNECTSTATE_CLASS::WTSActive) {
// 				startTrayIcon(list[i].SessionId);
// 			}
// 		}
// 	}
}
void NSClientT::startTrayIcon(DWORD dwSessionId) {
// 	if (shared_server_.get() == NULL) {
// 		LOG_MESSAGE_STD(_T("No master session so tray icons not started"));
// 		return;
// 	}
// 	if (!shared_server_->re_attach_client(dwSessionId)) {
// 		if (!tray_starter::start(dwSessionId)) {
// 			LOG_ERROR_STD(_T("Failed to start session (") + strEx::itos(dwSessionId) + _T("): " ) + error::lookup::last_error());
// 		}
// 	}
}
*/
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
	/*
	LOG_DEBUG_STD(_T("Stopping: Socket Helpers"));
	try {
		simpleSocket::WSACleanup();
	} catch (simpleSocket::SocketException e) {
		LOG_ERROR_STD(_T("Socket exception: ") + e.getMessage());
	} catch (...) {
		LOG_ERROR_STD(_T("Unknown exception uniniating socket..."));
	}
	*/
	LOG_DEBUG_CORE("Stopping: Settings instance");
	settings_manager::destroy_settings();
	// 	try {
	// 		if (shared_client_.get() != NULL) {
	// 			LOG_DEBUG_STD(_T("Stopping: shared client"));
	// 			shared_client_->set_handler(NULL);
	// 			shared_client_->close_session();
	// 		}
	// 	} catch(nsclient_session::session_exception &e) {
	// 		LOG_ERROR_STD(_T("Exception closing shared client session: ") + e.what());
	// 	} catch(...) {
	// 		LOG_ERROR_STD(_T("Exception closing shared client session: Unknown exception!"));
	// 	}
	// 	try {
	// 		if (shared_server_.get() != NULL) {
	// 			LOG_DEBUG_STD(_T("Stopping: shared server"));
	// 			shared_server_->set_handler(NULL);
	// 			shared_server_->close_session();
	// 		}
	// 	} catch(...) {
	// 		LOG_ERROR_CORE("UNknown exception raised: When closing shared session");
	// 	}
	return true;
}
bool NSClientT::stop_exit_post() {
	try {
		nsclient::logging::logger::shutdown();
		google::protobuf::ShutdownProtobufLibrary();
	} catch (...) {
		LOG_ERROR_CORE("UNknown exception raised: When closing shared session");
	}
	return true;
}
/*
void NSClientT::service_on_session_changed(unsigned long dwSessionId, bool logon, unsigned long dwEventType) {
// 	if (shared_server_.get() == NULL) {
// 		LOG_DEBUG_STD(_T("No shared session: ignoring change event!"));
// 		return;
// 	}
// 	LOG_DEBUG_CORE_STD(_T("Got session change: ") + strEx::itos(dwSessionId));
// 	if (!logon) {
// 		LOG_DEBUG_CORE_STD(_T("Not a logon event: ") + strEx::itos(dwEventType));
// 		return;
// 	}
// 	tray_starter::start(dwSessionId);
}
*/
//////////////////////////////////////////////////////////////////////////
// Member functions

/**
 * Unload all plug-ins (in reversed order)
 */
void NSClientT::unloadPlugins() {
	nsclient::logging::logger::clear_subscribers();
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
		routers_.remove_all();
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
}

void NSClientT::loadPlugins(NSCAPI::moduleLoadMode mode) {
	{
		std::set<long> broken;
		{
			boost::shared_lock<boost::shared_mutex> readLock(m_mutexRW, boost::get_system_time() + boost::posix_time::milliseconds(5000));
			if (!readLock.owns_lock()) {
				LOG_ERROR_CORE("FATAL ERROR: Could not get read-mutex.");
				return;
			}
			BOOST_FOREACH(plugin_type &plugin, plugins_) {
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
				LOG_DEBUG_CORE_STD("Found duplicate plugin returning old " + strEx::s::xtos(plug->get_id()));
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
		if (plugin->hasMessageHandler())
			nsclient::logging::logger::subscribe_raw(plugin);
		if (plugin->has_routing_handler())
			routers_.add_plugin(plugin);
		settings_manager::get_core()->register_key(0xffff, MAIN_MODULES_SECTION, plugin->getModule(), settings::settings_core::key_string, plugin->getName(), plugin->getDescription(), "0", false, false);
	}
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

NSCAPI::nagiosReturn NSClientT::inject(std::string command, std::string arguments, std::string &msg, std::string & perf) {
	std::list<std::string> args;
	strEx::s::parse_command(arguments, args);
	std::string request, response;
	nscapi::protobuf::functions::create_simple_query_request(command, args, request);
	NSCAPI::nagiosReturn ret = injectRAW(request, response);
	if (ret == NSCAPI::cmd_return_codes::hasFailed && response.empty()) {
		msg = "Failed to execute: " + command;
		return NSCAPI::query_return_codes::returnUNKNOWN;
	} else if (response.empty()) {
		msg = "No data returned from: " + command;
		return NSCAPI::query_return_codes::returnUNKNOWN;
	}
	return nscapi::protobuf::functions::parse_simple_query_response(response, msg, perf);
}

struct command_chunk {
	nsclient::commands::plugin_type plugin;
	Plugin::QueryRequestMessage request;
};

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
NSCAPI::nagiosReturn NSClientT::injectRAW(std::string &request, std::string &response) {
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
				strEx::append_list(missing_commands, command);
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
					strEx::append_list(missing_commands, payload->command());
				}
			}
		}

		if (command_chunks.size() == 0) {
			LOG_ERROR_CORE("Unknown command(s): " + missing_commands + " available commands: " + commands_.to_string());
			nscapi::protobuf::functions::create_simple_header(response_message.mutable_header());
			Plugin::QueryResponseMessage::Response *payload = response_message.add_payload();
			payload->set_command(missing_commands);
			nscapi::protobuf::functions::set_response_bad(*payload, "Unknown command(s): " + missing_commands);
			response = response_message.SerializeAsString();
			return NSCAPI::cmd_return_codes::hasFailed;
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
	nscapi::protobuf::functions::create_simple_header(response_message.mutable_header());

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

NSCAPI::errorReturn NSClientT::reroute(std::string &channel, std::string &buffer) {
	BOOST_FOREACH(nsclient::plugin_type p, routers_.get(channel)) {
		char *new_channel_buffer;
		char *new_buffer;
		unsigned int new_buffer_len;
		int status = p->route_message(channel.c_str(), buffer.c_str(), buffer.size(), &new_channel_buffer, &new_buffer, &new_buffer_len);
		if ((status&NSCAPI::message::modified) == NSCAPI::message::modified) {
			buffer = std::string(new_buffer, new_buffer_len);
			p->deleteBuffer(&new_buffer);
		}
		if ((status&NSCAPI::message::routed) == NSCAPI::message::routed) {
			channel = new_channel_buffer;
			//p->deleteBuffer(new_channel_buffer);
			return NSCAPI::message::routed;
		}
		if ((status&NSCAPI::message::ignored) == NSCAPI::message::ignored)
			return NSCAPI::message::ignored;
		if ((status&NSCAPI::message::digested) == NSCAPI::message::digested)
			return NSCAPI::message::ignored;
	}
	return NSCAPI::message::hasFailed;
}

NSCAPI::errorReturn NSClientT::register_submission_listener(unsigned int plugin_id, const char* channel) {
	channels_.register_listener(plugin_id, channel);
	return NSCAPI::api_return_codes::isSuccess;
}
NSCAPI::errorReturn NSClientT::register_routing_listener(unsigned int plugin_id, const char* channel) {
	try {
		routers_.register_listener(plugin_id, channel);
	} catch (const std::exception &e) {
		LOG_ERROR_CORE("Failed to register channel: " + utf8::cvt<std::string>(channel) + ": " + utf8::utf8_from_native(e.what()));
		return NSCAPI::api_return_codes::hasFailed;
	}
	return NSCAPI::api_return_codes::isSuccess;
}

NSCAPI::errorReturn NSClientT::send_notification(const char* channel, std::string &request, std::string &response) {
	boost::shared_lock<boost::shared_mutex> readLock(m_mutexRW, boost::get_system_time() + boost::posix_time::milliseconds(5000));
	if (!readLock.owns_lock()) {
		LOG_ERROR_CORE("FATAL ERROR: Could not get read-mutex.");
		return NSCAPI::api_return_codes::hasFailed;
	}

	std::string schannel = channel;
	try {
		int count = 0;
		while (reroute(schannel, request) == NSCAPI::message::routed && count++ <= 10) {
			LOG_DEBUG_CORE_STD("Re-routing message to: " + schannel);
		}
		if (count >= 10) {
			LOG_ERROR_CORE("More then 10 routes, discarding message...");
			return NSCAPI::api_return_codes::hasFailed;
		}
	} catch (nsclient::plugins_list_exception &e) {
		LOG_ERROR_CORE("Error routing channel: " + schannel + ": " + utf8::utf8_from_native(e.what()));
		return NSCAPI::api_return_codes::hasFailed;
	} catch (...) {
		LOG_ERROR_CORE("Error routing channel: " + schannel);
		return NSCAPI::api_return_codes::hasFailed;
	}

	bool found = false;
	BOOST_FOREACH(std::string cur_chan, strEx::s::splitEx(schannel, std::string(","))) {
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
	HMODULE hKernel = ::LoadLibrary(_TEXT("kernel32"));
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
}

typedef std::map<unsigned int, std::string> modules_type;

inline std::string add_plugin_data(modules_type module_cache, unsigned int plugin_id, NSClientT *instance) {
	modules_type::const_iterator it = module_cache.find(plugin_id);
	std::string module;
	if (it == module_cache.end()) {
		module = utf8::cvt<std::string>(instance->get_plugin_module_name(plugin_id));
		module_cache[plugin_id] = module;
		return module;
	} else {
		return it->second;
	}
}

void settings_add_plugin_data(const std::set<unsigned int> &plugins, modules_type module_cache, ::Plugin::Settings_Information* info, NSClientT *instance) {
	BOOST_FOREACH(unsigned int i, plugins) {
		info->add_plugin(add_plugin_data(module_cache, i, instance));
	}
}

NSCAPI::errorReturn NSClientT::settings_query(const char *request_buffer, const unsigned int request_buffer_len, char **response_buffer, unsigned int *response_buffer_len) {
	std::string response_string;
	Plugin::SettingsRequestMessage request;
	Plugin::SettingsResponseMessage response;
	nscapi::protobuf::functions::create_simple_header(response.mutable_header());
	modules_type module_cache;
	request.ParseFromArray(request_buffer, request_buffer_len);
	timer t;
	for (int i = 0; i < request.payload_size(); i++) {
		Plugin::SettingsResponseMessage::Response* rp = response.add_payload();
		try {
			const Plugin::SettingsRequestMessage::Request &r = request.payload(i);
			if (r.has_inventory()) {
				const Plugin::SettingsRequestMessage::Request::Inventory &q = r.inventory();
				boost::optional<unsigned int> plugin_id;
				if (q.has_plugin()) {
					plugin_type p = find_plugin(q.plugin());
					if (p)
						plugin_id = p->get_id();
				}
				if (q.node().has_key()) {
					t.start("fetching key");
					settings::settings_core::key_description desc = settings_manager::get_core()->get_registred_key(q.node().path(), q.node().key());
					t.end();
					Plugin::SettingsResponseMessage::Response::Inventory *rpp = rp->add_inventory();
					rpp->mutable_node()->CopyFrom(q.node());
					rpp->mutable_info()->set_title(desc.title);
					rpp->mutable_info()->set_description(desc.description);
				} else {
					bool fetch_samples = q.fetch_samples();
					if (q.recursive_fetch()) {
						std::string base_path;
						if (q.node().has_path())
							base_path = q.node().path();
						t.start("fetching paths");
						std::list<std::string> list = settings_manager::get_core()->get_reg_sections(base_path, fetch_samples);
						t.end();
						BOOST_FOREACH(const std::string &path, list) {
							if (q.fetch_keys()) {
								t.start("fetching keys");
								std::list<std::string> klist = settings_manager::get_core()->get_reg_keys(path, fetch_samples);
								t.end();
								boost::unordered_set<std::string> cache;
								BOOST_FOREACH(const std::string &key, klist) {
									settings::settings_core::key_description desc = settings_manager::get_core()->get_registred_key(path, key);
									if (plugin_id && !desc.has_plugin(*plugin_id))
										continue;
									Plugin::SettingsResponseMessage::Response::Inventory *rpp = rp->add_inventory();
									cache.emplace(key);
									rpp->mutable_node()->set_path(path);
									rpp->mutable_node()->set_key(key);
									rpp->mutable_info()->set_title(desc.title);
									rpp->mutable_info()->set_description(desc.description);
									rpp->mutable_info()->set_advanced(desc.advanced);
									rpp->mutable_info()->set_sample(desc.is_sample);
									rpp->mutable_info()->mutable_default_value()->set_string_data(desc.defValue);
									if (desc.type == NSCAPI::key_string) {
										settings::settings_interface::op_string val = settings_manager::get_settings()->get_string(path, key);
										if (val)
											rpp->mutable_value()->set_string_data(*val);
									} else if (desc.type == NSCAPI::key_integer) {
										settings::settings_interface::op_int val = settings_manager::get_settings()->get_int(path, key);
										if (val)
											rpp->mutable_value()->set_int_data(*val);
									} else if (desc.type == NSCAPI::key_bool) {
										settings::settings_interface::op_bool val = settings_manager::get_settings()->get_bool(path, key);
										if (val)
											rpp->mutable_value()->set_bool_data(*val);
									} else {
										LOG_ERROR_CORE("Invalid type");
									}
									settings_add_plugin_data(desc.plugins, module_cache, rpp->mutable_info(), this);
								}
								if (!plugin_id) {
									t.start("fetching more keys");
									klist = settings_manager::get_settings()->get_keys(path);
									t.end();
									BOOST_FOREACH(const std::string &key, klist) {
										if (cache.find(key) == cache.end()) {
											Plugin::SettingsResponseMessage::Response::Inventory *rpp = rp->add_inventory();
											rpp->mutable_node()->set_path(path);
											rpp->mutable_node()->set_key(key);
											rpp->mutable_info()->set_advanced(true);
											rpp->mutable_info()->set_sample(false);
											rpp->mutable_info()->mutable_default_value()->set_string_data("");
											settings::settings_interface::op_string val = settings_manager::get_settings()->get_string(path, key);
											if (val)
												rpp->mutable_value()->set_string_data(*val);
										}
									}
								}
							}
							if (q.fetch_paths()) {
								t.start("fetching path");
								settings::settings_core::path_description desc = settings_manager::get_core()->get_registred_path(path);
								t.end();
								Plugin::SettingsResponseMessage::Response::Inventory *rpp = rp->add_inventory();
								rpp->mutable_node()->set_path(path);
								rpp->mutable_info()->set_title(desc.title);
								rpp->mutable_info()->set_description(desc.description);
								rpp->mutable_info()->set_advanced(desc.advanced);
								rpp->mutable_info()->set_sample(desc.is_sample);
								settings_add_plugin_data(desc.plugins, module_cache, rpp->mutable_info(), this);
							}
						}
					} else {
						std::string path = q.node().path();
						if (q.fetch_keys()) {
							t.start("fetching keys");
							std::list<std::string> list = settings_manager::get_core()->get_reg_keys(path, fetch_samples);
							t.end();
							boost::unordered_set<std::string> cache;
							BOOST_FOREACH(const std::string &key, list) {
								t.start("fetching keys");
								settings::settings_core::key_description desc = settings_manager::get_core()->get_registred_key(path, key);
								if (plugin_id && !desc.has_plugin(*plugin_id))
									continue;
								t.end();
								Plugin::SettingsResponseMessage::Response::Inventory *rpp = rp->add_inventory();
								cache.emplace(key);
								rpp->mutable_node()->set_path(path);
								rpp->mutable_node()->set_key(key);
								rpp->mutable_info()->set_title(desc.title);
								rpp->mutable_info()->set_description(desc.description);
								rpp->mutable_info()->set_advanced(desc.advanced);
								rpp->mutable_info()->set_sample(desc.is_sample);
								rpp->mutable_info()->mutable_default_value()->set_string_data(desc.defValue);
								try {
									if (desc.type == NSCAPI::key_string)
										rpp->mutable_value()->set_string_data(settings_manager::get_settings()->get_string(path, key, ""));
									else if (desc.type == NSCAPI::key_integer)
										rpp->mutable_value()->set_int_data(settings_manager::get_settings()->get_int(path, key, 0));
									else if (desc.type == NSCAPI::key_bool)
										rpp->mutable_value()->set_bool_data(settings_manager::get_settings()->get_bool(path, key, false));
									else {
										LOG_ERROR_CORE("Invalid type");
									}
								} catch (settings::settings_exception &) {}
								settings_add_plugin_data(desc.plugins, module_cache, rpp->mutable_info(), this);
							}
							if (!plugin_id) {
								t.start("fetching more keys");
								list = settings_manager::get_settings()->get_keys(path);
								t.end();
								BOOST_FOREACH(const std::string &key, list) {
									if (cache.find(key) == cache.end()) {
										Plugin::SettingsResponseMessage::Response::Inventory *rpp = rp->add_inventory();
										rpp->mutable_node()->set_path(path);
										rpp->mutable_node()->set_key(key);
										rpp->mutable_info()->set_advanced(true);
										rpp->mutable_info()->set_sample(false);
										settings::settings_interface::op_string val = settings_manager::get_settings()->get_string(path, key);
										if (val)
											rpp->mutable_value()->set_string_data(*val);
									}
								}
							}
						}
						if (q.fetch_paths()) {
							t.start("fetching paths");
							settings::settings_core::path_description desc = settings_manager::get_core()->get_registred_path(path);
							t.end();
							Plugin::SettingsResponseMessage::Response::Inventory *rpp = rp->add_inventory();
							rpp->mutable_node()->set_path(path);
							rpp->mutable_info()->set_title(desc.title);
							rpp->mutable_info()->set_description(desc.description);
							rpp->mutable_info()->set_advanced(desc.advanced);
							rpp->mutable_info()->set_sample(desc.is_sample);
							settings_add_plugin_data(desc.plugins, module_cache, rpp->mutable_info(), this);
						}
					}
					if (q.fetch_templates()) {
						t.start("fetching templates");
						BOOST_FOREACH(const settings::settings_core::tpl_description &desc, settings_manager::get_core()->get_registred_tpls()) {
							Plugin::SettingsResponseMessage::Response::Inventory *rpp = rp->add_inventory();
							rpp->mutable_node()->set_path(desc.path);
							rpp->mutable_info()->set_title(desc.title);
							rpp->mutable_info()->set_is_template(true);
							rpp->mutable_value()->set_string_data(desc.data);
							rpp->mutable_info()->add_plugin(add_plugin_data(module_cache, desc.plugin_id, this));
						}
						t.end();
					}
					rp->mutable_result()->set_code(Plugin::Common_Result_StatusCodeType_STATUS_OK);
				}
			} else if (r.has_query()) {
				const Plugin::SettingsRequestMessage::Request::Query &q = r.query();
				Plugin::SettingsResponseMessage::Response::Query *rpp = rp->mutable_query();
				rpp->mutable_node()->CopyFrom(q.node());
				if (q.node().has_key()) {
					if (q.type() == Plugin::Common_DataType_STRING)
						rpp->mutable_value()->set_string_data(settings_manager::get_settings()->get_string(q.node().path(), q.node().key(), q.default_value().string_data()));
					else if (q.type() == Plugin::Common_DataType_INT)
						rpp->mutable_value()->set_int_data(settings_manager::get_settings()->get_int(q.node().path(), q.node().key(), q.default_value().int_data()));
					else if (q.type() == Plugin::Common_DataType_BOOL)
						rpp->mutable_value()->set_bool_data(settings_manager::get_settings()->get_bool(q.node().path(), q.node().key(), q.default_value().bool_data()));
					else {
						LOG_ERROR_CORE("Invalid type");
					}
				} else {
					::Plugin::Common::AnyDataType *value = rpp->mutable_value();
					if (q.has_recursive() && q.recursive()) {
						BOOST_FOREACH(const std::string &key, settings_manager::get_settings()->get_sections(q.node().path())) {
							value->add_list_data(key);
						}
					} else {
						BOOST_FOREACH(const std::string &key, settings_manager::get_settings()->get_keys(q.node().path())) {
							value->add_list_data(key);
						}
					}
				}
				rp->mutable_result()->set_code(Plugin::Common_Result_StatusCodeType_STATUS_OK);
			} else if (r.has_registration()) {
				const Plugin::SettingsRequestMessage::Request::Registration &q = r.registration();
				rp->mutable_registration();
				if (q.has_fields()) {
					json_spirit::Object node;

					try {
						json_spirit::Value value;
						std::string data = q.fields();
						json_spirit::read_or_throw(data, value);
						if (value.isObject())
							node = value.getObject();
					} catch (const std::exception &e) {
						LOG_ERROR_CORE(std::string("Failed to process fields for ") + e.what());
					} catch (const json_spirit::ParseError &e) {
						LOG_ERROR_CORE(std::string("Failed to process fields for ") + e.reason_ + " @ " + strEx::s::xtos(e.line_) + ":" + strEx::s::xtos(e.column_));
					} catch (...) {
						LOG_ERROR_CORE("Failed to process fields for ");
					}

					node.insert(json_spirit::Object::value_type("plugin", r.plugin_id()));
					node.insert(json_spirit::Object::value_type("path", q.node().path()));
					node.insert(json_spirit::Object::value_type("title", q.info().title()));
					node.insert(json_spirit::Object::value_type("icon", q.info().icon()));
					node.insert(json_spirit::Object::value_type("description", q.info().description()));

					//node.insert(json_spirit::Object::value_type("fields", value));
					std::string tplData = json_spirit::write(node);
					settings_manager::get_core()->register_tpl(r.plugin_id(), q.node().path(), q.info().title(), tplData);
				} else if (q.node().has_key()) {
					settings_manager::get_core()->register_key(r.plugin_id(), q.node().path(), q.node().key(), settings::settings_core::key_string, q.info().title(), q.info().description(), q.info().default_value().string_data(), q.info().advanced(), q.info().sample());
				} else {
					settings_manager::get_core()->register_path(r.plugin_id(), q.node().path(), q.info().title(), q.info().description(), q.info().advanced(), q.info().sample());
				}
				rp->mutable_result()->set_code(Plugin::Common_Result_StatusCodeType_STATUS_OK);
			} else if (r.has_update()) {
				const Plugin::SettingsRequestMessage::Request::Update &p = r.update();
				rp->mutable_update();
				if (p.value().has_string_data()) {
					settings_manager::get_settings()->set_string(p.node().path(), p.node().key(), p.value().string_data());
				} else if (p.value().has_bool_data()) {
					settings_manager::get_settings()->set_bool(p.node().path(), p.node().key(), p.value().bool_data());
				} else if (p.value().has_int_data()) {
					settings_manager::get_settings()->set_int(p.node().path(), p.node().key(), p.value().int_data());
				} else {
					LOG_ERROR_CORE("Invalid data type");
				}
				rp->mutable_result()->set_code(Plugin::Common_Result_StatusCodeType_STATUS_OK);
			} else if (r.has_control()) {
				const Plugin::SettingsRequestMessage::Request::Control &p = r.control();
				rp->mutable_control();
				if (p.command() == Plugin::Settings_Command_LOAD) {
					if (p.has_context() && p.context().size() > 0)
						settings_manager::get_core()->migrate_from(p.context());
					else
						settings_manager::get_settings()->load();
					settings_manager::get_settings()->reload();
					rp->mutable_result()->set_code(Plugin::Common_Result_StatusCodeType_STATUS_OK);
				} else if (p.command() == Plugin::Settings_Command_SAVE) {
					if (p.has_context() && p.context().size() > 0)
						settings_manager::get_core()->migrate_to(p.context());
					else
						settings_manager::get_settings()->save();
					rp->mutable_result()->set_code(Plugin::Common_Result_StatusCodeType_STATUS_OK);
				} else {
					rp->mutable_result()->set_code(Plugin::Common_Result_StatusCodeType_STATUS_OK);
					rp->mutable_result()->set_message("Unknown command");
				}
			} else if (r.has_status()) {
				rp->mutable_status()->set_has_changed(settings_manager::get_core()->is_dirty());
				rp->mutable_status()->set_context(settings_manager::get_settings()->get_context());
				rp->mutable_status()->set_type(settings_manager::get_settings()->get_type());
				rp->mutable_result()->set_code(Plugin::Common_Result_StatusCodeType_STATUS_OK);
			} else {
				rp->mutable_result()->set_code(Plugin::Common_Result_StatusCodeType_STATUS_OK);
				rp->mutable_result()->set_message("Settings error: Invalid action");
				LOG_ERROR_CORE_STD("Settings error: Invalid action");
			}
		} catch (settings::settings_exception &e) {
			rp->mutable_result()->set_code(Plugin::Common_Result_StatusCodeType_STATUS_OK);
			rp->mutable_result()->set_message("Settings error: " + e.reason());
			LOG_ERROR_CORE_STD("Settings error: " + e.reason());
		} catch (const std::exception &e) {
			rp->mutable_result()->set_code(Plugin::Common_Result_StatusCodeType_STATUS_OK);
			rp->mutable_result()->set_message("Settings error: " + utf8::utf8_from_native(e.what()));
			LOG_ERROR_CORE_STD("Settings error: " + utf8::utf8_from_native(e.what()));
		} catch (...) {
			rp->mutable_result()->set_code(Plugin::Common_Result_StatusCodeType_STATUS_OK);
			rp->mutable_result()->set_message("Settings error");
			LOG_ERROR_CORE_STD("Settings error");
		}
	}
	try {
		*response_buffer_len = response.ByteSize();
		*response_buffer = new char[*response_buffer_len + 10];
		response.SerializeToArray(*response_buffer, *response_buffer_len);
		return NSCAPI::api_return_codes::isSuccess;
	} catch (const std::exception &e) {
		LOG_ERROR_CORE_STD("Settings error: " + utf8::utf8_from_native(e.what()));
	} catch (...) {
		LOG_ERROR_CORE_STD("Settings error");
	}
	return NSCAPI::api_return_codes::hasFailed;
}

boost::optional<boost::filesystem::path> locateFileICase(const boost::filesystem::path path, const std::string filename) {
	boost::filesystem::path fullpath = path / filename;
#ifdef WIN32
	std::wstring tmp = utf8::cvt<std::wstring>(fullpath.string());
	SHFILEINFOW sfi = { 0 };
	boost::replace_all(tmp, "/", "\\");
	HRESULT hr = SHGetFileInfo(tmp.c_str(), 0, &sfi, sizeof(sfi), SHGFI_DISPLAYNAME);
	if (SUCCEEDED(hr)) {
		tmp = sfi.szDisplayName;
		boost::filesystem::path rpath = path / utf8::cvt<std::string>(tmp);
		return rpath;
	}
#else
	if (boost::filesystem::is_regular_file(fullpath))
		return fullpath;
	boost::filesystem::directory_iterator it(path), eod;
	std::string tmp = boost::algorithm::to_lower_copy(filename);
	BOOST_FOREACH(boost::filesystem::path const &p, std::make_pair(it, eod)) {
		if (boost::filesystem::is_regular_file(p) && boost::algorithm::to_lower_copy(file_helpers::meta::get_filename(p)) == tmp) {
			return p;
		}
	}
#endif
	return boost::optional<boost::filesystem::path>();
}

NSCAPI::errorReturn NSClientT::registry_query(const char *request_buffer, const unsigned int request_buffer_len, char **response_buffer, unsigned int *response_buffer_len) {
	try {
		std::string response_string;
		Plugin::RegistryRequestMessage request;
		Plugin::RegistryResponseMessage response;
		nscapi::protobuf::functions::create_simple_header(response.mutable_header());
		modules_type module_cache;
		request.ParseFromArray(request_buffer, request_buffer_len);
		for (int i = 0; i < request.payload_size(); i++) {
			const Plugin::RegistryRequestMessage::Request &r = request.payload(i);
			if (r.has_inventory()) {
				const Plugin::RegistryRequestMessage::Request::Inventory &q = r.inventory();
				Plugin::RegistryResponseMessage::Response* rp = response.add_payload();
				timer tme;
				tme.start("...");
				for (int i = 0; i < q.type_size(); i++) {
					Plugin::Registry_ItemType type = q.type(i);
					if (type == Plugin::Registry_ItemType_QUERY || type == Plugin::Registry_ItemType_ALL) {
						if (q.has_name()) {
							nsclient::commands::command_info info = commands_.describe(q.name());
							if (!info.name.empty()) {
								Plugin::RegistryResponseMessage::Response::Inventory *rpp = rp->add_inventory();
								rpp->set_name(q.name());
								rpp->set_type(Plugin::Registry_ItemType_COMMAND);
								rpp->mutable_info()->add_plugin(add_plugin_data(module_cache, info.plugin_id, this));
								rpp->mutable_info()->set_title(info.name);
								rpp->mutable_info()->set_description(info.description);
								if (q.has_fetch_all() && q.fetch_all()) {
									std::string resp;
									Plugin::QueryRequestMessage req;
									Plugin::QueryResponseMessage res;
									nscapi::protobuf::functions::create_simple_header(req.mutable_header());
									Plugin::QueryRequestMessage::Request * p = req.add_payload();
									p->set_command(q.name());
									p->add_arguments("help-pb");
									std::string msg = req.SerializeAsString();
									injectRAW(msg, resp);
									res.ParseFromString(resp);
									for (int i = 0; i < res.payload_size(); i++) {
										const Plugin::QueryResponseMessage::Response p = res.payload(i);
										rpp->mutable_parameters()->ParseFromString(p.data());
									}
								}
							}
						} else {
							BOOST_FOREACH(const std::string &command, commands_.list_commands()) {
								nsclient::commands::command_info info = commands_.describe(command);
								Plugin::RegistryResponseMessage::Response::Inventory *rpp = rp->add_inventory();
								rpp->set_name(command);
								rpp->set_type(Plugin::Registry_ItemType_COMMAND);
								rpp->mutable_info()->add_plugin(add_plugin_data(module_cache, info.plugin_id, this));
								rpp->mutable_info()->set_title(info.name);
								rpp->mutable_info()->set_description(info.description);
								if (q.has_fetch_all() && q.fetch_all()) {
									std::string resp;
									Plugin::QueryRequestMessage req;
									Plugin::QueryResponseMessage res;
									nscapi::protobuf::functions::create_simple_header(req.mutable_header());
									Plugin::QueryRequestMessage::Request * p = req.add_payload();
									p->set_command(command);
									p->add_arguments("help-pb");
									std::string msg = req.SerializeAsString();
									injectRAW(msg, resp);
									res.ParseFromString(resp);
									for (int i = 0; i < res.payload_size(); i++) {
										const Plugin::QueryResponseMessage::Response p = res.payload(i);
										rpp->mutable_parameters()->ParseFromString(p.data());
									}
								}
							}
						}
					}
					if (type == Plugin::Registry_ItemType_QUERY_ALIAS || type == Plugin::Registry_ItemType_ALL) {
						BOOST_FOREACH(const std::string &command, commands_.list_aliases()) {
							nsclient::commands::command_info info = commands_.describe(command);
							Plugin::RegistryResponseMessage::Response::Inventory *rpp = rp->add_inventory();
							rpp->set_name(command);
							rpp->set_type(Plugin::Registry_ItemType_QUERY_ALIAS);
							rpp->mutable_info()->add_plugin(add_plugin_data(module_cache, info.plugin_id, this));
							rpp->mutable_info()->set_title(info.name);
							rpp->mutable_info()->set_description(info.description);
						}
					}
					if (type == Plugin::Registry_ItemType_MODULE || type == Plugin::Registry_ItemType_ALL) {
						boost::unordered_set<std::string> cache;
						bool has_files = false;
						tme.start("enumerating loaded");
						{
							tme.start("getting lock");
							boost::shared_lock<boost::shared_mutex> readLock(m_mutexRW, boost::get_system_time() + boost::posix_time::milliseconds(5000));
							tme.end();
							if (readLock.owns_lock()) {
								BOOST_FOREACH(plugin_type plugin, plugins_) {
									Plugin::RegistryResponseMessage::Response::Inventory *rpp = rp->add_inventory();
									cache.emplace(plugin->getModule());
									rpp->set_name(plugin->getModule());
									rpp->set_type(Plugin::Registry_ItemType_MODULE);
									rpp->set_id(plugin->get_alias_or_name());
									rpp->mutable_info()->add_plugin(plugin->getModule());
									rpp->mutable_info()->set_title(plugin->getName());
									rpp->mutable_info()->set_description(plugin->getDescription());
									Plugin::Common::KeyValue *kvp = rpp->mutable_info()->add_metadata();
									kvp->set_key("plugin_id");
									kvp->set_value(strEx::s::xtos(plugin->get_id()));
									kvp = rpp->mutable_info()->add_metadata();
									kvp->set_key("loaded");
									kvp->set_value("true");
								}
								if (!plugin_cache_.empty()) {
									has_files = true;
									BOOST_FOREACH(const plugin_cache_item &itm, plugin_cache_) {
										if (cache.find(itm.name) == cache.end()) {
											Plugin::RegistryResponseMessage::Response::Inventory *rpp = rp->add_inventory();
											rpp->set_name(itm.name);
											rpp->set_type(Plugin::Registry_ItemType_MODULE);
											rpp->mutable_info()->set_title(itm.title);
											rpp->mutable_info()->set_description(itm.desc);
											Plugin::Common::KeyValue *kvp = rpp->mutable_info()->add_metadata();
											kvp->set_key("loaded");
											kvp->set_value("false");
										}
									}
								}
							} else {
								LOG_ERROR_CORE("FATAL ERROR: Could not get read-mutex.");
							}
						}
						tme.end();
						if (!has_files) {
							tme.start("enumerating files");
							plugin_cache_list  tmp_plugin_cache;
							boost::filesystem::path pluginPath = expand_path("${module-path}");
							boost::filesystem::directory_iterator end_itr; // default construction yields past-the-end
							for (boost::filesystem::directory_iterator itr(pluginPath); itr != end_itr; ++itr) {
								if (!is_directory(itr->status())) {
									boost::filesystem::path file = itr->path().filename();
									if (NSCPlugin::is_module(pluginPath / file)) {
										const std::string module = NSCPlugin::file_to_module(file);
										plugin_cache_item itm;
										try {
											plugin_type plugin = find_plugin(module);
											bool has_plugin = static_cast<bool>(plugin);
											if (!has_plugin) {
												boost::filesystem::path p = (pluginPath / file).normalize();
												LOG_DEBUG_CORE("Loading " + p.string());
												plugin = plugin_type(new NSCPlugin(-1, p, ""));
												plugin->load_dll();
											} else {
												LOG_DEBUG_CORE("Found cached " + module);
											}
											itm.name = plugin->getModule();
											itm.title = plugin->getName();
											itm.desc = plugin->getDescription();
											tmp_plugin_cache.push_back(itm);
											if (!has_plugin) {
												plugin->unload_dll();
											}
										} catch (const std::exception &e) {
											LOG_DEBUG_CORE("Failed to load " + file.string() + ": " + utf8::utf8_from_native(e.what()));
											continue;
										}
										if (!itm.name.empty() && cache.find(itm.name) == cache.end()) {
											Plugin::RegistryResponseMessage::Response::Inventory *rpp = rp->add_inventory();
											rpp->set_name(itm.name);
											rpp->set_type(Plugin::Registry_ItemType_MODULE);
											rpp->mutable_info()->set_title(itm.title);
											rpp->mutable_info()->set_description(itm.desc);
											Plugin::Common::KeyValue *kvp = rpp->mutable_info()->add_metadata();
											kvp->set_key("loaded");
											kvp->set_value("false");
										}
									}
								}
							}
							tme.end();

							{
								tme.start("getting lock");
								boost::unique_lock<boost::shared_mutex> writeLock(m_mutexRW, boost::get_system_time() + boost::posix_time::milliseconds(5000));
								tme.end();
								if (writeLock.owns_lock()) {
									plugin_cache_.insert(plugin_cache_.end(), tmp_plugin_cache.begin(), tmp_plugin_cache.end());
								}
							}
						}
					}
				}
				tme.end();
				BOOST_FOREACH(const std::string &s, tme.get()) {
					LOG_DEBUG_CORE(s);
				}
				rp->mutable_result()->set_code(Plugin::Common_Result_StatusCodeType_STATUS_OK);
			} else if (r.has_registration()) {
				const Plugin::RegistryRequestMessage::Request::Registration registration = r.registration();
				Plugin::RegistryResponseMessage::Response* rp = response.add_payload();
				if (registration.type() == Plugin::Registry_ItemType_QUERY) {
					if (registration.unregister()) {
						commands_.unregister_command(registration.plugin_id(), registration.name());
						BOOST_FOREACH(const std::string &alias, registration.alias())
							commands_.unregister_command(registration.plugin_id(), alias);
					} else {
						commands_.register_command(registration.plugin_id(), registration.name(), registration.info().description());
						std::string description = "Alternative name for: " + registration.name();
						BOOST_FOREACH(const std::string &alias, registration.alias())
							commands_.register_alias(registration.plugin_id(), alias, description);
					}
				} else if (registration.type() == Plugin::Registry_ItemType_QUERY_ALIAS) {
					commands_.register_alias(registration.plugin_id(), registration.name(), registration.info().description());
					for (int i = 0; i < registration.alias_size(); i++) {
						commands_.register_alias(registration.plugin_id(), registration.alias(i), registration.info().description());
					}
				} else if (registration.type() == Plugin::Registry_ItemType_HANDLER) {
					channels_.register_listener(registration.plugin_id(), registration.name());
				} else if (registration.type() == Plugin::Registry_ItemType_ROUTER) {
					routers_.register_listener(registration.plugin_id(), registration.name());
				} else if (registration.type() == Plugin::Registry_ItemType_MODULE) {
					Plugin::RegistryResponseMessage::Response::Registration *rpp = rp->mutable_registration();
					boost::shared_lock<boost::shared_mutex> readLock(m_mutexRW, boost::get_system_time() + boost::posix_time::milliseconds(5000));
					if (readLock.owns_lock()) {
						plugin_type match;
						BOOST_FOREACH(plugin_type plugin, plugins_) {
							if (plugin->get_id() == registration.plugin_id()) {
								match = plugin;
							}
						}
						if (match) {
							int new_id = next_plugin_id_++;
							commands_.add_plugin(new_id, match);
							rpp->set_item_id(new_id);
						} else {
							LOG_ERROR_CORE("Plugin not found.");
						}
					} else {
						LOG_ERROR_CORE("FATAL ERROR: Could not get read-mutex.");
					}
				} else {
					LOG_ERROR_CORE("Registration query: Unsupported type");
				}
				rp->mutable_result()->set_code(Plugin::Common_Result_StatusCodeType_STATUS_OK);
			} else if (r.has_control()) {
				const Plugin::RegistryRequestMessage::Request::Control control = r.control();
				Plugin::RegistryResponseMessage::Response* rp = response.add_payload();
				if (control.type() == Plugin::Registry_ItemType_MODULE) {
					if (control.command() == Plugin::Registry_Command_LOAD) {
						boost::filesystem::path pluginPath = expand_path("${module-path}");
						boost::optional<boost::filesystem::path> module = locateFileICase(pluginPath, NSCPlugin::get_plugin_file(control.name()));
						if (!module)
							module = locateFileICase(boost::filesystem::path("./modules"), NSCPlugin::get_plugin_file(control.name()));
						if (!module) {
							LOG_ERROR_CORE("Failed to find: " + control.name());
						} else {
							LOG_DEBUG_CORE_STD("Module name: " + module->string());

							try {
								plugin_type instance = addPlugin(*module, control.alias());
								instance->load_plugin(NSCAPI::normalStart);
							} catch (const NSPluginException& e) {
								if (e.file().find("FileLogger") != std::string::npos) {
									LOG_DEBUG_CORE_STD("Failed to load " + module->string() + ": " + e.reason());
								} else {
									LOG_ERROR_CORE_STD("Failed to load " + module->string() + ": " + e.reason());
								}
							} catch (const std::exception &e) {
								LOG_ERROR_CORE_STD("exception loading plugin " + module->string() + ": " + utf8::utf8_from_native(e.what()));
								return false;
							} catch (...) {
								LOG_ERROR_CORE_STD("Unknown exception loading plugin: " + module->string());
								return false;
							}
						}
					} else if (control.command() == Plugin::Registry_Command_UNLOAD) {
						boost::unique_lock<boost::shared_mutex> writeLock(m_mutexRW, boost::get_system_time() + boost::posix_time::seconds(5));
						if (!writeLock.owns_lock()) {
							LOG_ERROR_CORE("FATAL ERROR: Could not get write-mutex.");
						} else {
							for (pluginList::iterator it = plugins_.begin(); it != plugins_.end();) {
								if ((*it)->getModule() == control.name()) {
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
					} else {
						LOG_ERROR_CORE("Registration query: Invalid command");
					}
				} else {
					LOG_ERROR_CORE("Registration query: Unsupported type");
				}
				rp->mutable_result()->set_code(Plugin::Common_Result_StatusCodeType_STATUS_OK);
			} else {
				LOG_ERROR_CORE("Registration query: Unsupported action");
			}
		}
		*response_buffer_len = response.ByteSize();
		*response_buffer = new char[*response_buffer_len + 10];
		response.SerializeToArray(*response_buffer, *response_buffer_len);
	} catch (settings::settings_exception e) {
		LOG_ERROR_CORE_STD("Failed query: " + e.reason());
		return NSCAPI::api_return_codes::hasFailed;
	} catch (const std::exception &e) {
		LOG_ERROR_CORE_STD("Failed query: " + utf8::utf8_from_native(e.what()));
		return NSCAPI::api_return_codes::hasFailed;
	} catch (...) {
		LOG_ERROR_CORE("Failed query");
		return NSCAPI::api_return_codes::hasFailed;
	}
	return NSCAPI::api_return_codes::isSuccess;
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