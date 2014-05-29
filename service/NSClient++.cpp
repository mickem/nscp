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
#include "StdAfx.h"
#include "NSClient++.h"
#include <settings/settings_core.hpp>
#include <charEx.h>
#include <config.h>
#ifdef WIN32
#include <Userenv.h>
#include <Lmcons.h>
#endif


#include <boost/unordered_map.hpp>
#include <timer.hpp>

#include "core_api.h"
#include "../helpers/settings_manager/settings_manager_impl.h"
#include <settings/macros.h>
// #include "simple_client.hpp"
// #include "settings_client.hpp"
// #include "service_manager.hpp"
#include <nscapi/functions.hpp>

#include <settings/client/settings_client.hpp>
#include "cli_parser.hpp"
#include "../version.hpp"

#include <config.h>

#ifdef WIN32
#include <ServiceCmd.h>
#include <com_helpers.hpp>
com_helper::initialize_com com_helper_;
#endif


#ifdef USE_BREAKPAD
#include <breakpad/exception_handler_win32.hpp>
// Used for breakpad crash handling
static ExceptionManager *g_exception_manager = NULL;
#endif

NSClient mainClient;	// Global core instance.

#define LOG_CRITICAL_CORE(msg) { nsclient::logging::logger::get_logger()->fatal("core", __FILE__, __LINE__, msg);}
#define LOG_CRITICAL_CORE_STD(msg) LOG_CRITICAL_CORE(std::string(msg))
#define LOG_ERROR_CORE(msg) { nsclient::logging::logger::get_logger()->error("core", __FILE__, __LINE__, msg);}
#define LOG_ERROR_CORE_STD(msg) LOG_ERROR_CORE(std::string(msg))
#define LOG_INFO_CORE(msg) { nsclient::logging::logger::get_logger()->info("core", __FILE__, __LINE__, msg);}
#define LOG_INFO_CORE_STD(msg) LOG_INFO_CORE(std::string(msg))
#define LOG_DEBUG_CORE(msg) { nsclient::logging::logger::get_logger()->debug("core", __FILE__, __LINE__, msg);}
#define LOG_DEBUG_CORE_STD(msg) LOG_DEBUG_CORE(std::string(msg))

/**
 * START OF Tray starter MERGE HELPER
 */
class tray_starter {
	struct start_block {
		std::wstring cmd;
		std::wstring cmd_line;
		DWORD sessionId;
	};

public:
	static LPVOID init(DWORD dwSessionId, std::wstring exe, std::wstring cmdline) {
		start_block *sb = new start_block;
		sb->cmd = exe;
		sb->cmd_line = cmdline;
		sb->sessionId = dwSessionId;
		return sb;
	}
	DWORD threadProc(LPVOID lpParameter) {
#ifdef WIN32
		start_block* param = static_cast<start_block*>(lpParameter);
		DWORD dwSessionId = param->sessionId;
		std::wstring cmd = param->cmd;
		std::wstring cmdline = param->cmd_line;
		delete param;
		for (int i=0;i<10;i++) {
			Sleep(1000);
			if (startTrayHelper(dwSessionId, cmd, cmdline, false))
				break;
		}
#endif
		return 0;
	}

	static bool start(std::wstring command, unsigned long  dwSessionId) {
// 		std::wstring cmdln = _T("\"") + command + _T("\" -channel __") + strEx::itos(dwSessionId) + _T("__");
// 		return tray_starter::startTrayHelper(dwSessionId, command, cmdln);
		return false;
	}

	static bool startTrayHelper(unsigned long dwSessionId, std::wstring exe, std::wstring cmdline, bool startThread = true) {
// 		HANDLE hToken = NULL;
// 		if (!remote_processes::GetSessionUserToken(dwSessionId, &hToken)) {
// 			LOG_ERROR_STD(_T("Failed to query user token: ") + error::lookup::last_error());
// 			return false;
// 		} else {
// 			STARTUPINFO          StartUPInfo;
// 			PROCESS_INFORMATION  ProcessInfo;
// 
// 			ZeroMemory(&StartUPInfo,sizeof(STARTUPINFO));
// 			ZeroMemory(&ProcessInfo,sizeof(PROCESS_INFORMATION));
// 			StartUPInfo.wShowWindow = SW_HIDE;
// 			StartUPInfo.lpDesktop = L"Winsta0\\Default";
// 			StartUPInfo.cb = sizeof(STARTUPINFO);
// 
// 			wchar_t *buffer = new wchar_t[cmdline.size()+10];
// 			wcscpy(buffer, cmdline.c_str());
// 			LOG_MESSAGE_STD(_T("Running: ") + exe);
// 			LOG_MESSAGE_STD(_T("Running: ") + cmdline);
// 
// 			LPVOID pEnv =NULL;
// 			DWORD dwCreationFlags = CREATE_NO_WINDOW; //0; //DETACHED_PROCESS
// 
// 			if(CreateEnvironmentBlock(&pEnv,hToken,TRUE)) {
// 				dwCreationFlags|=CREATE_UNICODE_ENVIRONMENT;
// 			} else {
// 				LOG_ERROR_STD(_T("Failed to create enviornment: ") + error::lookup::last_error());
// 				pEnv=NULL;
// 			}
// 			/*
// 			LOG_ERROR_STD(_T("Impersonating user: "));
// 			if (!ImpersonateLoggedOnUser(hToken)) {
// 				LOG_ERROR_STD(_T("Failed to impersonate the user: ") + error::lookup::last_error());
// 			}
// 
// 			wchar_t pszUname[UNLEN + 1];
// 			ZeroMemory(pszUname,sizeof(pszUname));
// 			DWORD dwSize = UNLEN;
// 			if (!GetUserName(pszUname,&dwSize)) {
// 				DWORD dwErr = GetLastError();
// 				if (!RevertToSelf())
// 					LOG_ERROR_STD(_T("Failed to revert to self: ") + error::lookup::last_error());
// 				LOG_ERROR_STD(_T("Failed to get username: ") + error::format::from_system(dwErr));
// 				return false;
// 			}
// 			
// 
// 			PROFILEINFO info;
// 			info.dwSize = sizeof(PROFILEINFO);
// 			info.lpUserName = pszUname;
// 			if (!LoadUserProfile(hToken, &info)) {
// 				DWORD dwErr = GetLastError();
// 				if (!RevertToSelf())
// 					LOG_ERROR_STD(_T("Failed to revert to self: ") + error::lookup::last_error());
// 				LOG_ERROR_STD(_T("Failed to get username: ") + error::format::from_system(dwErr));
// 				return false;
// 			}
// 			*/
// 			if (!CreateProcessAsUser(hToken, exe.c_str(), buffer, NULL, NULL, FALSE, dwCreationFlags, pEnv, NULL, &StartUPInfo, &ProcessInfo)) {
// 				DWORD dwErr = GetLastError();
// 				delete [] buffer;
// 				/*
// 				if (!RevertToSelf()) {
// 					LOG_ERROR_STD(_T("Failed to revert to self: ") + error::lookup::last_error());
// 				}
// 				*/
// 				if (startThread && dwErr == ERROR_PIPE_NOT_CONNECTED) {
// 					LOG_MESSAGE(_T("Failed to start trayhelper: starting a background thread to do it instead..."));
// 					Thread<tray_starter> *pThread = new Thread<tray_starter>(_T("tray-starter-thread"));
// 					pThread->createThread(tray_starter::init(dwSessionId, exe, cmdline));
// 					return false;
// 				} else if (dwErr == ERROR_PIPE_NOT_CONNECTED) {
// 					LOG_ERROR_STD(_T("Thread failed to start trayhelper (will try again): ") + error::format::from_system(dwErr));
// 					return false;
// 				} else {
// 					LOG_ERROR_STD(_T("Failed to start trayhelper: ") + error::format::from_system(dwErr));
// 					return true;
// 				}
// 			} else {
// 				delete [] buffer;
// 				/*
// 				if (!RevertToSelf()) {
// 					LOG_ERROR_STD(_T("Failed to revert to self: ") + error::lookup::last_error());
// 				}
// 				*/
// 				LOG_MESSAGE_STD(_T("Started tray in other user session: ") + strEx::itos(dwSessionId));
// 			}
// 
// 
// 			CloseHandle(hToken);
// 			return true;
// 		}
		return false;
	}
};

/**
 * End of class tray started (MERGE HELP)
 */

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
	for (int i=0;i<argc;i++) {
		std::string s = utf8::cvt<std::string>(argv[i]);
		wargv[i] = new char[s.length()+10];
		strncpy(wargv[i], s.c_str(), s.size()+1);
	}
	int ret = nscp_main(argc, wargv); 
	for (int i=0;i<argc;i++) {
		delete [] wargv[i];
	}
	delete [] wargv;
	return ret;
}
#else
int main(int argc, char* argv[]) { 
	return nscp_main(argc, argv); 
}
#endif

int nscp_main(int argc, char* argv[])
{
	srand( (unsigned)time( NULL ) );
	cli_parser parser(&mainClient);
	return parser.parse(argc, argv);
}

std::list<std::string> NSClientT::list_commands() {
	return commands_.list_all();
}

bool contains_plugin(NSClientT::plugin_alias_list_type &ret, std::string alias, std::string plugin) {
	std::pair<std::string,std::string> v;
	BOOST_FOREACH(v, ret.equal_range(alias)) {
		if (v.second == plugin)
			return true;
	}
	return false;
}

NSClientT::NSClientT() 
	: enable_shared_session_(false)
	, next_plugin_id_(0)
	, service_name_(DEFAULT_SERVICE_NAME) 
{
		nsclient::logging::logger::startup();
}

NSClientT::~NSClientT() {
	try {
		nsclient::logging::logger::destroy();
	} catch(...) {
		std::cerr << "UNknown exception raised: When destroying logger" << std::endl;
	}
}


NSClientT::plugin_alias_list_type NSClientT::find_all_plugins(bool active) {
	plugin_alias_list_type ret;

	settings::string_list list = settings_manager::get_settings()->get_keys(MAIN_MODULES_SECTION);
	BOOST_FOREACH(std::string plugin, list) {
		std::string alias;
		try {
			alias = settings_manager::get_settings()->get_string(MAIN_MODULES_SECTION, plugin);
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
		if (plugin.length() > 4 && plugin.substr(plugin.length()-4) == ".dll")
			plugin = plugin.substr(0, plugin.length()-4);
		ret.insert(plugin_alias_list_type::value_type(alias, plugin));
	}
	if (!active) {
		boost::filesystem::path pluginPath = expand_path("${module-path}");
		boost::filesystem::directory_iterator end_itr; // default construction yields past-the-end
		for ( boost::filesystem::directory_iterator itr( pluginPath ); itr != end_itr; ++itr ) {
			if ( !is_directory(itr->status()) ) {
				boost::filesystem::path file= itr->path().filename();
				if (NSCPlugin::is_module(pluginPath  / file)) {
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
			if (v.second == "NSCPDOTNET.dll" || v.second == "NSCPDOTNET")
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
		return mainClient.expand_path(file);
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
			("/modules",			"MODULES", "A list of modules.")
			;

		settings.add_path_to_settings()
			("log",				"LOG SETTINGS", "Section for configuring the log handling.")
			("shared session",	"SHRED SESSION", "Section for configuring the shared session.")
			("crash",			"CRASH HANDLER", "Section for configuring the crash handler.")
			;

		settings.add_key_to_settings("log")
			("level", sh::string_key(&log_level, "info"),
			"LOG LEVEL", "Log level to use. Available levels are error,warning,info,debug,trace")
			;

		settings.add_key_to_settings("shared session")
			("enabled", sh::bool_key(&enable_shared_session_ , false),
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
#else
//	LOG_ERROR_CORE("Warning Not compiled with google breakpad support!");
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
			} catch(const NSPluginException& e) {
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
		if (plugin.length() > 4 && plugin.substr(plugin.length()-4) == ".dll")
			plugin = plugin.substr(0, plugin.length()-4);

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
	} catch(const std::exception &e) {
		LOG_ERROR_CORE_STD("Module (" + plugin + ") was not found: " + utf8::utf8_from_native(e.what()));
	} catch(...) {
		LOG_ERROR_CORE_STD("Module (" + plugin + ") was not found...");
	}
	return false;
}
bool NSClientT::boot_start_plugins(bool boot) {
	try {
		loadPlugins(boot?NSCAPI::normalStart:NSCAPI::dontStart);
	} catch (...) {
		LOG_ERROR_CORE("Unknown exception loading plugins");
		return false;
	}
	LOG_DEBUG_CORE(utf8::cvt<std::string>(APPLICATION_NAME " - " CURRENT_SERVICE_VERSION " Started!"));
	return true;
}

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

bool NSClientT::stop_unload_plugins_pre() {
	LOG_DEBUG_CORE("Attempting to stop all plugins");
	try {
		LOG_DEBUG_CORE("Stopping all plugins");
		mainClient.unloadPlugins();
	} catch(NSPluginException e) {
		LOG_ERROR_CORE_STD("Exception raised when unloading non msg plguins: " + e.reason() + " in module: " + e.file());
	} catch(...) {
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
	} catch(...) {
		LOG_ERROR_CORE("UNknown exception raised: When closing shared session");
	}
	return true;
}

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
			} catch(const NSPluginException &e) {
				LOG_ERROR_CORE_STD("Exception raised when unloading plugin: " + e.reason() + " in module: " + e.file());
			} catch(...) {
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

bool NSClientT::do_reload(const bool delay, const std::string module) {
	if (delay) {
		boost::this_thread::sleep(boost::posix_time::seconds(3));
	}
	if (module == "settings") {
		try {
			settings_manager::get_settings()->clear_cache();
			return true;
		} catch(const std::exception &e) {
			LOG_ERROR_CORE_STD("Exception raised when reloading: " + utf8::utf8_from_native(e.what()));
		} catch(...) {
			LOG_ERROR_CORE("Exception raised when reloading: UNKNOWN");
		}
	} else if (module == "service") {
		try {
			LOG_DEBUG_CORE_STD(std::string("Reloading all modules ") + (delay?"(delayed)":"") + ".");
			stop_unload_plugins_pre();
			boot_load_all_plugins();
			boot_start_plugins(true);
			return true;
		} catch(const std::exception &e) {
			LOG_ERROR_CORE_STD("Exception raised when reloading: " + utf8::utf8_from_native(e.what()));
		} catch(...) {
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
				LOG_DEBUG_CORE_STD(std::string("Found module: ") + p->get_alias_or_name() + ", reloading " + (delay?"(delayed)":"") + ".");
				p->unload_plugin();
				p->load_plugin(NSCAPI::normalStart);
				return true;
			}
		}
	}
	return false;
}


NSCAPI::errorReturn NSClientT::reload(const std::string module) {
	if (module.size() > 8 && module.substr(0,8) == "delayed,") {
		boost::thread delayed_thread(boost::bind(&NSClientT::do_reload, this, true, module.substr(8)));
		delayed_thread.detach();
		return NSCAPI::isSuccess;
	} else if (module.size() > 6 && module.substr(0,6) == "delay,") {
		boost::thread delayed_thread(boost::bind(&NSClientT::do_reload, this, true, module.substr(6)));
		delayed_thread.detach();
		return NSCAPI::isSuccess;
	} else {
		return do_reload(false, module)?NSCAPI::isSuccess:NSCAPI::hasFailed;
	}
}

void NSClientT::loadPlugins(NSCAPI::moduleLoadMode mode) {
	{
		boost::shared_lock<boost::shared_mutex> readLock(m_mutexRW, boost::get_system_time() + boost::posix_time::milliseconds(5000));
		if (!readLock.owns_lock()) {
			LOG_ERROR_CORE("FATAL ERROR: Could not get read-mutex.");
			return;
		}
		for (pluginList::iterator it=plugins_.begin(); it != plugins_.end();) {
			LOG_DEBUG_CORE_STD("Loading plugin: " + (*it)->get_description());
			try {
				if (!(*it)->load_plugin(mode)) {
					LOG_ERROR_CORE_STD("Plugin refused to load: " + (*it)->get_description());
					it = plugins_.erase(it);
				} else
					++it;
			} catch (NSPluginException e) {
				it = plugins_.erase(it);
				LOG_ERROR_CORE_STD("Could not load plugin: " + e.reason() + ": " + e.file());
			} catch (...) {
				it = plugins_.erase(it);
				LOG_ERROR_CORE_STD("Could not load plugin: " + (*it)->get_description());
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
		if (plugin->hasMessageHandler())
			nsclient::logging::logger::subscribe_raw(plugin);
		if (plugin->has_routing_handler())
			routers_.add_plugin(plugin);
		settings_manager::get_core()->register_key(0xffff, "/modules", plugin->getModule(), settings::settings_core::key_string, plugin->getName(), plugin->getDescription(), "0", false, false);
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
	/*if (shared_client_.get() != NULL && shared_client_->hasMaster()) {
		try {
			return shared_client_->inject(command, arguments, splitter, escape, msg, perf);
		} catch (nsclient_session::session_exception &e) {
			LOG_ERROR_STD(_T("Failed to inject remote command: ") + e.what());
			return NSCAPI::returnCRIT;
		} catch (...) {
			LOG_ERROR_STD(_T("Failed to inject remote command: Unknown exception"));
			return NSCAPI::returnCRIT;
		}
	} else */{

		std::list<std::string> args;
		strEx::s::parse_command(arguments, args);
		std::string request, response;
		nscapi::protobuf::functions::create_simple_query_request(command, args, request);
		NSCAPI::nagiosReturn ret = injectRAW(request, response);
		if (ret == NSCAPI::hasFailed && response.empty()) {
			msg = "Failed to execute: " + command;
			return NSCAPI::returnUNKNOWN;
		} else if (response.empty()) {
			msg = "No data returned from: " + command;
			return NSCAPI::returnUNKNOWN;
		}
		return nscapi::protobuf::functions::parse_simple_query_response(response, msg, perf);
	}
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

		std::string commands;
		for (int i=0;i<request_message.payload_size(); i++) {
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
				strEx::append_list(commands, payload->command());
			}
		}

		if (command_chunks.size() == 0) {
			LOG_ERROR_CORE("Unknown command(s): " + commands + " available commands: " + commands_.to_string());
			nscapi::protobuf::functions::create_simple_header(response_message.mutable_header());
			Plugin::QueryResponseMessage::Response *payload = response_message.add_payload();
			payload->set_command(commands);
			nscapi::protobuf::functions::set_response_bad(*payload, "Unknown command(s): " + commands);
			response = response_message.SerializeAsString();
			return NSCAPI::hasFailed;
		}

		BOOST_FOREACH(command_chunk_type::value_type &v, command_chunks) {
			std::string local_response;
			int ret = v.second.plugin->handleCommand(v.second.request.SerializeAsString(), local_response);
			if (ret != NSCAPI::isSuccess) {
				LOG_ERROR_CORE("Failed to execute command");
			} else {
				Plugin::QueryResponseMessage local_response_message;
				local_response_message.ParseFromString(local_response);
				if (!response_message.has_header()) {
					response_message.mutable_header()->CopyFrom(local_response_message.header());
				}
				for (int i=0;i<local_response_message.payload_size();i++) {
					response_message.add_payload()->CopyFrom(local_response_message.payload(i));
				}
			}
		}
		response = response_message.SerializeAsString();
	} catch (const std::exception &e) {
		LOG_ERROR_CORE("Failed to process command: " + utf8::utf8_from_native(e.what()));
		return NSCAPI::hasFailed;
	} catch (...) {
		LOG_ERROR_CORE("Failed to process command: ");
		return NSCAPI::hasFailed;
	}
	return NSCAPI::isSuccess;
}


int NSClientT::load_and_run(std::string module, run_function fun, std::list<std::string> &errors) {
	int ret = -1;
	bool found = false;
	{
		boost::shared_lock<boost::shared_mutex> readLock(m_mutexRW, boost::get_system_time() + boost::posix_time::seconds(5));
		if (!readLock.owns_lock()) {
			LOG_ERROR_CORE("FATAL ERROR: Could not get read-mutex.");
			return -1;
		}
		BOOST_FOREACH(plugin_type plugin, plugins_) {
			if (plugin && (module.empty() || plugin->getModule() == module)) {
				LOG_DEBUG_CORE_STD("Found module: " + plugin->get_alias_or_name() + "...");
				try {
					ret = fun(plugin);
					found = true;
				} catch (const NSPluginException &e) {
					errors.push_back("Could not execute command: " + e.reason() + " in " + e.file());
					return -1;
				}
			}
		}
	}
	if (!found && !module.empty()) {
		try {
			boost::filesystem::path file = NSCPlugin::get_filename(getBasePath() / boost::filesystem::path("modules"), module);
			if (boost::filesystem::is_regular(file)) {
				plugin_type plugin = addPlugin(file, "");
				if (plugin) {
					LOG_DEBUG_CORE_STD("Loading plugin: " + plugin->get_alias_or_name() + "...");
					plugin->load_plugin(NSCAPI::dontStart);
					ret = fun(plugin);
				} else {
					errors.push_back("Failed to load: " + utf8::cvt<std::string>(module));
					return 1;
				}
			} else {
				errors.push_back("Failed to load: " + utf8::cvt<std::string>(module));
				return 1;
			}
		} catch (const NSPluginException &e) {
			errors.push_back("Module (" + e.file() + ") was not found: " + utf8::utf8_from_native(e.what()));
		} catch(const std::exception &e) {
			errors.push_back(std::string("Module (") + utf8::cvt<std::string>(module) + ") was not found: " + utf8::utf8_from_native(e.what()));
			return 1;
		} catch(...) {
			errors.push_back("Module (" + utf8::cvt<std::string>(module) + ") was not found...");
			return 1;
		}
	}
	return ret;
}

int exec_helper(NSClientT::plugin_type plugin, std::string command, std::vector<std::string> arguments, std::string request, std::list<std::string> *responses) {
	std::string response;
	if (!plugin || !plugin->has_command_line_exec())
		return -1;
	int ret = plugin->commandLineExec(request, response);
	if (ret != NSCAPI::returnIgnored && !response.empty())
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
		command = command.substr(pos+1);
	}
	nscapi::protobuf::functions::create_simple_exec_request(command, arguments, request);
	int ret = load_and_run(module, boost::bind(&exec_helper, _1, command, arguments, request, &responses), errors);

	BOOST_FOREACH(std::string &r, responses) {
		try {
			ret = nscapi::protobuf::functions::parse_simple_exec_response(r, resp);
		} catch (std::exception &e) {
			resp.push_back("Failed to extract return message: " + utf8::utf8_from_native(e.what()));
			LOG_ERROR_CORE_STD("Failed to extract return message: " + utf8::utf8_from_native(e.what()));
			return NSCAPI::returnUNKNOWN;
		}
	}
	BOOST_FOREACH(const std::string &e, errors) {
		LOG_ERROR_CORE_STD(e);
		resp.push_back(e);
	}
	return ret;
}
int query_helper(NSClientT::plugin_type plugin, std::string command, std::vector<std::string> arguments, std::string request, std::list<std::string> *responses) {
	return NSCAPI::returnIgnored;
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
		return NSCAPI::returnUNKNOWN;
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
		return NSCAPI::returnUNKNOWN;
	}
	BOOST_FOREACH(const std::string &e, errors) {
		LOG_ERROR_CORE_STD(e);
		resp.push_back(e);
	}
	return ret;
}

NSCAPI::nagiosReturn NSClientT::exec_command(const char* raw_target, std::string &request, std::string &response) {
	std::string target = raw_target;
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
				try {
					if (match_all || match_any || p->get_alias() == target || p->get_alias_or_name().find(target) != std::string::npos) {
						std::string respbuffer;
						NSCAPI::nagiosReturn r = p->commandLineExec(request, respbuffer);
						if (r != NSCAPI::returnIgnored && !respbuffer.empty()) {
							LOG_DEBUG_CORE_STD("Module handled execution request: " + p->getName());
							found = true;
							if (match_any) {
								response = respbuffer;
								return NSCAPI::isSuccess;
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
		for (int i=0;i<tmp.payload_size();i++) {
			Plugin::ExecuteResponseMessage::Response *r = response_message.add_payload();
			r->CopyFrom(tmp.payload(i));
		}
	}
	response_message.SerializeToString(&response);
	if (found)
		return NSCAPI::isSuccess;
	return NSCAPI::returnIgnored;
}


NSCAPI::errorReturn NSClientT::reroute(std::string &channel, std::string &buffer) {
	BOOST_FOREACH(nsclient::plugin_type p, routers_.get(channel)) {
		char *new_channel_buffer;
		char *new_buffer;
		unsigned int new_buffer_len;
		int status = p->route_message(channel.c_str(), buffer.c_str(), buffer.size(), &new_channel_buffer, &new_buffer, &new_buffer_len);
		if ((status&NSCAPI::message_modified) == NSCAPI::message_modified) {
			buffer = std::string(new_buffer, new_buffer_len);
			p->deleteBuffer(&new_buffer);
		}
		if ((status&NSCAPI::message_routed) == NSCAPI::message_routed) {
			channel = new_channel_buffer;
			//p->deleteBuffer(new_channel_buffer);
			return NSCAPI::message_routed;
		}
		if ((status&NSCAPI::message_ignored) == NSCAPI::message_ignored)
			return NSCAPI::message_ignored;
		if ((status&NSCAPI::message_digested) == NSCAPI::message_digested)
			return NSCAPI::message_ignored;
	}
	return NSCAPI::isfalse;
}

NSCAPI::errorReturn NSClientT::register_submission_listener(unsigned int plugin_id, const char* channel) {
	channels_.register_listener(plugin_id, channel);
	return NSCAPI::isSuccess;
}
NSCAPI::errorReturn NSClientT::register_routing_listener(unsigned int plugin_id, const char* channel) {
	try {
		routers_.register_listener(plugin_id, channel);
	} catch (const std::exception &e) {
		LOG_ERROR_CORE("Failed to register channel: " + utf8::cvt<std::string>(channel) + ": " + utf8::utf8_from_native(e.what()));
		return NSCAPI::hasFailed;
	}
	return NSCAPI::isSuccess;
}

NSCAPI::errorReturn NSClientT::send_notification(const char* channel, std::string &request, std::string &response) {
	boost::shared_lock<boost::shared_mutex> readLock(m_mutexRW, boost::get_system_time() + boost::posix_time::milliseconds(5000));
	if (!readLock.owns_lock()) {
		LOG_ERROR_CORE("FATAL ERROR: Could not get read-mutex.");
		return NSCAPI::hasFailed;
	}

	std::string schannel = channel;
	try {
		int count = 0;
		while (reroute(schannel, request)==NSCAPI::message_routed && count++ <= 10) {
			LOG_DEBUG_CORE_STD("Re-routing message to: " + schannel);
		}
		if (count >= 10) {
			LOG_ERROR_CORE("More then 10 routes, discarding message...");
			return NSCAPI::hasFailed;
		}
	} catch (nsclient::plugins_list_exception &e) {
		LOG_ERROR_CORE("Error routing channel: " + schannel + ": " + utf8::utf8_from_native(e.what()));
		return NSCAPI::hasFailed;
	} catch (...) {
		LOG_ERROR_CORE("Error routing channel: " + schannel);
		return NSCAPI::hasFailed;
	}

	bool found = false;
	BOOST_FOREACH(std::string cur_chan, strEx::s::splitEx(schannel, std::string(","))) {
		if (cur_chan == "log") {
			Plugin::SubmitRequestMessage msg;
			msg.ParseFromString(request);
			for (int i=0;i<msg.payload_size();i++) {
				LOG_INFO_CORE("Logging notification: " + msg.payload(i).message());
			}
			found = true;
			nscapi::protobuf::functions::create_simple_submit_response(cur_chan, "TODO", NSCAPI::isSuccess, "seems ok", response);
			continue;
		}
		try {
			//LOG_ERROR_CORE_STD(_T("Notifying: ") + strEx::strip_hex(to_wstring(std::string(result,result_len))));
			BOOST_FOREACH(nsclient::plugin_type p, channels_.get(cur_chan)) {
				p->handleNotification(cur_chan.c_str(), request, response);
				found = true;
			}
		} catch (nsclient::plugins_list_exception &e) {
			LOG_ERROR_CORE("No handler for channel: " + schannel + ": " + utf8::utf8_from_native(e.what()));
			return NSCAPI::hasFailed;
		} catch (...) {
			LOG_ERROR_CORE("No handler for channel: " + schannel);
			return NSCAPI::hasFailed;
		}
	}
	if (!found) {
		LOG_ERROR_CORE("No handler for channel: " + schannel);
		return NSCAPI::hasFailed;
	}
	return NSCAPI::isSuccess;
}

std::string NSClientT::get_plugin_module_name(unsigned int plugin_id) {
	boost::shared_lock<boost::shared_mutex> readLock(m_mutexRW, boost::get_system_time() + boost::posix_time::milliseconds(5000));
	if (!readLock.owns_lock()) {
		LOG_ERROR_CORE("FATAL ERROR: Could not get read-mutex.");
		return "";
	}
	BOOST_FOREACH(plugin_type plugin, plugins_) {
		if (plugin->get_id() == plugin_id)
			return plugin->getModule();
	}
	return "";
}
void NSClientT::listPlugins() {
	boost::shared_lock<boost::shared_mutex> readLock(m_mutexRW, boost::get_system_time() + boost::posix_time::milliseconds(5000));
	if (!readLock.owns_lock()) {
		LOG_ERROR_CORE("FATAL ERROR: Could not get read-mutex.");
		return;
	}
	for (pluginList::iterator it=plugins_.begin(); it != plugins_.end(); ++it) {
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

boost::filesystem::path NSClientT::getBasePath(void) {
	boost::unique_lock<boost::timed_mutex> lock(internalVariables, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!lock.owns_lock()) {
		LOG_ERROR_CORE("FATAL ERROR: Could not get mutex.");
		return "";
	}
	if (!basePath.empty())
		return basePath;
#ifdef WIN32
	unsigned int buf_len = 4096;
	wchar_t* buffer = new wchar_t[buf_len+1];
	GetModuleFileName(NULL, buffer, buf_len);
	std::wstring path = buffer;
	std::wstring::size_type pos = path.rfind('\\');
	basePath = path.substr(0, pos) + _T("\\");
	delete [] buffer;
#else 
	basePath = boost::filesystem::initial_path();
#endif
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
typedef DWORD (WINAPI *PFGetTempPath)(__in DWORD nBufferLength, __out  LPTSTR lpBuffer);
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
	if (hKernel)  
	{
		// Find PSAPI functions
		PFGetTempPath FGetTempPath = (PFGetTempPath)::GetProcAddress(hKernel, "GetTempPathW");
		if (FGetTempPath) {
			wchar_t* buffer = new wchar_t[buf_len+1];
			if (FGetTempPath(buf_len, buffer)) {
				tempPath = buffer;
			}
			delete [] buffer;
		}
	}
#else
	tempPath = "/tmp";
#endif
	return tempPath;
}

// Service API
NSClient* NSClientT::get_global_instance() {
	return &mainClient;
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
typedef BOOL (WINAPI *fnSHGetSpecialFolderPath)(HWND hwndOwner, LPTSTR lpszPath, int nFolder, BOOL fCreate);

__inline BOOL WINAPI _SHGetSpecialFolderPath(HWND hwndOwner, LPTSTR lpszPath, int nFolder, BOOL fCreate) {
	static fnSHGetSpecialFolderPath __SHGetSpecialFolderPath = NULL;
	if (!__SHGetSpecialFolderPath) {
		HMODULE hDLL = LoadLibrary(_T("shfolder.dll"));
		if (hDLL != NULL)
			__SHGetSpecialFolderPath = (fnSHGetSpecialFolderPath)GetProcAddress(hDLL,"SHGetSpecialFolderPathW");
	}
	if(__SHGetSpecialFolderPath)
		return __SHGetSpecialFolderPath(hwndOwner, lpszPath, nFolder, fCreate);
	return FALSE;
}
#endif

typedef boost::unordered_map<std::string, std::string> paths_type;
paths_type paths;
#define CONFIG_PATHS "/paths"

std::string NSClientT::getFolder(std::string key) {
	paths_type::const_iterator p = paths.find(key);
	if (p != paths.end())
		return p->second;
	std::string default_value = getBasePath().string();
	if (key == "certificate-path") {
		default_value = "${shared-path}/security";
	} else if (key == "module-path") {
		default_value = "${shared-path}/modules";
	} else if (key == "web-path") {
		default_value = "${shared-path}/web";
	} else if (key == "base-path") {
		default_value = getBasePath().string();
	} else if (key == "temp") {
		default_value = getTempPath().string();
	} else if (key == "shared-path" || key == "base-path" || key == "exe-path") {
		default_value = getBasePath().string();
	}
#ifdef WIN32
	else if (key == "common-appdata") {
		wchar_t buf[MAX_PATH+1];
		if (_SHGetSpecialFolderPath(NULL, buf, CSIDL_COMMON_APPDATA, FALSE))
			default_value = utf8::cvt<std::string>(buf);
		else 
			default_value = getBasePath().string();
	}
#else
	if (key == "etc") {
		default_value = "/etc";
	}
#endif
	try {
		if (settings_manager::get_core()->is_ready()) {
			std::string path = settings_manager::get_settings()->get_string(CONFIG_PATHS, key, default_value);
			settings_manager::get_core()->register_key(0xffff, CONFIG_PATHS, key, settings::settings_core::key_string, "Path for " + key, "", default_value, false, false);
			paths[key] = path;
			return path;
		}
	} catch (const settings::settings_exception &e) {
		// TODO: Maybe this should be fixed!
		paths[key] = default_value;
	}
	return default_value;
}

std::string NSClientT::expand_path(std::string file) {
	std::string::size_type pos = file.find('$');
	while (pos != std::string::npos) {
		std::string::size_type pstart = file.find('{', pos);
		std::string::size_type pend = file.find('}', pstart);
		std::string key = file.substr(pstart+1, pend-2);

		std::string tmp = file;
		strEx::replace(file, "${" + key + "}", getFolder(key));
		if (file == tmp)
			pos = file.find_first_of('$', pos+1);
		else
			pos = file.find_first_of('$');
	}
	return file;
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
	for (int i=0;i<request.payload_size();i++) {
		Plugin::SettingsResponseMessage::Response* rp = response.add_payload();
		try {
			const Plugin::SettingsRequestMessage::Request &r = request.payload(i);
			if (r.has_inventory()) {
				const Plugin::SettingsRequestMessage::Request::Inventory &q = r.inventory(); 
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
							if (q.fetch_keys()) {
								t.start("fetching keys");
								std::list<std::string> list = settings_manager::get_core()->get_reg_keys(path, fetch_samples);
								t.end();
								BOOST_FOREACH(const std::string &key, list) {
									settings::settings_core::key_description desc = settings_manager::get_core()->get_registred_key(path, key);
									Plugin::SettingsResponseMessage::Response::Inventory *rpp = rp->add_inventory();
									rpp->mutable_node()->set_path(path);
									rpp->mutable_node()->set_key(key);
									rpp->mutable_info()->set_title(desc.title);
									rpp->mutable_info()->set_description(desc.description);
									rpp->mutable_info()->set_advanced(desc.advanced);
									rpp->mutable_info()->set_sample(desc.is_sample);
									rpp->mutable_info()->mutable_default_value()->set_type(Plugin::Common_DataType_STRING);
									rpp->mutable_info()->mutable_default_value()->set_string_data(desc.defValue);
									if (desc.type == NSCAPI::key_string) {
										try {
											rpp->mutable_value()->set_string_data(settings_manager::get_settings()->get_string(q.node().path(), q.node().key()));
										} catch (settings::settings_exception &e) {}
									} else if (desc.type == NSCAPI::key_integer) {
										try {
											rpp->mutable_value()->set_int_data(settings_manager::get_settings()->get_int(q.node().path(), q.node().key()));
										} catch (settings::settings_exception &e) {}
									} else if (desc.type == NSCAPI::key_bool) {
										try {
											rpp->mutable_value()->set_bool_data(settings_manager::get_settings()->get_bool(q.node().path(), q.node().key()));
										} catch (settings::settings_exception &e) {}
									} else {
										LOG_ERROR_CORE("Invalid type");
									}
									settings_add_plugin_data(desc.plugins, module_cache, rpp->mutable_info(), this);
								}
							}
						}
					} else {
						std::string path = q.node().path();
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
						if (q.fetch_keys()) {
							t.start("fetching keys");
							std::list<std::string> list = settings_manager::get_core()->get_reg_keys(path, fetch_samples);
							t.end();
							BOOST_FOREACH(const std::string &key, list) {
								t.start("fetching keys");
								settings::settings_core::key_description desc = settings_manager::get_core()->get_registred_key(path, key);
								t.end();
								Plugin::SettingsResponseMessage::Response::Inventory *rpp = rp->add_inventory();
								rpp->mutable_node()->set_path(path);
								rpp->mutable_node()->set_key(key);
								rpp->mutable_info()->set_title(desc.title);
								rpp->mutable_info()->set_description(desc.description);
								rpp->mutable_info()->set_advanced(desc.advanced);
								rpp->mutable_info()->set_sample(desc.is_sample);
								rpp->mutable_info()->mutable_default_value()->set_type(Plugin::Common_DataType_STRING);
								rpp->mutable_info()->mutable_default_value()->set_string_data(desc.defValue);
								if (desc.type == NSCAPI::key_string) {
									try {
										std::string value = settings_manager::get_settings()->get_string(path, key);
										rpp->mutable_value()->set_type(Plugin::Common_DataType_STRING);
										rpp->mutable_value()->set_string_data(value);
									} catch (settings::settings_exception &e) {}
								} else if (desc.type == NSCAPI::key_integer) {
									try {
										int value = settings_manager::get_settings()->get_int(path, key);
										rpp->mutable_value()->set_type(Plugin::Common_DataType_INT);
										rpp->mutable_value()->set_int_data(value);
									} catch (settings::settings_exception &e) {}
								} else if (desc.type == NSCAPI::key_bool) {
									try {
										bool value = settings_manager::get_settings()->get_bool(path, key);
										rpp->mutable_value()->set_type(Plugin::Common_DataType_BOOL);
										rpp->mutable_value()->set_bool_data(value);
									} catch (settings::settings_exception &e) {}
								} else {
									LOG_ERROR_CORE("Invalid type");
								}
								settings_add_plugin_data(desc.plugins, module_cache, rpp->mutable_info(), this);
							}
						}
					}
					rp->mutable_result()->set_status(Plugin::Common_Status_StatusType_STATUS_OK);
				}
			} else if (r.has_query()) {
				const Plugin::SettingsRequestMessage::Request::Query &q = r.query(); 
				Plugin::SettingsResponseMessage::Response::Query *rpp = rp->mutable_query();
				rpp->mutable_node()->CopyFrom(q.node());
				if (q.node().has_key()) {
					rpp->mutable_value()->set_type(q.type());
					if (q.type() == Plugin::Common_DataType_STRING) {
						rpp->mutable_value()->set_string_data(settings_manager::get_settings()->get_string(q.node().path(), q.node().key(), q.default_value().string_data()));
					} else if (q.type() == Plugin::Common_DataType_INT) {
						rpp->mutable_value()->set_int_data(settings_manager::get_settings()->get_int(q.node().path(), q.node().key(), q.default_value().int_data()));
					} else if (q.type() == Plugin::Common_DataType_BOOL) {
						rpp->mutable_value()->set_bool_data(settings_manager::get_settings()->get_bool(q.node().path(), q.node().key(), q.default_value().bool_data()));
					} else {
						LOG_ERROR_CORE("Invalid type");
					}
				} else {
					rpp->mutable_value()->set_type(Plugin::Common_DataType_LIST);
					if (q.has_recursive() && q.recursive()) {
						BOOST_FOREACH(const std::string &key, settings_manager::get_settings()->get_sections(q.node().path())) {
							rpp->mutable_value()->add_list_data(key);
						}
					} else {
						BOOST_FOREACH(const std::string &key, settings_manager::get_settings()->get_keys(q.node().path())) {
							rpp->mutable_value()->add_list_data(key);
						}
					}
				}
				rp->mutable_result()->set_status(Plugin::Common_Status_StatusType_STATUS_OK);
			} else if (r.has_registration()) {
				const Plugin::SettingsRequestMessage::Request::Registration &q = r.registration(); 
				rp->mutable_registration();
				if (q.node().has_key()) {
					settings_manager::get_core()->register_key(r.plugin_id(), q.node().path(), q.node().key(), settings::settings_core::key_string, q.info().title(), q.info().description(), q.info().default_value().string_data(), q.info().advanced(), q.info().sample());
				} else {
					settings_manager::get_core()->register_path(r.plugin_id(), q.node().path(), q.info().title(), q.info().description(), q.info().advanced(), q.info().sample());
				}
				rp->mutable_result()->set_status(Plugin::Common_Status_StatusType_STATUS_OK);
			} else if (r.has_update()) {
				const Plugin::SettingsRequestMessage::Request::Update &p = r.update(); 
				rp->mutable_update();
				if (p.value().type() == Plugin::Common_DataType_STRING) {
					settings_manager::get_settings()->set_string(p.node().path(), p.node().key(), p.value().string_data());
				} else if (p.value().type() == Plugin::Common_DataType_BOOL) {
					settings_manager::get_settings()->set_bool(p.node().path(), p.node().key(), p.value().bool_data());
				} else if (p.value().type() == Plugin::Common_DataType_INT) {
					settings_manager::get_settings()->set_int(p.node().path(), p.node().key(), p.value().int_data());
				} else {
					LOG_ERROR_CORE("Invalid data type");
				}
				rp->mutable_result()->set_status(Plugin::Common_Status_StatusType_STATUS_OK);
			} else if (r.has_control()) {
				const Plugin::SettingsRequestMessage::Request::Control &p = r.control(); 
				rp->mutable_control();
				if (p.command() == Plugin::Settings_Command_LOAD) {
					if (p.has_context() && p.context().size() > 0)
						settings_manager::get_core()->migrate_from(p.context());
					else
						settings_manager::get_settings()->load();
					settings_manager::get_settings()->reload();
					rp->mutable_result()->set_status(Plugin::Common_Status_StatusType_STATUS_OK);
				} else if (p.command() == Plugin::Settings_Command_SAVE) {
					if (p.has_context() && p.context().size() > 0)
						settings_manager::get_core()->migrate_to(p.context());
					else
						settings_manager::get_settings()->save();
					rp->mutable_result()->set_status(Plugin::Common_Status_StatusType_STATUS_OK);
				} else {
					rp->mutable_result()->set_status(Plugin::Common_Status_StatusType_STATUS_OK);
					rp->mutable_result()->set_message("Unknown command");
				}
			} else if (r.has_status()) {
				const Plugin::SettingsRequestMessage::Request::Control &p = r.control(); 
				rp->mutable_status()->set_has_changed(settings_manager::get_core()->is_dirty());
				rp->mutable_status()->set_context(settings_manager::get_settings()->get_context());
				rp->mutable_status()->set_type(settings_manager::get_settings()->get_type());
				if (p.command() == Plugin::Settings_Command_LOAD) {
					if (p.has_context() && p.context().size() > 0)
						settings_manager::get_core()->migrate_from(p.context());
					else
						settings_manager::get_settings()->load();
					settings_manager::get_settings()->reload();
					rp->mutable_result()->set_status(Plugin::Common_Status_StatusType_STATUS_OK);
				} else if (p.command() == Plugin::Settings_Command_SAVE) {
					if (p.has_context() && p.context().size() > 0)
						settings_manager::get_core()->migrate_to(p.context());
					else
						settings_manager::get_settings()->save();
					rp->mutable_result()->set_status(Plugin::Common_Status_StatusType_STATUS_OK);
				} else {
					rp->mutable_result()->set_status(Plugin::Common_Status_StatusType_STATUS_OK);
					rp->mutable_result()->set_message("Unknown command");
				}
			} else {
				rp->mutable_result()->set_status(Plugin::Common_Status_StatusType_STATUS_OK);
				rp->mutable_result()->set_message("Settings error: Invalid action");
				LOG_ERROR_CORE_STD("Settings error: Invalid action");
			}
		} catch (settings::settings_exception &e) {
			rp->mutable_result()->set_status(Plugin::Common_Status_StatusType_STATUS_OK);
			rp->mutable_result()->set_message("Settings error: " + e.reason());
			LOG_ERROR_CORE_STD("Settings error: " + e.reason());
		} catch (const std::exception &e) {
			rp->mutable_result()->set_status(Plugin::Common_Status_StatusType_STATUS_OK);
			rp->mutable_result()->set_message("Settings error: " + utf8::utf8_from_native(e.what()));
			LOG_ERROR_CORE_STD("Settings error: " + utf8::utf8_from_native(e.what()));
		} catch (...) {
			rp->mutable_result()->set_status(Plugin::Common_Status_StatusType_STATUS_OK);
			rp->mutable_result()->set_message("Settings error");
			LOG_ERROR_CORE_STD("Settings error");
		}
	}
	//BOOST_FOREACH(const std::string &s, t.get()) {
	//	LOG_DEBUG_CORE(s);
	//}
	*response_buffer_len = response.ByteSize();
	*response_buffer = new char[*response_buffer_len + 10];
	response.SerializeToArray(*response_buffer, *response_buffer_len);
	return NSCAPI::isSuccess;
}

NSCAPI::errorReturn NSClientT::registry_query(const char *request_buffer, const unsigned int request_buffer_len, char **response_buffer, unsigned int *response_buffer_len) {
	try {
		std::string response_string;
		Plugin::RegistryRequestMessage request;
		Plugin::RegistryResponseMessage response;
		nscapi::protobuf::functions::create_simple_header(response.mutable_header());
		modules_type module_cache;
		request.ParseFromArray(request_buffer, request_buffer_len);
		for (int i=0;i<request.payload_size();i++) {
			const Plugin::RegistryRequestMessage::Request &r = request.payload(i);
			if (r.has_inventory()) {
				const Plugin::RegistryRequestMessage::Request::Inventory &q = r.inventory(); 
				Plugin::RegistryResponseMessage::Response* rp = response.add_payload();
				for (int i=0;i<q.type_size();i++) {
					Plugin::Registry_ItemType type = q.type(i);
					if (type == Plugin::Registry_ItemType_QUERY || type == Plugin::Registry_ItemType_ALL) {
						BOOST_FOREACH(const std::string &command, commands_.list_commands()) {
							nsclient::commands::command_info info = commands_.describe(command);
							Plugin::RegistryResponseMessage::Response::Inventory *rpp = rp->add_inventory();
							rpp->set_name(command);
							rpp->set_type(Plugin::Registry_ItemType_COMMAND);
							rpp->mutable_info()->add_plugin(add_plugin_data(module_cache, info.plugin_id, this));
							rpp->mutable_info()->set_title(info.name);
							rpp->mutable_info()->set_description(info.description);
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
					if (type == Plugin::Registry_ItemType_PLUGIN || type == Plugin::Registry_ItemType_ALL) {
						boost::shared_lock<boost::shared_mutex> readLock(m_mutexRW, boost::get_system_time() + boost::posix_time::milliseconds(5000));
						if (readLock.owns_lock()) {
							BOOST_FOREACH(plugin_type plugin, plugins_) {
								Plugin::RegistryResponseMessage::Response::Inventory *rpp = rp->add_inventory();
								rpp->set_name(plugin->getModule());
								rpp->set_type(Plugin::Registry_ItemType_COMMAND);
								rpp->mutable_info()->add_plugin(plugin->getModule());
								rpp->mutable_info()->set_title(plugin->getName());
								rpp->mutable_info()->set_description(plugin->getDescription());
							}
						} else {
							LOG_ERROR_CORE("FATAL ERROR: Could not get read-mutex.");
						}
					}
				}
				rp->mutable_result()->set_status(Plugin::Common_Status_StatusType_STATUS_OK);
			} else if (r.has_registration()) {
				const Plugin::RegistryRequestMessage::Request::Registration registration = r.registration();
				Plugin::RegistryResponseMessage::Response* rp = response.add_payload();
				if (registration.type() == Plugin::Registry_ItemType_QUERY) {
					commands_.register_command(registration.plugin_id(), registration.name(), registration.info().description());
					std::string description = "Alternative name for: " + registration.name();
					for (int i=0;i<registration.alias_size();i++) {
						commands_.register_alias(registration.plugin_id(), registration.alias(i), description);
					}
				} else if (registration.type() == Plugin::Registry_ItemType_HANDLER) {
					channels_.register_listener(registration.plugin_id(), registration.name());
				} else if (registration.type() == Plugin::Registry_ItemType_ROUTER) {
					routers_.register_listener(registration.plugin_id(), registration.name());
				} else if (registration.type() == Plugin::Registry_ItemType_PLUGIN) {
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
				rp->mutable_result()->set_status(Plugin::Common_Status_StatusType_STATUS_OK);
			} else {
				LOG_ERROR_CORE("Registration query: Unsupported action");
			}
		}
		*response_buffer_len = response.ByteSize();
		*response_buffer = new char[*response_buffer_len + 10];
		response.SerializeToArray(*response_buffer, *response_buffer_len);
	} catch (settings::settings_exception e) {
		LOG_ERROR_CORE_STD("Failed query: " + e.reason());
		return NSCAPI::hasFailed;
	} catch (const std::exception &e) {
		LOG_ERROR_CORE_STD("Failed query: " + utf8::utf8_from_native(e.what()));
		return NSCAPI::hasFailed;
	} catch (...) {
		LOG_ERROR_CORE("Failed query");
		return NSCAPI::hasFailed;
	}
	return NSCAPI::isSuccess;
}


#ifdef _WIN32
void NSClientT::handle_session_change(unsigned long dwSessionId, bool logon) {

}
#endif
