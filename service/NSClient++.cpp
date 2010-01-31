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
#include <settings/Settings.h>
#include <charEx.h>
//#include <Socket.h>
#include <config.h>
#include <msvc_wrappers.h>
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
#include "settings_manager_impl.h"
#include <settings/macros.h>
#include <NSCHelper.h>
#include "simple_client.hpp"
#include "settings_client.hpp"
#include "service_manager.hpp"

#include "../proto/plugin.pb.h"

NSClient mainClient(SZSERVICENAME);	// Global core instance.
bool g_bConsoleLog = false;


//////////////////////////////////////////////////////////////////////////
// Log macros to simplify logging
// Generally names are of the form LOG_<severity>[_STD] 
// Where _STD indicates that strings are force wrapped inside a std::wstring
//
#define LOG_ERROR_STD(msg) LOG_ERROR(((std::wstring)msg).c_str())
#define LOG_ERROR(msg) \
	NSAPIMessage(NSCAPI::error, __FILEW__, __LINE__, msg)

#define LOG_CRITICAL_STD(msg) LOG_CRITICAL(((std::wstring)msg).c_str())
#define LOG_CRITICAL(msg) \
	NSAPIMessage(NSCAPI::critical, __FILEW__, __LINE__, msg)
#define LOG_MESSAGE_STD(msg) LOG_MESSAGE(((std::wstring)msg).c_str())
#define LOG_MESSAGE(msg) \
	NSAPIMessage(NSCAPI::log, __FILEW__, __LINE__, msg)

#define LOG_DEBUG_STD(msg) LOG_DEBUG(((std::wstring)msg).c_str())
#define LOG_DEBUG(msg) \
	NSAPIMessage(NSCAPI::debug, __FILEW__, __LINE__, msg)

#define LOG_ERROR_CORE(msg) reportMessage(NSCAPI::error, __FILEW__, __LINE__, msg)
#define LOG_ERROR_CORE_STD(msg) LOG_ERROR_CORE(std::wstring(msg))


#define SETTINGS_GET_BOOL_CORE(key) \
	settings_manager::get_settings()->get_bool(setting_keys::key ## _PATH, setting_keys::key, setting_keys::key ## _DEFAULT)

#define SETTINGS_GET_STRING_CORE(key) \
	settings_manager::get_settings()->get_string(setting_keys::key ## _PATH, setting_keys::key, setting_keys::key ## _DEFAULT)
/*
#define SETTINGS_SET_STRING_CORE(key, value) \
	Settings::get_settings()->set_string(setting_keys::key ## _PATH, setting_keys::key, value);
*/
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

	static bool start(unsigned long  dwSessionId) {
		boost::filesystem::wpath program = mainClient.getBasePath() / SETTINGS_GET_STRING_CORE(settings_def::SYSTRAY_EXE);
		std::wstring cmdln = _T("\"") + program.string() + _T("\" -channel __") + strEx::itos(dwSessionId) + _T("__");
		return tray_starter::startTrayHelper(dwSessionId, program.string(), cmdln);
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

#define XNSC_DEFINE_SETTING_KEY(name, tag) \
	name ## _SECTION \
	
 /**
 * RANDOM JUNK (MERGE HELP)
 */

void display(std::wstring title, std::wstring message) {
#ifdef WIN32
	::MessageBox(NULL, message.c_str(), title.c_str(), MB_OK|MB_ICONERROR);
#endif
	std::wcout << title << std::endl << message << std::endl;
}


bool is_module(boost::filesystem::wpath file ) 
{
	return boost::ends_with(file.string(), _T(".dll")) || boost::ends_with(file.string(), _T(".so"));
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
	int nRetCode = 0;
	if ( (argc > 1) && ((*argv[1] == '-') || (*argv[1] == '/')) ) {
		if (false) {
		} if ( wcscasecmp( _T("install"), argv[1]+1 ) == 0 ) {
			nsclient::client::service_manager mgr(argc-1,&argv[1]);
			return mgr.install();
		} else if ( wcscasecmp( _T("uninstall"), argv[1]+1 ) == 0 ) {
			nsclient::client::service_manager mgr(argc-1,&argv[1]);
			return mgr.uninstall();
		} else if ( wcscasecmp( _T("start"), argv[1]+1 ) == 0 ) {
			nsclient::client::service_manager mgr(argc-1,&argv[1]);
			return mgr.start();
		} else if ( wcscasecmp( _T("stop"), argv[1]+1 ) == 0 ) {
			nsclient::client::service_manager mgr(argc-1,&argv[1]);
			return mgr.stop();
		} else if ( wcscasecmp( _T("svc"), argv[1]+1 ) == 0 ) {
			nsclient::client::service_manager mgr(argc-1,&argv[1]);
			return mgr.print_command();
		} else if ( wcscasecmp( _T("encrypt"), argv[1]+1 ) == 0 ) {
			g_bConsoleLog = true;
			std::wstring password;
			if (!settings_manager::init_settings(mainClient.getBasePath())) {
				std::wcout << _T("Could not find settings") << std::endl;;
				return 1;
			}
			std::wcout << _T("Enter password to encrypt (has to be a single word): ");
			std::wcin >> password;
			std::wstring xor_pwd = Encrypt(password);
			std::wcout << _T("obfuscated_password=") << xor_pwd << std::endl;
			std::wstring outPasswd = Decrypt(xor_pwd);
			if (password != outPasswd) 
				std::wcout << _T("ERROR: Password did not match: ") << outPasswd<< std::endl;
			settings_manager::destroy_settings();
			return 0;
		} else if ( wcscasecmp( _T("about"), argv[1]+1 ) == 0 ) {
			try {
				g_bConsoleLog = true;
				LOG_MESSAGE(SZAPPNAME _T(" (C) Michael Medin - michael<at>medin<dot>name"));
				LOG_MESSAGE(_T("Version: ") SZVERSION);
				LOG_MESSAGE(_T("Architecture: ") SZARCH);

				boost::filesystem::wpath pluginPath = (boost::filesystem::wpath)mainClient.getBasePath() / _T("modules");
				LOG_MESSAGE_STD(_T("Looking at plugins in: ") + pluginPath.string());

				boost::filesystem::wdirectory_iterator end_itr; // default construction yields past-the-end
				for ( boost::filesystem::wdirectory_iterator itr( pluginPath ); itr != end_itr; ++itr ) {
					if ( !is_directory(itr->status()) ) {
						std::wstring file= itr->leaf();
						LOG_MESSAGE_STD(_T("Found: ") + file);
						if (is_module(pluginPath / file)) {
							NSCPlugin *plugin = new NSCPlugin(pluginPath / file);
							std::wstring name = _T("<unknown>");
							std::wstring description = _T("<unknown>");
							try {
								plugin->load_dll();
								name = plugin->getName();
								description = plugin->getDescription();
							} catch(const NSPluginException& e) {
								LOG_ERROR_STD(_T("Exception raised: ") + e.error_ + _T(" in module: ") + e.file_);
							} catch (std::exception e) {
								LOG_ERROR_STD(_T("exception loading plugin: ") + strEx::string_to_wstring(e.what()));
							} catch (...) {
								LOG_ERROR_STD(_T("Unknown exception loading plugin"));
							}
							LOG_MESSAGE_STD(_T("* ") + name + _T(" (") + file + _T(")"));
							std::list<std::wstring> list = strEx::splitEx(description, _T("\n"));
							for (std::list<std::wstring>::const_iterator cit = list.begin(); cit != list.end(); ++cit) {
								LOG_MESSAGE_STD(_T("    ") + *cit);
							}
						}
					}
				}
				LOG_MESSAGE_STD(_T("Done listing plugins from: ") + pluginPath.string());
				return true;
			} catch (std::exception &e) {
				LOG_ERROR_STD(_T("Exception: ") + to_wstring(e.what()));
			} catch (...) {
				LOG_ERROR_STD(_T("Unknown Exception: "));
			}
			return false;
		} else if ( wcscasecmp( _T("version"), argv[1]+1 ) == 0 ) {
			g_bConsoleLog = true;
			LOG_MESSAGE(SZAPPNAME _T(" Version: ") SZVERSION _T(", Plattform: ") SZARCH);
		} else if ( wcscasecmp( _T("d"), argv[1]+1 ) == 0 ) {
			// Run command from command line (like NRPE) but with debug enabled
		} else if ( wcscasecmp( _T("noboot"), argv[1]+1 ) == 0 ) {
			g_bConsoleLog = true;
			mainClient.enableDebug(false);
			mainClient.initCore(false);
			if (argc>=3)
				nRetCode = mainClient.commandLineExec(argv[2], argc-3, &argv[3]);
			else
				nRetCode = mainClient.commandLineExec(argv[2], 0, NULL);
			mainClient.exitCore(true);
			return nRetCode;
		} else if ( wcscasecmp( _T("c"), argv[1]+1 ) == 0 ) {
			// Run command from command line (like NRPE)
			g_bConsoleLog = true;
			mainClient.enableDebug(false);
			mainClient.initCore(true);
			std::wstring command, args, msg, perf;
			if (argc > 2)
				command = argv[2];
			for (int i=3;i<argc;i++) {
				if (i!=3) args += _T(" ");
				args += argv[i];
			}
			nRetCode = mainClient.inject(command, args, msg, perf);
			std::wcout << msg << _T("|") << perf << std::endl;
			mainClient.exitCore(true);
			return nRetCode;
		} else if ( wcscasecmp( _T("test"), argv[1]+1 ) == 0 ) {
			g_bConsoleLog = true;
			bool server = false;
			if (argc > 2 && wcscasecmp( _T("server"), argv[2] ) == 0 ) {
				server = true;
			}
			std::wcout << "Launching test mode - " << (server?_T("server mode"):_T("client mode")) << std::endl;
			LOG_MESSAGE_STD(_T("Booting: ") SZSERVICEDISPLAYNAME );
#ifdef WIN32
			try {
				if (serviceControll::isStarted(SZSERVICENAME)) {
					std::wcerr << "Service seems to be started, this is probably not a good idea..." << std::endl;
				}
			} catch (...) {
				// Empty by design
			}
#endif
			nsclient::simple_client client(&mainClient);
			client.start();
			return 0;
		} else if ( wcscasecmp( _T("settings"), argv[1]+1 ) == 0 ) {
			nsclient::settings_client client(&mainClient);
			g_bConsoleLog = true;
			if (argc > 2)
				client.parse(argv[2], argc-3, argv+3);
			else
				client.help();
			return 0;
		} else {
			std::wcerr << _T("Usage: -version, -about, -install, -uninstall, -start, -stop, -encrypt -settings") << std::endl;
			std::wcerr << _T("Usage: [-noboot] <ModuleName> <commnd> [arguments]") << std::endl;
			return -1;
		}
		return nRetCode;
	} else if (argc > 2) {
		g_bConsoleLog = true;
		std::wcout << _T(" * * * * * * * ") << std::endl;
		mainClient.initCore(true);
		if (argc>=3)
			nRetCode = mainClient.commandLineExec(argv[1], argc-2, &argv[2]);
		else
			nRetCode = mainClient.commandLineExec(argv[1], 0, NULL);
		mainClient.exitCore(true);
		return nRetCode;
	} else if (argc > 1) {
		g_bConsoleLog = true;
		mainClient.enableDebug(true);
		std::wcerr << _T("Invalid command line argument: ") << argv[1] << std::endl;
		std::wcerr << _T("Usage: -version, -about, -install, -uninstall, -start, -stop, -encrypt") << std::endl;
		std::wcerr << _T("Usage: [-noboot] <ModuleName> <commnd> [arguments]") << std::endl;
		return -1;
	}
	try {
		std::wcout << _T("Running as service...") << std::endl;
		mainClient.enableDebug(true);
		mainClient.start_and_wait();
	} catch (...) {
		std::wcerr << _T("Unknown exception in service") << std::endl;
	}
	return nRetCode;
}

void migrate() {}

std::list<std::wstring> NSClientT::list_commands() {
	return commands_.list();
}

NSClientT::plugin_info_list NSClientT::get_all_plugins() {
	plugin_info_list ret;
	boost::filesystem::wpath pluginPath = getBasePath() / boost::filesystem::wpath(_T("modules"));
	boost::filesystem::wdirectory_iterator end_itr; // default construction yields past-the-end
	for ( boost::filesystem::wdirectory_iterator itr( pluginPath ); itr != end_itr; ++itr ) {
		if ( !is_directory(itr->status()) ) {
			boost::filesystem::wpath file= itr->leaf();
			if (is_module(pluginPath  / file)) {
				plugin_info_type info;
				info.dll = itr->leaf();
				try {
					LOG_DEBUG_STD(_T("Attempting to fake load: ") + file.string());
					NSCPlugin plugin(pluginPath / file);
					plugin.load_dll();
					plugin.load_plugin(NSCAPI::dontStart);
					info.name = plugin.getName();
					info.description = plugin.getDescription();
					plugin.unload();
				} catch (NSPluginException e) {
					LOG_CRITICAL_STD(_T("Error loading: ") + e.file_ + _T(" root cause: ") + e.error_);
				} catch (...) {
					LOG_CRITICAL_STD(_T("Unknown Error loading: ") + file.string());
				}
				ret.push_back(info);
			}
		}
	}
	return ret;
}


void NSClientT::load_all_plugins(int mode) {
	boost::filesystem::wpath modPath = getBasePath() / boost::filesystem::wpath(_T("modules"));

	boost::filesystem::wdirectory_iterator end_itr; // default construction yields past-the-end
	for ( boost::filesystem::wdirectory_iterator itr( modPath ); itr != end_itr; ++itr ) {
		if ( !is_directory(itr->status()) ) {
			boost::filesystem::wpath file= itr->leaf();
			if (is_module(modPath / file)) {
				if (settings_manager::get_settings()->has_key(MAIN_MODULES_SECTION, file.string())) {
					if (settings_manager::get_settings()->get_string(MAIN_MODULES_SECTION, file.string()) == _T("disabled")) {
						try {
							LOG_DEBUG_STD(_T("Attempting to fake load: ") + file.string());
							NSCPlugin plugin(modPath / file);
							plugin.load_dll();
							plugin.load_plugin(mode);
							plugin.unload();
						} catch (NSPluginException e) {
							LOG_CRITICAL_STD(_T("Error loading: ") + e.file_ + _T(" root cause: ") + e.error_);
						} catch (...) {
							LOG_CRITICAL_STD(_T("Unknown Error loading: ") + file.string());
						}
					}
				} else {
					std::wstring desc;
					std::wstring name = file.string();
					try {
						NSCPlugin plugin(modPath / file);
						name = plugin.getModule();
						plugin.load_dll();
						plugin.load_plugin(mode);
						desc = plugin.getName() + _T(" - ");
						desc += plugin.getDescription();
						plugin.unload();
					} catch (NSPluginException e) {
						desc += _T("unknown module");
						LOG_CRITICAL_STD(_T("Error loading: ") + e.file_ + _T(" root cause: ") + e.error_);
					} catch (...) {
						desc += _T("unknown module");
						LOG_CRITICAL_STD(_T("Unknown Error loading: ") + file.string());
					}
					settings_manager::get_core()->register_key(MAIN_MODULES_SECTION, name, Settings::SettingsCore::key_string, desc, desc, _T("disabled"), false);
				}
			}
		} 
	}
}

void NSClientT::session_error(std::wstring file, unsigned int line, std::wstring msg) {
	NSAPIMessage(NSCAPI::error, file.c_str(), line, msg.c_str());
}

void NSClientT::session_info(std::wstring file, unsigned int line, std::wstring msg) {
	NSAPIMessage(NSCAPI::log, file.c_str(), line, msg.c_str());
}




//////////////////////////////////////////////////////////////////////////
// Service functions

/**
 * Initialize the program
 * @param boot true if we shall boot all plugins
 * @param attachIfPossible is true we will attach to a running instance.
 * @return success
 * @author mickem
 */
bool NSClientT::initCore(bool boot) {
	LOG_MESSAGE(_T("Attempting to start NSCLient++ - ") SZVERSION);
	if (!settings_manager::init_settings(getBasePath())) {
		return false;
	}
	LOG_MESSAGE(_T("Got settings subsystem..."));
	try {
		if (debug_)
			settings_manager::get_settings()->set_int(_T("log"), _T("debug"), 1);
			settings_manager::get_settings()->set_int(_T("Settings"), _T("shared_Session"), 1);
		enable_shared_session_ = SETTINGS_GET_BOOL_CORE(settings_def::SHARED_SESSION);
	} catch (SettingsException e) {
		LOG_ERROR_CORE_STD(_T("Could not find settings: ") + e.getMessage());
	}

	if (enable_shared_session_) {
		LOG_DEBUG_STD(_T("Enabling shared session..."));
		if (boot) {
			LOG_MESSAGE_STD(_T("shared session not ported yet!..."));
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
		} else {
			LOG_MESSAGE_STD(_T("shared session not ported yet!..."));
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
		}
	}
/*
	try {
		simpleSocket::WSAStartup();
	} catch (simpleSocket::SocketException e) {
		LOG_ERROR_STD(_T("Socket exception: ") + e.getMessage());
		return false;
	} catch (...) {
		LOG_ERROR_STD(_T("Unknown exception iniating socket..."));
		return false;
	}
*/
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
	if (boot) {
		try {
			Settings::string_list list = settings_manager::get_settings()->get_keys(MAIN_MODULES_SECTION);
			for (Settings::string_list::const_iterator cit = list.begin(); cit != list.end(); ++cit) {
				LOG_DEBUG_STD(_T("Processing plugin: " + *cit));
				try {
					if (settings_manager::get_settings()->get_string(MAIN_MODULES_SECTION, *cit) == _T("disabled")) {
						LOG_DEBUG_STD(_T("Not booting: ") + *cit + _T(" since it is disabled."));
						continue;
					}
				} catch (...) {
					// If we except we load the plugin in as-is
				}
				try {
					loadPlugin(getBasePath() / boost::filesystem::wpath(_T("modules")) / boost::filesystem::wpath(*cit));
				} catch(const NSPluginException& e) {
					LOG_ERROR_CORE_STD(_T("Exception raised: '") + e.error_ + _T("' in module: ") + e.file_);
					//return false;
				} catch (std::exception e) {
					LOG_ERROR_CORE_STD(_T("exception loading plugin: ") + (*cit) + strEx::string_to_wstring(e.what()));
					return false;
				} catch (...) {
					LOG_ERROR_CORE_STD(_T("Unknown exception loading plugin: ") + (*cit));
					return false;
				}
			}
		} catch (SettingsException e) {
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
		LOG_DEBUG_STD(_T("NSCLient++ - ") SZVERSION _T(" Started!"));
	}
	LOG_MESSAGE_STD(_T("NSCLient++ - ") SZVERSION _T(" Started!"));
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
	plugins_loaded_ = false;
	LOG_DEBUG(_T("Attempting to stop NSCLient++ - ") SZVERSION);
	if (boot) {
		try {
			LOG_DEBUG_STD(_T("Stopping: NON Message Handling Plugins"));
			mainClient.unloadPlugins(false);
		} catch(NSPluginException e) {
			LOG_ERROR_CORE_STD(_T("Exception raised when unloading non msg plguins: ") + e.error_ + _T(" in module: ") + e.file_);
		} catch(...) {
			LOG_ERROR_CORE_STD(_T("Unknown exception raised when unloading non msg plugins"));
		}
	}
#ifdef WIN32
	LOG_DEBUG_STD(_T("Stopping: COM helper"));
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
	LOG_DEBUG_STD(_T("Stopping: Settings instance"));
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
	if (boot) {
		try {
			LOG_DEBUG_STD(_T("Stopping: Message handling Plugins"));
			mainClient.unloadPlugins(true);
		} catch(NSPluginException e) {
			LOG_ERROR_CORE_STD(_T("Exception raised when unloading msg plugins: ") + e.error_ + _T(" in module: ") + e.file_);
		} catch(...) {
			LOG_ERROR_CORE_STD(_T("UNknown exception raised: When stopping message plguins"));
		}
	}
	LOG_MESSAGE_STD(_T("NSCLient++ - ") SZVERSION _T(" Stopped succcessfully"));
	return true;
}
#ifdef WIN32x
/**
 * Forward this to the main service dispatcher helper class
 * @param dwArgc 
 * @param *lpszArgv 
 */
void WINAPI NSClientT::service_main_dispatch(DWORD dwArgc, LPTSTR *lpszArgv) {
	try {
		//WTF!!! mainClient.service_main(dwArgc, lpszArgv);
	} catch (service_helper::service_exception e) {
		LOG_ERROR_STD(_T("Unknown service error: ") + e.what());
	} catch (...) {
		LOG_ERROR_STD(_T("Unknown service error!"));
	}
}
DWORD WINAPI NSClientT::service_ctrl_dispatch_ex(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext) {
	return mainClient.service_ctrl_ex(dwControl, dwEventType, lpEventData, lpContext);
}
/**
 * Forward this to the main service dispatcher helper class
 * @param dwCtrlCode 
 */
void WINAPI NSClientT::service_ctrl_dispatch(unsigned long dwCtrlCode) {
	mainClient.service_ctrl_ex(dwCtrlCode, NULL, NULL, NULL);
}
#endif

void NSClientT::service_on_session_changed(unsigned long dwSessionId, bool logon, unsigned long dwEventType) {
// 	if (shared_server_.get() == NULL) {
// 		LOG_DEBUG_STD(_T("No shared session: ignoring change event!"));
// 		return;
// 	}
	LOG_DEBUG_STD(_T("Got session change: ") + strEx::itos(dwSessionId));
	if (!logon) {
		LOG_DEBUG_STD(_T("Not a logon event: ") + strEx::itos(dwEventType));
		return;
	}
	tray_starter::start(dwSessionId);
}

//////////////////////////////////////////////////////////////////////////
// Member functions

int NSClientT::commandLineExec(const wchar_t* module, const unsigned int argLen, wchar_t** args) {
	std::wstring sModule = module;
	std::wstring moduleList = _T("");
	{
		boost::shared_lock<boost::shared_mutex> readLock(m_mutexRW, boost::get_system_time() + boost::posix_time::seconds(5));
		if (!readLock.owns_lock()) {
			LOG_ERROR_CORE(_T("FATAL ERROR: Could not get read-mutex."));
			return -1;
		}
		for (pluginList::size_type i=0;i<plugins_.size();++i) {
			plugin_type p = plugins_[i];
			if (!moduleList.empty())
				moduleList += _T(", ");
			if (p) {
				moduleList += p->getModule();
				if (p->getModule() == sModule) {
					LOG_DEBUG_STD(_T("Found module: ") + p->getName() + _T("..."));
					try {
						return p->commandLineExec(argLen, args);
					} catch (NSPluginException e) {
						LOG_ERROR_CORE_STD(_T("Could not execute command: ") + e.error_ + _T(" in ") + e.file_);
						return -1;
					}
				}
			}
		}
	}
	try {
		plugin_type plugin = loadPlugin(getBasePath() / boost::filesystem::wpath(_T("modules")) / boost::filesystem::wpath(module));
		LOG_DEBUG_STD(_T("Loading plugin: ") + plugin->getName() + _T("..."));
		plugin->load_plugin(NSCAPI::dontStart);
		return plugin->commandLineExec(argLen, args);
	} catch (NSPluginException e) {
		LOG_MESSAGE_STD(_T("Module (") + e.file_ + _T(") was not found: ") + e.error_);
	}
	LOG_ERROR_CORE_STD(_T("Module not found: ") + std::wstring(module) + _T(" available modules are: ") + moduleList);
	return 0;
}

/**
 * Load a list of plug-ins
 * @param plugins A list with plug-ins (DLL files) to load
 */
void NSClientT::addPlugins(const std::list<std::wstring> plugins) {
	boost::shared_lock<boost::shared_mutex> readLock(m_mutexRW, boost::get_system_time() + boost::posix_time::seconds(10));
	if (!readLock.owns_lock()) {
		LOG_ERROR_CORE(_T("FATAL ERROR: Could not get read-mutex."));
		return;
	}
	std::list<std::wstring>::const_iterator it;
	for (it = plugins.begin(); it != plugins.end(); ++it) {
		loadPlugin(*it);
	}
}
/**
 * Unload all plug-ins (in reversed order)
 */
void NSClientT::unloadPlugins(bool unloadLoggers) {
	{
		boost::unique_lock<boost::shared_mutex> writeLock(m_mutexRW, boost::get_system_time() + boost::posix_time::seconds(10));
		if (!writeLock.owns_lock()) {
			LOG_ERROR_CORE(_T("FATAL ERROR: Could not get read-mutex."));
			return;
		}
		commandHandlers_.clear();
		if (unloadLoggers)
			messageHandlers_.clear();
	}
	{
		boost::shared_lock<boost::shared_mutex> readLock(m_mutexRW, boost::get_system_time() + boost::posix_time::milliseconds(5000));
		if (!readLock.owns_lock()) {
			LOG_ERROR_CORE(_T("FATAL ERROR: Could not get read-mutex."));
			return;
		}
		for (pluginList::reverse_iterator it = plugins_.rbegin(); it != plugins_.rend(); ++it) {
			plugin_type p = *it;
			if (!p)
				continue;
			try {
				if (unloadLoggers || !p->hasMessageHandler()) {
					LOG_DEBUG_STD(_T("Unloading plugin: ") + p->getModule() + _T("..."));
					p->unload();
				} else {
					LOG_DEBUG_STD(_T("Skipping log plugin: ") + p->getModule() + _T("..."));
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
			LOG_ERROR_CORE(_T("FATAL ERROR: Could not get read-mutex."));
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
			LOG_ERROR_CORE(_T("FATAL ERROR: Could not get read-mutex."));
			return;
		}
		for (pluginList::iterator it=plugins_.begin(); it != plugins_.end();) {
			LOG_DEBUG_STD(_T("Loading plugin: ") + (*it)->getName() + _T("..."));
			try {
				if (!(*it)->load_plugin(NSCAPI::normalStart)) {
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
	/*
	for (pluginList::iterator it=plugins_.begin(); it != plugins_.end();) {
		LOG_DEBUG_STD(_T("Loading plugin: ") + (*it)->getName() + _T("..."));
		try {
			(*it)->load_plugin(mode);
			++it;
		} catch(NSPluginException e) {
			it = plugins_.erase(it);
			LOG_ERROR_CORE_STD(_T("Exception raised when loading plugin: ") + e.error_ + _T(" in module: ") + e.file_ + _T(" plugin has been removed."));
		} catch(...) {
			it = plugins_.erase(it);
			LOG_ERROR_CORE(_T("Unknown exception raised when unloading plugin plugin has been removed"));
		}
	}
	*/
	plugins_loaded_ = true;
}
/**
 * Load a single plug-in using a DLL filename
 * @param file The DLL file
 */
NSClientT::plugin_type NSClientT::loadPlugin(const boost::filesystem::wpath file) {
	plugin_type plugin(new NSCPlugin(file));
	return addPlugin(plugin);
}
/**
 * Load and add a plugin to various internal structures
 * @param plugin The plug-in instance to load. The pointer is managed by the 
 */
NSClientT::plugin_type NSClientT::addPlugin(plugin_type plugin) {
	plugin->load_dll();
	{
		boost::unique_lock<boost::shared_mutex> writeLock(m_mutexRW, boost::get_system_time() + boost::posix_time::seconds(10));
		if (!writeLock.owns_lock()) {
			LOG_ERROR_CORE(_T("FATAL ERROR: Could not get read-mutex."));
			return plugin;
		}
		plugins_.insert(plugins_.end(), plugin);
		if (plugin->hasCommandHandler())
			commandHandlers_.insert(commandHandlers_.end(), plugin);
		if (plugin->hasMessageHandler())
			messageHandlers_.insert(messageHandlers_.end(), plugin);
		settings_manager::get_core()->register_key(_T("/modules"), plugin->getModule(), Settings::SettingsCore::key_string, plugin->getName(), plugin->getDescription(), _T(""), false);
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

unsigned int NSClientT::getBufferLength() {
	static unsigned int len = 0;
	if (len == 0) {
		try {
			len = settings_manager::get_settings()->get_int(SETTINGS_KEY(settings_def::PAYLOAD_LEN));
		} catch (SettingsException &e) {
			LOG_DEBUG_STD(_T("Failed to get length: ") + e.getMessage());
			return setting_keys::settings_def::PAYLOAD_LEN_DEFAULT;
		} catch (...) {
			LOG_ERROR_CORE(_T("Failed to get length: :("));
			return setting_keys::settings_def::PAYLOAD_LEN_DEFAULT;
		}
	}
	return len;
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
		PluginCommand::RequestMessage message;
		PluginCommand::Header *hdr = message.mutable_header();
		hdr->set_type(PluginCommand::Header_Type_REQUEST);
		hdr->set_version(PluginCommand::Header_Version_VERSION_1);

		PluginCommand::Request *req = message.add_payload();
		req->set_command(to_string(command));
		req->set_version(PluginCommand::Request_Version_VERSION_1);

		std::string args = to_string(arguments);
		boost::tokenizer<boost::escaped_list_separator<char> > tok(args, boost::escaped_list_separator<char>('\\', ' ', '\"'));
		BOOST_FOREACH(string s, tok)
			req->add_arguments(s);

		std::string request, response;
		message.SerializeToString(&request);



		NSCAPI::nagiosReturn ret = injectRAW(command.c_str(), request, response);
		if (response.empty()) {
			LOG_ERROR(_T("No data retutned from command"));
			return NSCAPI::returnUNKNOWN;
		}

		PluginCommand::ResponseMessage rsp_msg;
		rsp_msg.ParseFromString(response);
		if (rsp_msg.payload_size() != 1) {
			LOG_ERROR_STD(_T("Failed to extract return message not 1 payload: ") + strEx::itos(rsp_msg.payload_size()));
			return NSCAPI::returnUNKNOWN;
		}
		msg = to_wstring(rsp_msg.payload(0).message());
		if ( (ret == NSCAPI::returnInvalidBufferLen) || (ret == NSCAPI::returnIgnored) ) {
			return ret;
		}
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
NSCAPI::nagiosReturn NSClientT::injectRAW(const wchar_t* command, std::string &request, std::string &response) {
	if (logDebug()) {
		LOG_DEBUG_STD(_T("Injecting: ") + std::wstring(command) + _T(": {{{") + strEx::strip_hex(to_wstring(request)) + _T("}}}"));
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
			LOG_ERROR_CORE(_T("FATAL ERROR: Could not get read-mutex."));
			return NSCAPI::returnUNKNOWN;
		}
		for (pluginList::size_type i = 0; i < commandHandlers_.size(); i++) {
			try {
				NSCAPI::nagiosReturn c = commandHandlers_[i]->handleCommand(command, request, response);
				switch (c) {
					case NSCAPI::returnInvalidBufferLen:
						LOG_ERROR_CORE(_T("UNKNOWN: Return buffer to small to handle this command."));
						return c;
					case NSCAPI::returnIgnored:
						break;
					case NSCAPI::returnOK:
					case NSCAPI::returnWARN:
					case NSCAPI::returnCRIT:
					case NSCAPI::returnUNKNOWN:
						LOG_DEBUG_STD(_T("Result ") + std::wstring(command) + _T(": ") + NSCHelper::translateReturn(c) + _T(" {{{") + strEx::strip_hex(to_wstring(response)) + _T("}}}"));
						return c;
					default:
						LOG_ERROR_CORE_STD(_T("Unknown error from handleCommand: ") + strEx::itos(c) + _T(" the injected command was: ") + (std::wstring)command);
						return c;
				}
			} catch(const NSPluginException& e) {
				LOG_ERROR_CORE_STD(_T("Exception raised: ") + e.error_ + _T(" in module: ") + e.file_);
				return NSCAPI::returnCRIT;
			} catch(...) {
				LOG_ERROR_CORE(_T("Unknown exception raised in module"));
				return NSCAPI::returnCRIT;
			}
		}
		LOG_MESSAGE_STD(_T("No handler for command: '") + command + _T("'"));
		return NSCAPI::returnIgnored;
	}
}

void NSClientT::listPlugins() {
	boost::shared_lock<boost::shared_mutex> readLock(m_mutexRW, boost::get_system_time() + boost::posix_time::milliseconds(5000));
	if (!readLock.owns_lock()) {
		LOG_ERROR_CORE(_T("FATAL ERROR: Could not get read-mutex."));
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
	if (debug_ == log_unknown) {
		debug_ = log_looking;
		try {
			if (settings_manager::get_settings()->get_int(_T("log"), _T("debug"), 0) == 1)
				debug_ = log_debug;
			else
				debug_ = log_nodebug;
		} catch (SettingsException e) {
			return true;
		}
	} else if (debug_ == log_looking) 
		return true;
	return (debug_ == log_debug);
}

void log_broken_message(std::wstring msg) {
#ifdef WIN32
	OutputDebugString(msg.c_str());
#else
	std::wcout << _T("--BROKEN MESSAGE: ") << msg << _T("--") << std::endl;
#endif
}
/**
 * Report a message to all logging enabled modules.
 *
 * @param msgType Message type 
 * @param file Filename generally __FILE__
 * @param line  Line number, generally __LINE__
 * @param message The message as a human readable string.
 */
void NSClientT::reportMessage(int msgType, const wchar_t* file, const int line, std::wstring message) {
	try {
		strEx::replace(message, _T("\n"), _T(" "));
		strEx::replace(message, _T("\r"), _T(" "));
		if ((msgType == NSCAPI::debug)&&(!logDebug())) {
			return;
		}
// 		if (shared_server_.get() != NULL && shared_server_->hasClients()) {
// 			try {
// 				shared_server_->sendLogMessageToClients(msgType, file, line, message);
// 			} catch (nsclient_session::session_exception e) {
// 				log_broken_message(_T("Failed to send message to clients: ") + e.what());
// 			}
// 		}
		std::wstring file_stl = file;
		std::wstring::size_type pos = file_stl.find_last_of(_T("\\"));
		if (pos != std::wstring::npos)
			file_stl = file_stl.substr(pos);
		{
			boost::shared_lock<boost::shared_mutex> readLock(m_mutexRW, boost::get_system_time() + boost::posix_time::milliseconds(5000));
			if (!readLock.owns_lock()) {
				log_broken_message(_T("Message was lost as the (mutexRW) core was locked: ") + message);
				return;
			}
			boost::unique_lock<boost::timed_mutex> lock(messageMutex, boost::get_system_time() + boost::posix_time::seconds(5));
			if (!lock.owns_lock()) {
				log_broken_message(_T("Message was lost as the core was locked: ") + message);
				return;
			}
			if (g_bConsoleLog) {
				std::wstring k = _T("?");
				switch (msgType) {
				case NSCAPI::critical:
					k =_T("c");
					break;
				case NSCAPI::warning:
					k =_T("w");
					break;
				case NSCAPI::error:
					k =_T("e");
					break;
				case NSCAPI::log:
					k =_T("l");
					break;
				case NSCAPI::debug:
					k =_T("d");
					break;
				default:
					k =_T("?");
				}
				std::wcout << k << _T(" ") << file_stl << _T("(") << line << _T(") ") << message << std::endl;
			}
			if (!plugins_loaded_) {
				log_broken_message(message);
				cached_log_entry entry(msgType, file, line, message);
				log_cache_.push_back(entry);
			} else {
				if (log_cache_.size() > 0) {
					for (log_cache_type::const_iterator cit=log_cache_.begin();cit!=log_cache_.end();++cit) {
						for (pluginList::size_type i = 0; i< messageHandlers_.size(); i++) {
							try {
								messageHandlers_[i]->handleMessage((*cit).msgType, (_T("CACHE") + (*cit).file).c_str(), (*cit).line, (*cit).message.c_str());
							} catch(const NSPluginException& e) {
								log_broken_message(_T("Caught: ") + e.error_ + _T(" when trying to log a message..."));
								return;
							} catch(...) {
								log_broken_message(_T("Caught: Unknown Exception when trying to log a message..."));
								return;
							}
						}
					}
					log_cache_.clear();
				}
				for (pluginList::size_type i = 0; i< messageHandlers_.size(); i++) {
					try {
						messageHandlers_[i]->handleMessage(msgType, file, line, message.c_str());
					} catch(const NSPluginException& e) {
						log_broken_message(_T("Caught: ") + e.error_ + _T(" when trying to log a message..."));
						return;
					} catch(...) {
						log_broken_message(_T("Caught: Unknown Exception when trying to log a message..."));
						return;
					}
				}
			}
		}
	} catch (...) {
		log_broken_message(_T("Caught UNKNOWN Exception when trying to log a message: ") + message);
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
	} catch (SettingsException e) {
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

void NSClientT::nsclient_log_error(std::wstring file, int line, std::wstring error) {
	reportMessage(NSCAPI::error, file.c_str(), line, error);
}



// Service API
NSClient* NSClientT::get_global_instance() {
	return &mainClient;
}
void NSClientT::handle_startup() {
	initCore(true);
/*
	DWORD dwSessionId = remote_processes::getActiveSessionId();
	if (dwSessionId != 0xFFFFFFFF)
		tray_starter::start(dwSessionId);
	else
		LOG_ERROR_STD(_T("Failed to start tray helper:" ) + error::lookup::last_error());
		*/
}
void NSClientT::handle_shutdown() {
	exitCore(true);
}
#ifdef _WIN32
void NSClientT::handle_session_change(unsigned long dwSessionId, bool logon) {

}
#endif
