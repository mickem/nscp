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
//#include <Socket.h>
#include <config.h>
#ifdef WIN32
#include <Userenv.h>
#include <Lmcons.h>
//#ifdef DEBUG
#include <crtdbg.h>
//#endif
#endif
//#include <remote_processes.hpp>
//#include <winsvc.h>
//#include <Userenv.h>
//#include <Lmcons.h>
//#include <remote_processes.hpp>
#include "core_api.h"
#include "../helpers/settings_manager/settings_manager_impl.h"
#include <settings/macros.h>
#include "simple_client.hpp"
#include "settings_client.hpp"
#include "service_manager.hpp"
#include "settings_logger_impl.hpp"
#include <nscapi/nscapi_helper.hpp>
#include <nscapi/functions.hpp>

#include <settings/client/settings_client.hpp>
#include "cli_parser.hpp"
#include "../version.hpp"

#include <protobuf/plugin.pb.h>

#ifdef USE_BREAKPAD
#include <breakpad/exception_handler_win32.hpp>
// Used for breakpad crash handling
static ExceptionManager *g_exception_manager = NULL;
#endif

NSClient mainClient;	// Global core instance.


void NSClientT::log_debug(const char* file, const int line, std::wstring message) {
	std::string s = nsclient::logger_helper::create_debug(file, line, message);
	mainClient.reportMessage(s);
}
void NSClientT::log_error(const char* file, const int line, std::wstring message) {
	std::string s = nsclient::logger_helper::create_error(file, line, message);
	mainClient.reportMessage(s);
}
void NSClientT::log_error(const char* file, const int line, std::string message) {
	std::string s = nsclient::logger_helper::create_error(file, line, utf8::cvt<std::wstring>(message));
	mainClient.reportMessage(s);
}
void NSClientT::log_info(const char* file, const int line, std::wstring message) {
	std::string s = nsclient::logger_helper::create_info(file, line, message);
	mainClient.reportMessage(s);
}

#define LOG_CRITICAL_CORE(msg) { std::string s = nsclient::logger_helper::create_error(__FILE__, __LINE__, msg); mainClient.reportMessage(s); }
#define LOG_CRITICAL_CORE_STD(msg) LOG_CRITICAL_CORE(std::wstring(msg))
#define LOG_ERROR_CORE(msg) { std::string s = nsclient::logger_helper::create_error(__FILE__, __LINE__, msg); mainClient.reportMessage(s); }
#define LOG_ERROR_CORE_STD(msg) LOG_ERROR_CORE(std::wstring(msg))
#define LOG_INFO_CORE(msg) { std::string s = nsclient::logger_helper::create_info(__FILE__, __LINE__, msg); mainClient.reportMessage(s); }
#define LOG_INFO_CORE_STD(msg) LOG_INFO_CORE(std::wstring(msg))
#define LOG_DEBUG_CORE(msg) { if (mainClient.logDebug()) { std::string s = nsclient::logger_helper::create_debug(__FILE__, __LINE__, msg); mainClient.reportMessage(s); } }
#define LOG_DEBUG_CORE_STD(msg) LOG_DEBUG_CORE(std::wstring(msg))

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
		std::wstring cmdln = _T("\"") + command + _T("\" -channel __") + strEx::itos(dwSessionId) + _T("__");
		return tray_starter::startTrayHelper(dwSessionId, command, cmdln);
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

bool is_module(boost::filesystem::wpath file ) 
{
#ifdef WIN32
	return boost::ends_with(file.string(), _T(".dll"));
#else
	return boost::ends_with(file.string(), _T(".so"));
#endif
}
/**
 * Application startup point
 *
 * @param argc Argument count
 * @param argv[] Argument array
 * @param envp[] Environment array
 * @return exit status
 */
int nscp_main(int argc, wchar_t* argv[]);

#ifdef WIN32
int wmain(int argc, wchar_t* argv[], wchar_t* envp[]) { return nscp_main(argc, argv); }
#else
int main(int argc, char* argv[]) { 
	wchar_t **wargv = new wchar_t*[argc];
	for (int i=0;i<argc;i++) {
		std::wstring s = to_wstring(argv[i]);
		wargv[i] = new wchar_t[s.length()+10];
		wcscpy(wargv[i], s.c_str());
	}
	int ret = nscp_main(argc, wargv); 
	for (int i=0;i<argc;i++) {
		delete [] wargv[i];
	}
	delete [] wargv;
}
#endif

int nscp_main(int argc, wchar_t* argv[])
{
	srand( (unsigned)time( NULL ) );
	cli_parser parser(&mainClient);
	return parser.parse(argc, argv);

// 	int nRetCode = 0;
// 	if ( (argc > 1) && ((*argv[1] == '-') || (*argv[1] == '/')) ) {
// 		if (false) {
// 		} else if ( wcscasecmp( _T("encrypt"), argv[1]+1 ) == 0 ) {
// 			std::wstring password;
// 			if (!settings_manager::init_settings()) {
// 				std::wcout << _T("Could not find settings") << std::endl;;
// 				return 1;
// 			}
// 			std::wcout << _T("Enter password to encrypt (has to be a single word): ");
// 			std::wcin >> password;
// 			std::wstring xor_pwd = Encrypt(password);
// 			std::wcout << _T("obfuscated_password=") << xor_pwd << std::endl;
// 			std::wstring outPasswd = Decrypt(xor_pwd);
// 			if (password != outPasswd) 
// 				std::wcout << _T("ERROR: Password did not match: ") << outPasswd<< std::endl;
// 			settings_manager::destroy_settings();
// 			return 0;
// 		} else if ( wcscasecmp( _T("about"), argv[1]+1 ) == 0 ) {
// 			try {
// 				unsigned int next_plugin_id = 0;
// 				LOG_INFO_CORE(APPLICATION_NAME _T(" (C) Michael Medin - michael<at>medin<dot>name"));
// 				LOG_INFO_CORE(_T("Version: ") CURRENT_SERVICE_VERSION);
// 				LOG_INFO_CORE(_T("Architecture: ") SZARCH);
// 
// 				boost::filesystem::wpath pluginPath = (boost::filesystem::wpath)mainClient.getBasePath() / _T("modules");
// 				LOG_INFO_CORE_STD(_T("Looking at plugins in: ") + pluginPath.string());
// 
// 				boost::filesystem::wdirectory_iterator end_itr; // default construction yields past-the-end
// 				for ( boost::filesystem::wdirectory_iterator itr( pluginPath ); itr != end_itr; ++itr ) {
// 					if ( !is_directory(itr->status()) ) {
// 						std::wstring file= itr->leaf();
// 						LOG_INFO_CORE_STD(_T("Found: ") + file);
// 						if (is_module(pluginPath / file)) {
// 							NSCPlugin *plugin = new NSCPlugin(next_plugin_id++, pluginPath / file, _T(""));
// 							std::wstring name = _T("<unknown>");
// 							std::wstring description = _T("<unknown>");
// 							try {
// 								plugin->load_dll();
// 								name = plugin->getName();
// 								description = plugin->getDescription();
// 							} catch(NSPluginException& e) {
// 								LOG_ERROR_CORE_STD(_T("Exception raised: ") + e.error_ + _T(" in module: ") + e.file_);
// 							} catch (std::exception e) {
// 								LOG_ERROR_CORE_STD(_T("exception loading plugin: ") + strEx::string_to_wstring(e.what()));
// 							} catch (...) {
// 								LOG_ERROR_CORE_STD(_T("Unknown exception loading plugin"));
// 							}
// 							LOG_INFO_CORE_STD(_T("* ") + name + _T(" (") + file + _T(")"));
// 							std::list<std::wstring> list = strEx::splitEx(description, _T("\n"));
// 							for (std::list<std::wstring>::const_iterator cit = list.begin(); cit != list.end(); ++cit) {
// 								LOG_INFO_CORE_STD(_T("    ") + *cit);
// 							}
// 						}
// 					}
// 				}
// 				LOG_INFO_CORE_STD(_T("Done listing plugins from: ") + pluginPath.string());
// 				return true;
// 			} catch (std::exception &e) {
// 				LOG_ERROR_CORE_STD(_T("Exception: ") + to_wstring(e.what()));
// 			} catch (...) {
// 				LOG_ERROR_CORE_STD(_T("Unknown Exception: "));
// 			}
// 			return false;
// 		} else if ( wcscasecmp( _T("d"), argv[1]+1 ) == 0 ) {
// 			// Run command from command line (like NRPE) but with debug enabled
// 		} else if ( wcscasecmp( _T("c"), argv[1]+1 ) == 0 ) {
// 			// Run command from command line (like NRPE)
// 			mainClient.enableDebug(false);
// 			mainClient.initCore(true);
// 			std::wstring command, args, msg, perf;
// 			if (argc > 2)
// 				command = argv[2];
// 			for (int i=3;i<argc;i++) {
// 				if (i!=3) args += _T(" ");
// 				args += argv[i];
// 			}
// 			nRetCode = mainClient.inject(command, args, msg, perf);
// 			std::wcout << msg << _T("|") << perf << std::endl;
// 			mainClient.exitCore(true);
// 			return nRetCode;
// 		} else {
// 			std::wcerr << _T("Usage: -version, -about, -install, -uninstall, -start, -stop, -encrypt -settings") << std::endl;
// 			std::wcerr << _T("Usage: [-noboot] <ModuleName> <commnd> [arguments]") << std::endl;
// 			return -1;
// 		}
// 		return nRetCode;
// 	return nRetCode;
}

std::list<std::wstring> NSClientT::list_commands() {
	return commands_.list();
}

bool contains_plugin(NSClientT::plugin_alias_list_type &ret, std::wstring alias, std::wstring plugin) {
	std::pair<std::wstring,std::wstring> v;
	BOOST_FOREACH(v, ret.equal_range(alias)) {
		if (v.second == plugin)
			return true;
	}
	return false;
}
NSClientT::plugin_alias_list_type NSClientT::find_all_plugins(bool active) {
	plugin_alias_list_type ret;

	settings::string_list list = settings_manager::get_settings()->get_keys(MAIN_MODULES_SECTION);
	BOOST_FOREACH(std::wstring plugin, list) {
		std::wstring alias = settings_manager::get_settings()->get_string(MAIN_MODULES_SECTION, plugin);
		if (plugin == _T("enabled")) {
			plugin = alias;
			alias = _T("");
		} else if ((active && plugin == _T("disabled")) || (active && alias == _T("disabled")))
			continue;
		else if (plugin == _T("disabled")) {
			plugin = alias;
			alias = _T("");
		} else if (alias == _T("disabled")) {
			alias = _T("");
		}
		if (!alias.empty()) {
			std::wstring tmp = plugin;
			plugin = alias;
			alias = tmp;
		}
		LOG_DEBUG_CORE_STD(_T("Found: ") + plugin + _T(" as ") + alias);
		if (plugin.length() > 4 && plugin.substr(plugin.length()-4) == _T(".dll"))
			plugin = plugin.substr(0, plugin.length()-4);
		ret.insert(plugin_alias_list_type::value_type(alias, plugin));
	}
	if (!active) {
		boost::filesystem::wpath pluginPath = expand_path(_T("${module-path}"));
		boost::filesystem::wdirectory_iterator end_itr; // default construction yields past-the-end
		for ( boost::filesystem::wdirectory_iterator itr( pluginPath ); itr != end_itr; ++itr ) {
			if ( !is_directory(itr->status()) ) {
				boost::filesystem::wpath file= itr->leaf();
				if (is_module(pluginPath  / file) && !contains_plugin(ret, _T(""), file.string()))
					ret.insert(plugin_alias_list_type::value_type(_T(""), file.string()));
			}
		}
	} 
	return ret;
}

// NSClientT::plugin_info_list NSClientT::get_all_plugins() {
// 	boost::filesystem::wpath pluginPath = expand_path(_T("${module-path}"));
// 	plugin_alias_list_type plugins = find_all_plugins(false);
// 	plugin_info_list ret;
// 	std::pair<std::wstring,std::wstring> v;
// 
// 	BOOST_FOREACH(v, plugins) {
// 		plugin_info_type info;
// 		info.dll = v.second;
// 		try {
// 			LOG_DEBUG_STD(_T("Attempting to fake load: ") + v.second + _T(" as ") + v.first);
// 			NSCPlugin plugin(next_plugin_id_++, pluginPath / v.second, v.first);
// 			plugin.load_dll();
// 			plugin.load_plugin(NSCAPI::dontStart);
// 			info.name = plugin.getName();
// 			info.description = plugin.getDescription();
// 			plugin.unload();
// 		} catch (NSPluginException e) {
// 			LOG_CRITICAL_STD(_T("Error loading: ") + e.file_ + _T(" root cause: ") + e.error_);
// 		} catch (...) {
// 			LOG_CRITICAL_STD(_T("Unknown Error loading: ") + info.dll);
// 		}
// 		ret.push_back(info);
// 	}
// 	return ret;
// }


void NSClientT::load_all_plugins(int mode) {
	boost::filesystem::wpath pluginPath;
	{
		try {
			pluginPath = expand_path(_T("${module-path}"));
		} catch (std::exception &e) {
			LOG_CRITICAL_CORE_STD(_T("Failed to load plugins: ") + to_wstring(e.what()) + _T(" for ") + expand_path(_T("${module-path}")));
			return;
		}
		plugin_alias_list_type plugins = find_all_plugins(false);
		std::pair<std::wstring,std::wstring> v;

		BOOST_FOREACH(v, plugins) {
			try {
				addPlugin(pluginPath / v.second, v.first);
			} catch (NSPluginException &e) {
				LOG_CRITICAL_CORE_STD(_T("Failed to register plugin: ") + e.what());
			} catch (...) {
				LOG_CRITICAL_CORE_STD(_T("Failed to register plugin key: ") + v.second);
			}
		}
	}

	try {
		loadPlugins(mode);
	} catch (...) {
		LOG_ERROR_CORE_STD(_T("Unknown exception loading plugins"));
	}

// 		std::wstring desc;
// 		std::wstring name = v.second;
// 		try {
// 			NSCPlugin plugin(next_plugin_id_++, pluginPath / v.second, v.first);
// 			name = plugin.getModule();
// 			plugin.load_dll();
// 			plugin.load_plugin(mode);
// 			desc = plugin.getName() + _T(" - ");
// 			desc += plugin.getDescription();
// 			plugin.unload();
// 		} catch (NSPluginException e) {
// 			desc += _T("unknown module");
// 			LOG_CRITICAL_STD(_T("Error loading: ") + e.file_ + _T(" root cause: ") + e.error_);
// 		} catch (...) {
// 			desc += _T("unknown module");
// 			LOG_CRITICAL_STD(_T("Unknown Error loading: ") + name);
// 		}
// 		try {
// 			if (v.first.empty() && !settings_manager::get_settings()->has_key(MAIN_MODULES_SECTION, v.second))
// 				settings_manager::get_core()->register_key(MAIN_MODULES_SECTION, name, settings::settings_core::key_string, desc, desc, _T("disabled"), false);
// 		} catch (...) {
// 			LOG_CRITICAL_STD(_T("Failed to register plugin key: ") + name);
// 		}
	}

void NSClientT::session_error(std::string file, unsigned int line, std::wstring msg) {
	std::string s = nsclient::logger_helper::create_error(file.c_str(), line, msg); 
	reportMessage(s);
}

void NSClientT::session_info(std::string file, unsigned int line, std::wstring msg) {
	std::string s = nsclient::logger_helper::create_info(file.c_str(), line, msg); 
	reportMessage(s);
}




//////////////////////////////////////////////////////////////////////////
// Service functions


struct nscp_settings_provider : public settings_manager::provider_interface {
	virtual std::wstring expand_path(std::wstring file) {
		return mainClient.expand_path(file);
	}
	virtual void log_fatal_error(std::wstring error) {
		LOG_CRITICAL_CORE_STD(error);
	}
	virtual settings::logger_interface* create_logger() {
		return new settings_logger();
	}
	std::wstring get_data(std::wstring key) {
		// TODO
		return _T("");
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
bool NSClientT::boot_init() {
	LOG_INFO_CORE(SERVICE_NAME _T(" booting..."));

	if (!settings_manager::init_settings(&provider, context_)) {
		return false;
	}
	LOG_INFO_CORE(_T("Booted settings subsystem..."));

	bool crash_submit = false;
	bool crash_archive = false;
	bool crash_restart = false;
	std::wstring crash_url, crash_folder, crash_target, log_level;
	try {

		sh::settings_registry settings(settings_manager::get_proxy());

		settings.add_path_to_settings()
			(_T("log"),			_T("LOG SETTINGS"), _T("Section for configuring the log handling."))
			(_T("shared session"),	_T("SHRED SESSION"), _T("Section for configuring the shared session."))
			(_T("crash"),			_T("CRASH HANDLER"), _T("Section for configuring the crash handler."))
			;

		settings.add_key_to_settings(_T("log"))
			(_T("level"), sh::wstring_key(&log_level, _T("INFO")),
			_T("LOG LEVEL"), _T("Log level to use"))
			;

		settings.add_key_to_settings(_T("shared session"))
			(_T("enabled"), sh::bool_key(&enable_shared_session_ , false),
			_T("LOG LEVEL"), _T("Log level to use"))
			;

		settings.add_key_to_settings(_T("crash"))
			(_T("submit"), sh::bool_key(&crash_submit, false),
			_T("SUBMIT CRASHREPORTS"), _T("Submit crash reports to nsclient.org (or your configured submission server)"))

			(_T("archive"), sh::bool_key(&crash_archive, true),
			_T("ARCHIVE CRASHREPORTS"), _T("Archive crash reports in the archive folder"))

			(_T("restart"), sh::bool_key(&crash_restart, true),
			_T("RESTART"), _T("Submit crash reports to nsclient.org (or your configured submission server)"))

			(_T("restart target"), sh::wstring_key(&crash_target, get_service_control().get_service_name()),
			_T("RESTART SERVICE NAME"), _T("The url to submit crash reports to"))

			(_T("submit url"), sh::wstring_key(&crash_url, CRASH_SUBMIT_URL),
			_T("SUBMISSION URL"), _T("The url to submit crash reports to"))

			(_T("archive folder"), sh::wpath_key(&crash_folder, CRASH_ARCHIVE_FOLDER),
			CRASH_ARCHIVE_FOLDER_KEY, _T("The folder to archive crash dumps in"))
			;

		settings.register_all();
		settings.notify();

	} catch (settings::settings_exception e) {
		LOG_ERROR_CORE_STD(_T("Could not find settings: ") + e.getMessage());
	}

#ifdef USE_BREAKPAD

	if (!g_exception_manager) {
		g_exception_manager = new ExceptionManager(false);

		g_exception_manager->setup_app(to_wstring(APPLICATION_NAME), to_wstring(STRPRODUCTVER), to_wstring(STRPRODUCTDATE));

		if (crash_restart) {
			LOG_DEBUG_CORE(_T("On crash: restart: ") + crash_target);
			g_exception_manager->setup_restart(crash_target);
		}

		bool crashHandling = false;
		if (crash_submit) {
			g_exception_manager->setup_submit(false, crash_url);
			LOG_DEBUG_CORE(_T("Submitting crash dumps to central server: ") + crash_url);
			crashHandling = true;
		}
		if (crash_archive) {
			g_exception_manager->setup_archive(crash_folder);
			LOG_DEBUG_CORE(_T("Archiving crash dumps in: ") + crash_folder);
			crashHandling = true;
		}
		if (!crashHandling) {
			LOG_ERROR_CORE(_T("No crash handling configured"));
		} else {
			//g_exception_manager->StartMonitoring();
		}
	}
#else
	LOG_ERROR_CORE(_T("Warning Not compiled with google breakpad support!"));
#endif


	if (enable_shared_session_) {
		LOG_INFO_CORE(_T("shared session not ported yet!..."));
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
		LOG_ERROR_CORE_STD(_T("COM exception: ") + e.getMessage());
		return false;
	} catch (...) {
		LOG_ERROR_CORE(_T("Unknown exception iniating COM..."));
		return false;
	}
#endif
	return true;
}
bool NSClientT::boot_load_plugins(bool boot) {
	LOG_DEBUG_CORE(_T("booting::loading plugins"));
	try {
		boost::filesystem::wpath pluginPath = expand_path(_T("${module-path}"));
		plugin_alias_list_type plugins = find_all_plugins(true);
		std::pair<std::wstring,std::wstring> v;
		BOOST_FOREACH(v, plugins) {
			std::wstring file = NSCPlugin::get_plugin_file(v.second);
			std::wstring alias = v.first;
			LOG_DEBUG_CORE_STD(_T("Processing plugin: ") + file + _T(" as ") + alias);
			try {
				addPlugin(pluginPath / boost::filesystem::wpath(file), alias);
			} catch(const NSPluginException& e) {
				LOG_ERROR_CORE_STD(_T("Exception raised: '") + e.error_ + _T("' in module: ") + e.file_);
			} catch (std::exception e) {
				LOG_ERROR_CORE_STD(_T("exception loading plugin: ") + file + strEx::string_to_wstring(e.what()));
				return false;
			} catch (...) {
				LOG_ERROR_CORE_STD(_T("Unknown exception loading plugin: ") + file);
				return false;
			}
		}
	} catch (settings::settings_exception e) {
		LOG_ERROR_CORE_STD(_T("Failed to set settings file") + e.getMessage());
	} catch (...) {
		LOG_ERROR_CORE_STD(_T("Unknown exception when loading plugins"));
		return false;
	}
	try {
		loadPlugins(boot?NSCAPI::normalStart:NSCAPI::dontStart);
	} catch (...) {
		LOG_ERROR_CORE_STD(_T("Unknown exception loading plugins"));
		return false;
	}
	LOG_DEBUG_CORE_STD(APPLICATION_NAME _T(" - ") CURRENT_SERVICE_VERSION _T(" Started!"));
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

bool NSClientT::exitCore(bool boot) {
	LOG_DEBUG_CORE(_T("Attempting to stop"));
	try {
		LOG_DEBUG_CORE(_T("Stopping: NON Message Handling Plugins"));
		mainClient.unloadPlugins(false);
	} catch(NSPluginException e) {
		LOG_ERROR_CORE_STD(_T("Exception raised when unloading non msg plguins: ") + e.error_ + _T(" in module: ") + e.file_);
	} catch(...) {
		LOG_ERROR_CORE_STD(_T("Unknown exception raised when unloading non msg plugins"));
	}
#ifdef WIN32
	LOG_DEBUG_CORE(_T("Stopping: COM helper"));
	try {
		com_helper_.unInitialize();
	} catch (com_helper::com_exception e) {
		LOG_ERROR_CORE_STD(_T("COM exception: ") + e.getMessage());
	} catch (...) {
		LOG_ERROR_CORE_STD(_T("Unknown exception uniniating COM..."));
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
	LOG_DEBUG_CORE(_T("Stopping: Settings instance"));
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
	try {
// 		if (shared_server_.get() != NULL) {
// 			LOG_DEBUG_STD(_T("Stopping: shared server"));
// 			shared_server_->set_handler(NULL);
// 			shared_server_->close_session();
// 		}
	} catch(...) {
		LOG_ERROR_CORE_STD(_T("UNknown exception raised: When closing shared session"));
	}
	try {
		LOG_DEBUG_CORE(_T("Stopping: Message handling Plugins"));
		mainClient.unloadPlugins(true);
	} catch(NSPluginException e) {
		LOG_ERROR_CORE_STD(_T("Exception raised when unloading msg plugins: ") + e.error_ + _T(" in module: ") + e.file_);
	} catch(...) {
		LOG_ERROR_CORE_STD(_T("UNknown exception raised: When stopping message plguins"));
	}
	LOG_INFO_CORE(_T("Stopped succcessfully"));
	logger_master_.stop_slave();
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
void NSClientT::unloadPlugins(bool unloadLoggers) {
	{
		boost::unique_lock<boost::shared_mutex> writeLock(m_mutexRW, boost::get_system_time() + boost::posix_time::seconds(10));
		if (!writeLock.owns_lock()) {
			LOG_ERROR_CORE(_T("FATAL ERROR: Could not get read-mutex (003)."));
			return;
		}
		if (unloadLoggers)
			logger_master_.remove_all_plugins();
	}
	{
		boost::shared_lock<boost::shared_mutex> readLock(m_mutexRW, boost::get_system_time() + boost::posix_time::milliseconds(5000));
		if (!readLock.owns_lock()) {
			LOG_ERROR_CORE(_T("FATAL ERROR: Could not get read-mutex (004)."));
			return;
		}
		for (pluginList::reverse_iterator it = plugins_.rbegin(); it != plugins_.rend(); ++it) {
			plugin_type p = *it;
			if (!p)
				continue;
			try {
				if (unloadLoggers || !p->hasMessageHandler()) {
					LOG_DEBUG_CORE_STD(_T("Unloading plugin: ") + p->getModule() + _T("..."));
					p->unload();
				} else {
					LOG_DEBUG_CORE_STD(_T("Skipping log plugin: ") + p->getModule() + _T("..."));
				}
			} catch(NSPluginException e) {
				LOG_ERROR_CORE_STD(_T("Exception raised when unloading plugin: ") + e.error_ + _T(" in module: ") + e.file_);
			} catch(...) {
				LOG_ERROR_CORE_STD(_T("Unknown exception raised when unloading plugin"));
			}
		}
	}
	{
		boost::unique_lock<boost::shared_mutex> writeLock(m_mutexRW, boost::get_system_time() + boost::posix_time::seconds(10));
		if (!writeLock.owns_lock()) {
			LOG_ERROR_CORE(_T("FATAL ERROR: Could not get read-mutex (005)."));
			return;
		}
		commands_.remove_all();
		for (pluginList::iterator it = plugins_.begin(); it != plugins_.end();) {
			plugin_type p = (*it);
			try {
				if (!p && (unloadLoggers|| !p->isLoaded())) {
					it = plugins_.erase(it);
					//delete p;
					continue;
				}
			} catch(NSPluginException e) {
				LOG_ERROR_CORE_STD(_T("Exception raised when unloading plugin: ") + e.error_ + _T(" in module: ") + e.file_);
			} catch(...) {
				LOG_ERROR_CORE(_T("Unknown exception raised when unloading plugin"));
			}
			it++;
		}
	}
}

void NSClientT::loadPlugins(NSCAPI::moduleLoadMode mode) {
	bool hasBroken = false;
	{
		boost::shared_lock<boost::shared_mutex> readLock(m_mutexRW, boost::get_system_time() + boost::posix_time::milliseconds(5000));
		if (!readLock.owns_lock()) {
			LOG_ERROR_CORE(_T("FATAL ERROR: Could not get read-mutex (006)."));
			return;
		}
		for (pluginList::iterator it=plugins_.begin(); it != plugins_.end();) {
			LOG_DEBUG_CORE_STD(_T("Loading plugin: ") + (*it)->getName() + _T(" as ") + (*it)->get_alias());
			try {
				if (!(*it)->load_plugin(mode)) {
					LOG_ERROR_CORE_STD(_T("Plugin refused to load: ") + (*it)->getModule());
					it = plugins_.erase(it);
				} else
					++it;
			} catch (NSPluginException e) {
				it = plugins_.erase(it);
				LOG_ERROR_CORE_STD(_T("Could not load plugin: ") + e.file_ + _T(": ") + e.error_);
			} catch (...) {
				it = plugins_.erase(it);
				LOG_ERROR_CORE_STD(_T("Could not load plugin: ") + (*it)->getModule());
			}
		}
	}
	logger_master_.all_plugins_loaded();
}
/**
 * Load and add a plugin to various internal structures
 * @param plugin The plug-in instance to load. The pointer is managed by the 
 */
NSClientT::plugin_type NSClientT::addPlugin(boost::filesystem::wpath file, std::wstring alias) {
	{
		LOG_DEBUG_CORE_STD(_T("addPlugin(") + file.string() + _T(" as ") + alias + _T(")"));
		// Check if this is a duplicate plugin (if so return that instance)
		boost::unique_lock<boost::shared_mutex> writeLock(m_mutexRW, boost::get_system_time() + boost::posix_time::seconds(10));
		if (!writeLock.owns_lock()) {
			LOG_ERROR_CORE(_T("FATAL ERROR: Could not get read-mutex (007a)."));
			return plugin_type();
		}

		BOOST_FOREACH(plugin_type plug, plugins_) {
			if (plug->is_duplicate(file, alias)) {
				LOG_DEBUG_CORE_STD(_T("Found duplicate plugin returning old ") + to_wstring(plug->get_id()));
				return plug;
			}
		}

	}


	plugin_type plugin(new NSCPlugin(next_plugin_id_++, file, alias));
	plugin->load_dll();
	{
		boost::unique_lock<boost::shared_mutex> writeLock(m_mutexRW, boost::get_system_time() + boost::posix_time::seconds(10));
		if (!writeLock.owns_lock()) {
			LOG_ERROR_CORE(_T("FATAL ERROR: Could not get read-mutex (007b)."));
			return plugin;
		}

		plugins_.insert(plugins_.end(), plugin);
		if (plugin->hasCommandHandler())
			commands_.add_plugin(plugin);
		if (plugin->hasNotificationHandler())
			channels_.add_plugin(plugin);
		if (plugin->hasMessageHandler())
			logger_master_.add_plugin(plugin);
		if (plugin->has_routing_handler())
			routers_.add_plugin(plugin);
		//settings_manager::get_core()->register_key(_T("/modules"), plugin->getModule(), settings::settings_core::key_string, plugin->getName(), plugin->getDescription(), _T(""), false);
		// TODO add comments elsewhere to the settings store for all loaded modules...
	}
	return plugin;
}


std::wstring NSClientT::describeCommand(std::wstring command) {
	return commands_.describe(command);
}
std::list<std::wstring> NSClientT::getAllCommandNames() {
	return commands_.list();
}
void NSClientT::registerCommand(unsigned int id, std::wstring cmd, std::wstring desc) {
	return commands_.register_command(id, cmd, desc);
}

NSCAPI::nagiosReturn NSClientT::inject(std::wstring command, std::wstring arguments, std::wstring &msg, std::wstring & perf) {
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

		std::list<std::wstring> args;
		strEx::parse_command(arguments, args);
		std::string request, response;
		nscapi::functions::create_simple_query_request(command, args, request);
		NSCAPI::nagiosReturn ret = injectRAW(command.c_str(), request, response);
		if (response.empty()) {
			LOG_ERROR_CORE(_T("No data retutned from command"));
			return NSCAPI::returnUNKNOWN;
		}
		nscapi::functions::parse_simple_query_response(response, msg, perf);
		return ret;
	}
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
NSCAPI::nagiosReturn NSClientT::injectRAW(const wchar_t* raw_command, std::string &request, std::string &response) {
	std::wstring cmd = nsclient::commands::make_key(raw_command);
	if (logDebug()) {
		LOG_DEBUG_CORE_STD(_T("Injecting: ") + cmd + _T("..."));
	}
	/*if (shared_client_.get() != NULL && shared_client_->hasMaster()) {
		try {
			std::wstring msg, perf;
			int returnCode = shared_client_->inject(command, arrayBuffer::arrayBuffer2string(argument, argLen, _T(" ")), L' ', true, msg, perf);
			NSCHelper::wrapReturnString(returnMessageBuffer, returnMessageBufferLen, msg, returnCode);
			return NSCHelper::wrapReturnString(returnPerfBuffer, returnPerfBufferLen, perf, returnCode);
		} catch (nsclient_session::session_exception &e) {
			LOG_ERROR_STD(_T("Failed to inject remote command: ") + e.what());
			int returnCode = NSCHelper::wrapReturnString(returnMessageBuffer, returnMessageBufferLen, _T("Failed to inject remote command: ") + e.what(), NSCAPI::returnCRIT);
			return NSCHelper::wrapReturnString(returnPerfBuffer, returnPerfBufferLen, _T(""), returnCode);
		} catch (...) {
			LOG_ERROR_STD(_T("Failed to inject remote command: Unknown exception"));
			int returnCode = NSCHelper::wrapReturnString(returnMessageBuffer, returnMessageBufferLen, _T("Failed to inject remote command:  + e.what()"), NSCAPI::returnCRIT);
			return NSCHelper::wrapReturnString(returnPerfBuffer, returnPerfBufferLen, _T(""), returnCode);
		}
	} else */{
		boost::shared_lock<boost::shared_mutex> readLock(m_mutexRW, boost::get_system_time() + boost::posix_time::milliseconds(5000));
		if (!readLock.owns_lock()) {
			LOG_ERROR_CORE(_T("FATAL ERROR: Could not get read-mutex (008)."));
			return NSCAPI::returnUNKNOWN;
		}
		try {
			nsclient::commands::plugin_type plugin = commands_.get(cmd);
			if (!plugin) {
				LOG_ERROR_CORE(_T("No handler for command: ") + cmd + _T(" avalible commands: ") + commands_.to_wstring());
				return NSCAPI::returnIgnored;
			}
			NSCAPI::nagiosReturn c = plugin->handleCommand(cmd.c_str(), request, response);
			LOG_DEBUG_CORE_STD(_T("Result ") + cmd + _T(": ") + nscapi::plugin_helper::translateReturn(c));
			return c;
		} catch (nsclient::commands::command_exception &e) {
			LOG_ERROR_CORE(_T("No handler for command: ") + cmd + _T(": ") + to_wstring(e.what()));
			return NSCAPI::returnIgnored;
		} catch (...) {
			LOG_ERROR_CORE(_T("Error handling command: ") + cmd);
			return NSCAPI::returnIgnored;
		}
	}
}


int NSClientT::simple_exec(std::wstring module, std::wstring command, std::vector<std::wstring> arguments, std::list<std::wstring> &resp) {
	bool found = false;
	std::string request;
	std::list<std::string> responses;
	nscapi::functions::create_simple_exec_request(command, arguments, request);
	int ret = 0;
	{
		boost::shared_lock<boost::shared_mutex> readLock(m_mutexRW, boost::get_system_time() + boost::posix_time::seconds(5));
		if (!readLock.owns_lock()) {
			LOG_ERROR_CORE(_T("FATAL ERROR: Could not get read-mutex (001)."));
			return -1;
		}
		BOOST_FOREACH(plugin_type p, plugins_) {
			if (p) {
				if ((module.empty() || p->getModule() == module)&&(p->has_command_line_exec())) {
					LOG_DEBUG_CORE_STD(_T("Found module: ") + p->getName() + _T("..."));
					try {
						std::string response;
						ret = p->commandLineExec(command.c_str(), request, response);
						found = true;
						if (ret != NSCAPI::returnIgnored && !response.empty())
							responses.push_back(response);
					} catch (NSPluginException e) {
						LOG_ERROR_CORE_STD(_T("Could not execute command: ") + e.error_ + _T(" in ") + e.file_);
						return -1;
					}
				}
			}
		}
	}
	if (!found && !module.empty()) {
		try {
			boost::filesystem::wpath file = NSCPlugin::get_filename(getBasePath() / boost::filesystem::wpath(_T("modules")), module);
			if (boost::filesystem::is_regular(file)) {
				plugin_type plugin = addPlugin(file, _T(""));
				if (plugin) {
					LOG_DEBUG_CORE_STD(_T("Loading plugin: ") + plugin->getName() + _T("..."));
					plugin->load_plugin(NSCAPI::dontStart);
					std::string response;
					ret = plugin->commandLineExec(command.c_str(), request, response);
					if (ret != NSCAPI::returnIgnored && !response.empty())
						responses.push_back(response);
				} else {
					LOG_ERROR_CORE_STD(_T("Failed to load: ") + std::wstring(module));
					return 1;
				}
			} else {
				LOG_ERROR_CORE_STD(_T("Failed to load: ") + std::wstring(module));
				return 1;
			}
		} catch (const NSPluginException &e) {
			resp.push_back(_T("Module (") + e.file_ + _T(") was not found: ") + e.error_);
			LOG_INFO_CORE_STD(_T("Module (") + e.file_ + _T(") was not found: ") + e.error_);
		} catch(const std::exception &e) {
			resp.push_back(_T("Module (") + module + _T(") was not found: ") + utf8::cvt<std::wstring>(e.what()));
			LOG_INFO_CORE_STD(_T("Module (") + module + _T(") was not found: ") + utf8::cvt<std::wstring>(e.what()));
			return 1;
		} catch(...) {
			resp.push_back(_T("Module (") + module + _T(") was not found..."));
			LOG_INFO_CORE_STD(_T("Module (") + module + _T(") was not found..."));
			return 1;
		}
	}
	BOOST_FOREACH(std::string &r, responses) {
		try {
			nscapi::functions::parse_simple_exec_result(r, resp);
		} catch (std::exception &e) {
			resp.push_back(_T("Failed to extract return message: ") + utf8::cvt<std::wstring>(e.what()));
			LOG_ERROR_CORE_STD(resp.back());
			return NSCAPI::returnUNKNOWN;
		}
	}
	return ret;
}


NSCAPI::nagiosReturn NSClientT::exec_command(const wchar_t* raw_command, std::string &request, std::string &response) {
	std::list<std::string> responses;
	bool found = false;
	{
		boost::shared_lock<boost::shared_mutex> readLock(m_mutexRW, boost::get_system_time() + boost::posix_time::seconds(5));
		if (!readLock.owns_lock()) {
			LOG_ERROR_CORE(_T("FATAL ERROR: Could not get read-mutex (001)."));
			return -1;
		}
		BOOST_FOREACH(plugin_type p, plugins_) {
			if (p && p->has_command_line_exec()) {
				try {
					std::string response;
					NSCAPI::nagiosReturn r = p->commandLineExec(raw_command, request, response);
					if (r != NSCAPI::returnIgnored && !response.empty()) {
						LOG_DEBUG_CORE_STD(_T("Got response from: ") + p->getName());
						found = true;
						responses.push_back(response);
					}
				} catch (NSPluginException e) {
					LOG_ERROR_CORE_STD(_T("Could not execute command: ") + e.error_ + _T(" in ") + e.file_);
				}
			}
		}
	}

	Plugin::ExecuteResponseMessage response_message;
	nscapi::functions::create_simple_header(response_message.mutable_header());

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
		return NSCAPI::returnOK;
	return NSCAPI::returnIgnored;
}


NSCAPI::errorReturn NSClientT::reroute(std::wstring &channel, const wchar_t* command, std::string &buffer) {
	BOOST_FOREACH(nsclient::plugin_type p, routers_.get(channel)) {
		wchar_t *new_channel_buffer;
		char *new_buffer;
		unsigned int new_buffer_len;
		int status = p->route_message(channel.c_str(), command, buffer.c_str(), buffer.size(), &new_channel_buffer, &new_buffer, &new_buffer_len);
		if (status&NSCAPI::message_modified == NSCAPI::message_modified) {
			buffer = std::string(new_buffer, new_buffer_len);
			p->deleteBuffer(&new_buffer);
		}
		if (status&NSCAPI::message_routed == NSCAPI::message_routed) {
			channel = new_channel_buffer;
			//p->deleteBuffer(new_channel_buffer);
			return NSCAPI::message_routed;
		}
		if (status&NSCAPI::message_ignored == NSCAPI::message_ignored)
			return NSCAPI::message_ignored;
		if (status&NSCAPI::message_digested == NSCAPI::message_digested)
			return NSCAPI::message_ignored;
	}
	return NSCAPI::isfalse;
}

NSCAPI::errorReturn NSClientT::register_submission_listener(unsigned int plugin_id, const wchar_t* channel) {
	channels_.register_listener(plugin_id, channel);
	return NSCAPI::isSuccess;
}
NSCAPI::errorReturn NSClientT::register_routing_listener(unsigned int plugin_id, const wchar_t* channel) {
	routers_.register_listener(plugin_id, channel);
	return NSCAPI::isSuccess;
}

NSCAPI::errorReturn NSClientT::send_notification(const wchar_t* channel, const wchar_t* command, char* buffer, unsigned int buffer_len) {
	boost::shared_lock<boost::shared_mutex> readLock(m_mutexRW, boost::get_system_time() + boost::posix_time::milliseconds(5000));
	if (!readLock.owns_lock()) {
		LOG_ERROR_CORE(_T("FATAL ERROR: Could not get read-mutex (009)."));
		return NSCAPI::hasFailed;
	}

	std::wstring schannel = channel;
	std::string sbuffer = std::string(buffer, buffer_len);
	try {
		int count = 0;
		while (reroute(schannel, command, sbuffer)==NSCAPI::message_routed && count++ <= 10) {
			LOG_DEBUG_CORE_STD(_T("Re-routing message to: ") + schannel);
		}
		if (count >= 10) {
			LOG_ERROR_CORE(_T("More then 10 routes, discarding message..."));
			return NSCAPI::hasFailed;
		}
	} catch (nsclient::plugins_list_exception &e) {
		LOG_ERROR_CORE(_T("Error routing channel: ") + std::wstring(channel) + _T(": ") + to_wstring(e.what()) + _T("Current channels: ") + channels_.to_wstring());
		return NSCAPI::hasFailed;
	} catch (...) {
		LOG_ERROR_CORE(_T("Error routing channel: ") + std::wstring(channel));
		return NSCAPI::hasFailed;
	}

	try {
		//LOG_ERROR_CORE_STD(_T("Notifying: ") + strEx::strip_hex(to_wstring(std::string(result,result_len))));
		bool found = false;
		BOOST_FOREACH(nsclient::plugin_type p, channels_.get(schannel)) {
			p->handleNotification(schannel.c_str(), command, sbuffer.c_str(), sbuffer.length());
			found = true;
		}
		if (!found) {
			LOG_ERROR_CORE_STD(_T("No one listens for events from: ") + schannel + _T(" (") + std::wstring(channel) + _T(")"));
			return NSCAPI::hasFailed;
		}
		return NSCAPI::isSuccess;
	} catch (nsclient::plugins_list_exception &e) {
		LOG_ERROR_CORE(_T("No handler for channel: ") + std::wstring(channel) + _T(": ") + to_wstring(e.what()));
		return NSCAPI::hasFailed;
	} catch (...) {
		LOG_ERROR_CORE(_T("Error handling channel: ") + std::wstring(channel));
		return NSCAPI::hasFailed;
	}
}

void NSClientT::listPlugins() {
	boost::shared_lock<boost::shared_mutex> readLock(m_mutexRW, boost::get_system_time() + boost::posix_time::milliseconds(5000));
	if (!readLock.owns_lock()) {
		LOG_ERROR_CORE(_T("FATAL ERROR: Could not get read-mutex (010)."));
		return;
	}
	for (pluginList::iterator it=plugins_.begin(); it != plugins_.end(); ++it) {
		try {
			if ((*it)->isBroken()) {
				std::wcout << (*it)->getModule() << _T(": ") << _T("broken") << std::endl;
			} else {
				std::wcout << (*it)->getModule() << _T(": ") << (*it)->getName() << std::endl;
			}
		} catch (NSPluginException e) {
			LOG_ERROR_CORE_STD(_T("Could not load plugin: ") + e.file_ + _T(": ") + e.error_);
		}
	}

}

bool NSClientT::logDebug() {
	if (debug_ == log_state_unknown) {
		debug_ = log_state_looking;
		try {
			if (settings_manager::get_settings_no_wait()->get_bool(_T("log"), _T("debug"), false) == 1)
				debug_ = log_state_debug;
			else
				debug_ = log_state_nodebug;
		} catch (settings::settings_exception e) {
			debug_ = log_state_unknown;
			return false;
		}
	} else if (debug_ == log_state_looking) 
		return false;
	return (debug_ == log_state_debug);
}

/**
 * Report a message to all logging enabled modules.
 *
 * @param msgType Message type 
 * @param file Filename generally __FILE__
 * @param line  Line number, generally __LINE__
 * @param message The message as a human readable string.
 */
void NSClientT::reportMessage(std::string data) {
	try {
		logger_master_.log(data);
	} catch (...) {
		logger_master_.log_fatal_error("Caught UNKNOWN Exception when trying to log a message");
	}
}
boost::filesystem::wpath NSClientT::getBasePath(void) {
	boost::unique_lock<boost::timed_mutex> lock(internalVariables, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!lock.owns_lock()) {
		LOG_ERROR_CORE(_T("FATAL ERROR: Could not get mutex."));
		return _T("FATAL ERROR");
	}
	if (!basePath.empty())
		return basePath;
	unsigned int buf_len = 4096;
#ifdef WIN32
	wchar_t* buffer = new wchar_t[buf_len+1];
	GetModuleFileName(NULL, buffer, buf_len);
	std::wstring path = buffer;
	std::wstring::size_type pos = path.rfind('\\');
	basePath = path.substr(0, pos) + _T("\\");
	delete [] buffer;
#else 
	basePath = to_wstring(boost::filesystem::initial_path().string());
#endif
	try {
		settings_manager::get_core()->set_base(basePath);
	} catch (settings::settings_exception e) {
		LOG_ERROR_CORE_STD(_T("Failed to set settings file: ") + e.getMessage());
	} catch (...) {
		LOG_ERROR_CORE_STD(_T("Failed to set settings file"));
	}
	return basePath;
}


std::wstring Encrypt(std::wstring str, unsigned int algorithm) {
	unsigned int len = 0;
	NSAPIEncrypt(algorithm, str.c_str(), static_cast<unsigned int>(str.size()), NULL, &len);
	len+=2;
	wchar_t *buf = new wchar_t[len+1];
	NSCAPI::errorReturn ret = NSAPIEncrypt(algorithm, str.c_str(), static_cast<unsigned int>(str.size()), buf, &len);
	if (ret == NSCAPI::isSuccess) {
		std::wstring ret = buf;
		delete [] buf;
		return ret;
	}
	return _T("");
}
std::wstring Decrypt(std::wstring str, unsigned int algorithm) {
	unsigned int len = 0;
	NSAPIDecrypt(algorithm, str.c_str(), static_cast<unsigned int>(str.size()), NULL, &len);
	len+=2;
	wchar_t *buf = new wchar_t[len+1];
	NSCAPI::errorReturn ret = NSAPIDecrypt(algorithm, str.c_str(), static_cast<unsigned int>(str.size()), buf, &len);
	if (ret == NSCAPI::isSuccess) {
		std::wstring ret = buf;
		delete [] buf;
		return ret;
	}
	return _T("");
}

void NSClientT::nsclient_log_error(std::string file, int line, std::wstring error) {
	std::string s = nsclient::logger_helper::create_error(file, line, error); 
	reportMessage(s.c_str());
}



// Service API
NSClient* NSClientT::get_global_instance() {
	return &mainClient;
}
void NSClientT::handle_startup(std::wstring service_name) {
	service_name_ = service_name;
	boot_init();
	boot_load_plugins(true);
/*
	DWORD dwSessionId = remote_processes::getActiveSessionId();
	if (dwSessionId != 0xFFFFFFFF)
		tray_starter::start(dwSessionId);
	else
		LOG_ERROR_STD(_T("Failed to start tray helper:" ) + error::lookup::last_error());
		*/
}
void NSClientT::handle_shutdown(std::wstring service_name) {
	exitCore(true);
}

NSClientT::service_controller NSClientT::get_service_control() {
	return service_controller(service_name_);
}

void NSClientT::service_controller::stop() {
#ifdef WIN32
	serviceControll::StopNoWait(get_service_name());
#endif
}
void NSClientT::service_controller::start() {
#ifdef WIN32
	serviceControll::Start(get_service_name());
#endif
}
bool NSClientT::service_controller::is_started() {
#ifdef WIN32
	try {
		if (serviceControll::isStarted(get_service_name())) {
			return true;
		}
	} catch (...) {
		return false;
	}
#endif
	return false;
}

std::wstring NSClientT::expand_path(std::wstring file) {
	strEx::replace(file, _T("${certificate-path}"), _T("${shared-path}/security"));
	strEx::replace(file, _T("${module-path}"), _T("${shared-path}/modules"));
	
	strEx::replace(file, _T("${base-path}"), getBasePath().string());
#ifdef WIN32
	strEx::replace(file, _T("${shared-path}"), getBasePath().string());
#else
	strEx::replace(file, _T("${shared-path}"), getBasePath().string());
//	strEx::replace(file, _T("${shared-path}"), _T("/usr/share/nsclient++"));
#endif
	strEx::replace(file, _T("${exe-path}"), getBasePath().string());
	strEx::replace(file, _T("${etc}"), _T("/etc"));
	return file;
}

#ifdef _WIN32
void NSClientT::handle_session_change(unsigned long dwSessionId, bool logon) {

}
#endif
