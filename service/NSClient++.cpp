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
#include <remote_processes.hpp>
//#include <winsvc.h>
//#include <Userenv.h>
//#include <Lmcons.h>
#include <remote_processes.hpp>
#include "core_api.h"
#include "settings_manager_impl.h"
#include <settings/macros.h>
#include <NSCHelper.h>

NSClient mainClient(SZSERVICENAME);	// Global core instance.
bool g_bConsoleLog = false;

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
		return 0;
	}

	static bool start(DWORD dwSessionId) {
		std::wstring program = mainClient.getBasePath() +  _T("\\") + 
			SETTINGS_GET_STRING_CORE(settings_def::SYSTRAY_EXE);
		std::wstring cmdln = _T("\"") + program + _T("\" -channel __") + strEx::itos(dwSessionId) + _T("__");
		return tray_starter::startTrayHelper(dwSessionId, program, cmdln);
	}

	static bool startTrayHelper(DWORD dwSessionId, std::wstring exe, std::wstring cmdline, bool startThread = true) {
		HANDLE hToken = NULL;
		if (!remote_processes::GetSessionUserToken(dwSessionId, &hToken)) {
			LOG_ERROR_STD(_T("Failed to query user token: ") + error::lookup::last_error());
			return false;
		} else {
			STARTUPINFO          StartUPInfo;
			PROCESS_INFORMATION  ProcessInfo;

			ZeroMemory(&StartUPInfo,sizeof(STARTUPINFO));
			ZeroMemory(&ProcessInfo,sizeof(PROCESS_INFORMATION));
			StartUPInfo.wShowWindow = SW_HIDE;
			StartUPInfo.lpDesktop = L"Winsta0\\Default";
			StartUPInfo.cb = sizeof(STARTUPINFO);

			wchar_t *buffer = new wchar_t[cmdline.size()+10];
			wcscpy(buffer, cmdline.c_str());
			LOG_MESSAGE_STD(_T("Running: ") + exe);
			LOG_MESSAGE_STD(_T("Running: ") + cmdline);

			LPVOID pEnv =NULL;
			DWORD dwCreationFlags = CREATE_NO_WINDOW; //0; //DETACHED_PROCESS

			if(CreateEnvironmentBlock(&pEnv,hToken,TRUE)) {
				dwCreationFlags|=CREATE_UNICODE_ENVIRONMENT;
			} else {
				LOG_ERROR_STD(_T("Failed to create enviornment: ") + error::lookup::last_error());
				pEnv=NULL;
			}
			/*
			LOG_ERROR_STD(_T("Impersonating user: "));
			if (!ImpersonateLoggedOnUser(hToken)) {
				LOG_ERROR_STD(_T("Failed to impersonate the user: ") + error::lookup::last_error());
			}

			wchar_t pszUname[UNLEN + 1];
			ZeroMemory(pszUname,sizeof(pszUname));
			DWORD dwSize = UNLEN;
			if (!GetUserName(pszUname,&dwSize)) {
				DWORD dwErr = GetLastError();
				if (!RevertToSelf())
					LOG_ERROR_STD(_T("Failed to revert to self: ") + error::lookup::last_error());
				LOG_ERROR_STD(_T("Failed to get username: ") + error::format::from_system(dwErr));
				return false;
			}
			

			PROFILEINFO info;
			info.dwSize = sizeof(PROFILEINFO);
			info.lpUserName = pszUname;
			if (!LoadUserProfile(hToken, &info)) {
				DWORD dwErr = GetLastError();
				if (!RevertToSelf())
					LOG_ERROR_STD(_T("Failed to revert to self: ") + error::lookup::last_error());
				LOG_ERROR_STD(_T("Failed to get username: ") + error::format::from_system(dwErr));
				return false;
			}
			*/
			if (!CreateProcessAsUser(hToken, exe.c_str(), buffer, NULL, NULL, FALSE, dwCreationFlags, pEnv, NULL, &StartUPInfo, &ProcessInfo)) {
				DWORD dwErr = GetLastError();
				delete [] buffer;
				/*
				if (!RevertToSelf()) {
					LOG_ERROR_STD(_T("Failed to revert to self: ") + error::lookup::last_error());
				}
				*/
				if (startThread && dwErr == ERROR_PIPE_NOT_CONNECTED) {
					LOG_MESSAGE(_T("Failed to start trayhelper: starting a background thread to do it instead..."));
					Thread<tray_starter> *pThread = new Thread<tray_starter>(_T("tray-starter-thread"));
					pThread->createThread(tray_starter::init(dwSessionId, exe, cmdline));
					return false;
				} else if (dwErr == ERROR_PIPE_NOT_CONNECTED) {
					LOG_ERROR_STD(_T("Thread failed to start trayhelper (will try again): ") + error::format::from_system(dwErr));
					return false;
				} else {
					LOG_ERROR_STD(_T("Failed to start trayhelper: ") + error::format::from_system(dwErr));
					return true;
				}
			} else {
				delete [] buffer;
				/*
				if (!RevertToSelf()) {
					LOG_ERROR_STD(_T("Failed to revert to self: ") + error::lookup::last_error());
				}
				*/
				LOG_MESSAGE_STD(_T("Started tray in other user session: ") + strEx::itos(dwSessionId));
			}


			CloseHandle(hToken);
			return true;
		}
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
	::MessageBox(NULL, message.c_str(), title.c_str(), MB_OK|MB_ICONERROR);
}


/**
 * Application startup point
 *
 * @param argc Argument count
 * @param argv[] Argument array
 * @param envp[] Environment array
 * @return exit status
 */
int wmain(int argc, TCHAR* argv[], TCHAR* envp[])
{
	srand( (unsigned)time( NULL ) );
	int nRetCode = 0;
	if ( (argc > 1) && ((*argv[1] == '-') || (*argv[1] == '/')) ) {
		if (false) {
#ifdef WIN32
		} if ( _wcsicmp( _T("install"), argv[1]+1 ) == 0 ) {
			bool bGui = false;
			bool bStart = false;
			std::wstring service_name, service_description;
			for (int i=2;i<argc;i++) {
				if (_wcsicmp( _T("gui"), argv[i]) == 0) {
					bGui = true;
				} else if (_wcsicmp( _T("start"), argv[i]) == 0) {
					bStart = true;
				} else {
					if (service_name.empty())
						service_name = argv[i];
					else {
						if (!service_description.empty())
							service_description += _T(" ");
						service_description += argv[i];
					}
				}
			}
			if (service_name.empty())
				service_name = SZSERVICENAME;
			if (service_description.empty())
				service_description = SZSERVICEDISPLAYNAME;
			g_bConsoleLog = true;
			try {
				serviceControll::Install(service_name.c_str(), service_description.c_str(), SZDEPENDENCIES);
				if (bStart)
					serviceControll::Start(service_name);
			} catch (const serviceControll::SCException& e) {
				if (bGui)
					display(_T("Error installing"), _T("Service installation failed; ") + e.error_);
				LOG_ERROR_STD(_T("Service installation failed: ") + e.error_);
				return -1;
			}
			try {
				serviceControll::SetDescription(service_name, service_description);
			} catch (const serviceControll::SCException& e) {
				if (bGui)
					display(_T("Error installing"), _T("Service installation failed; ") + e.error_);
				LOG_MESSAGE_STD(_T("Couldn't set service description: ") + e.error_);
			}
			if (bGui)
				display(_T("Service installed"), _T("Service installed successfully!"));
			LOG_MESSAGE(_T("Service installed!"));
			return 0;
		} else if ( _wcsicmp( _T("uninstall"), argv[1]+1 ) == 0 ) {
			bool bGui = false;
			bool bStop = false;
			std::wstring service_name;
			for (int i=2;i<argc;i++) {
				if (_wcsicmp( _T("gui"), argv[i]) == 0) {
					bGui = true;
				} else if (_wcsicmp( _T("stop"), argv[i]) == 0) {
					bStop = true;
				} else {
					service_name = argv[i];
				}
			}
			if (service_name.empty())
				service_name = SZSERVICENAME;
			g_bConsoleLog = true;
			try {
				if (bStop)
					serviceControll::Stop(service_name);
			} catch (const serviceControll::SCException& e) {
				LOG_MESSAGE_STD(_T("Failed to stop service (") + service_name + _T(") failed; ") + e.error_);
			}
			try {
				serviceControll::Uninstall(service_name);
			} catch (const serviceControll::SCException& e) {
				if (bGui)
					display(_T("Error uninstalling"), _T("Service de-installation (") + service_name + _T(") failed; ") + e.error_ + _T("\nMaybe the service was not previously installed properly?"));
				LOG_ERROR_STD(_T("Service deinstallation failed; ") + e.error_);
				return 0;
			}
			if (bGui)
				display(_T("Service uninstalled"), _T("Service uninstalled successfully!"));
			LOG_MESSAGE(_T("Service uninstalled!"));
			return 0;
		} else if ( _wcsicmp( _T("start"), argv[1]+1 ) == 0 ) {
			g_bConsoleLog = true;
			bool bGui = false;
			std::wstring service_name;
			for (int i=2;i<argc;i++) {
				if (_wcsicmp( _T("gui"), argv[i]) == 0) {
					bGui = true;
				} else {
					service_name = argv[i];
				}
			}
			if (service_name.empty())
				service_name = SZSERVICENAME;
			try {
				serviceControll::Start(service_name.c_str());
			} catch (const serviceControll::SCException& e) {
				if (bGui)
					display(_T("Service failed to start"), e.error_);
				LOG_MESSAGE_STD(_T("Service failed to start: ") + e.error_);
				return -1;
			}
		} else if ( _wcsicmp( _T("stop"), argv[1]+1 ) == 0 ) {
			g_bConsoleLog = true;
			bool bGui = false;
			std::wstring service_name;
			for (int i=2;i<argc;i++) {
				if (_wcsicmp( _T("gui"), argv[i]) == 0) {
					bGui = true;
				} else {
					service_name = argv[i];
				}
			}
			if (service_name.empty())
				service_name = SZSERVICENAME;
			try {
				serviceControll::Stop(service_name.c_str());
			} catch (const serviceControll::SCException& e) {
				if (bGui)
					display(_T("Service failed to stop"), e.error_);
				LOG_MESSAGE_STD(_T("Service failed to stop: ") + e.error_);
				return -1;
			}
		} else if ( _wcsicmp( _T("svc"), argv[1]+1 ) == 0 ) {
			g_bConsoleLog = true;
			try {
				std::wstring exe = serviceControll::get_exe_path(SZSERVICENAME);
				LOG_MESSAGE_STD(_T("The Service uses: ") + exe);
			} catch (const serviceControll::SCException& e) {
				LOG_ERROR_STD(_T("Failed to find service: ") + e.error_);
			}
#endif
		} else if ( _wcsicmp( _T("encrypt"), argv[1]+1 ) == 0 ) {
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
		} else if ( _wcsicmp( _T("about"), argv[1]+1 ) == 0 ) {
			g_bConsoleLog = true;
			LOG_MESSAGE(SZAPPNAME _T(" (C) Michael Medin - michael<at>medin<dot>name"));
			LOG_MESSAGE(_T("Version: ") SZVERSION);
			LOG_MESSAGE(_T("Architecture: ") SZARCH);

			std::wstring pluginPath = mainClient.getBasePath() + _T("modules\\");
			LOG_MESSAGE_STD(_T("Looking at plugins in: ") + pluginPath);

			WIN32_FIND_DATA wfd;
			HANDLE hFind = FindFirstFile((pluginPath + _T("*.dll")).c_str(), &wfd);
			if (hFind != INVALID_HANDLE_VALUE) {
				do {
					std::wstring file = wfd.cFileName;
					NSCPlugin *plugin = new NSCPlugin(pluginPath + _T("\\") + file);
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
				} while (FindNextFile(hFind, &wfd));
			} else {
				LOG_CRITICAL(_T("No plugin was found!"));
			}
			FindClose(hFind);
		} else if ( _wcsicmp( _T("version"), argv[1]+1 ) == 0 ) {
			g_bConsoleLog = true;
			LOG_MESSAGE(SZAPPNAME _T(" Version: ") SZVERSION _T(", Plattform: ") SZARCH);
		} else if ( _wcsicmp( _T("d"), argv[1]+1 ) == 0 ) {
			// Run command from command line (like NRPE) but with debug enabled
		} else if ( _wcsicmp( _T("noboot"), argv[1]+1 ) == 0 ) {
			g_bConsoleLog = true;
			mainClient.enableDebug(false);
			mainClient.initCore(false);
			if (argc>=4)
				nRetCode = mainClient.commandLineExec(argv[2], argv[3], argc-4, &argv[4]);
			else
				nRetCode = mainClient.commandLineExec(argv[2], argv[3], 0, NULL);
			mainClient.exitCore(true);
			return nRetCode;
		} else if ( _wcsicmp( _T("c"), argv[1]+1 ) == 0 ) {
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
			nRetCode = mainClient.inject(command, args, L' ', true, msg, perf);
			std::wcout << msg << _T("|") << perf << std::endl;
			mainClient.exitCore(true);
			return nRetCode;
		} else if ( _wcsicmp( _T("test"), argv[1]+1 ) == 0 ) {
			bool server = false;
			if (argc > 2 && _wcsicmp( _T("server"), argv[2] ) == 0 ) {
				server = true;
			}
			std::wcout << "Launching test mode - " << (server?_T("server mode"):_T("client mode")) << std::endl;
			LOG_MESSAGE_STD(_T("Booting: " SZSERVICEDISPLAYNAME ));
#ifdef WIN32
			try {
				if (serviceControll::isStarted(SZSERVICENAME)) {
					std::wcerr << "Service seems to be started, this is probably not a good idea..." << std::endl;
				}
			} catch (const serviceControll::SCException& e) {
				e;// Empty by design
			}
#endif
			g_bConsoleLog = true;
			mainClient.enableDebug(true);
			if (!mainClient.initCore(true)) {
				LOG_ERROR_STD(_T("Service *NOT* started!"));
				return -1;
			}
			LOG_MESSAGE_STD(_T("Using settings from: ") + settings_manager::get_core()->get_settings_type_desc());
			LOG_MESSAGE(_T("Enter command to inject or exit to terminate..."));
/*
			Settings::get_settings()->clear_cache();
			LOG_MESSAGE_STD( _T("test 001: ") + SETTINGS_GET_STRING(NSCLIENT_TEST1) );
			LOG_MESSAGE_STD( _T("test 002: ") + SETTINGS_GET_STRING(NSCLIENT_TEST2) );
			LOG_MESSAGE_STD( _T("test 003: ") + SETTINGS_GET_STRING(NSCLIENT_TEST3) );
			LOG_MESSAGE_STD( _T("test 004: ") + SETTINGS_GET_STRING(NSCLIENT_TEST4) );

			Settings::get_settings()->save_to(_T("test.ini"));
*/
			std::wstring s = _T("");
			std::wstring buff = _T("");
			while (true) {
				std::wcin >> s;
				if (s == _T("exit")) {
					std::wcout << _T("Exiting...") << std::endl;
					break;
				} else if (s == _T("plugins")) {
					std::wcout << _T("Listing plugins...") << std::endl;
					mainClient.listPlugins();
				} else if (s == _T("off") && buff == _T("debug ")) {
					std::wcout << _T("Setting debug log off...") << std::endl;
					mainClient.enableDebug(false);
				} else if (s == _T("on") && buff == _T("debug ")) {
					std::wcout << _T("Setting debug log on...") << std::endl;
					mainClient.enableDebug(true);
				} else if (s == _T("reattach")) {
					std::wcout << _T("Reattaching to session 0") << std::endl;
					mainClient.startTrayIcon(0);
//#ifdef DEBUG
				} else if (s == _T("assert")) {
					throw "test";
//#endif
				} else if (std::cin.peek() < 15) {
					buff += s;
					strEx::token t = strEx::getToken(buff, ' ');
					std::wstring msg, perf;
					NSCAPI::nagiosReturn ret = mainClient.inject(t.first, t.second, ' ', true, msg, perf);
					if (ret == NSCAPI::returnIgnored) {
						std::wcout << _T("No handler for command: ") << t.first << std::endl;
					} else {
						std::wcout << NSCHelper::translateReturn(ret) << _T(":");
						std::cout << strEx::wstring_to_string(msg);
						if (!perf.empty())
							std::cout << "|" << strEx::wstring_to_string(perf);
						std::wcout << std::endl;
					}
					buff = _T("");
				} else {
					buff += s + _T(" ");
				}
			}
			mainClient.exitCore(true);
			return 0;
		} else {
			std::wcerr << _T("Usage: -version, -about, -install, -uninstall, -start, -stop, -encrypt -settings") << std::endl;
			std::wcerr << _T("Usage: [-noboot] <ModuleName> <commnd> [arguments]") << std::endl;
			return -1;
		}
		return nRetCode;
	} else if (argc > 2) {
		g_bConsoleLog = true;
		mainClient.initCore(true);
		if (argc>=3)
			nRetCode = mainClient.commandLineExec(argv[1], argv[2], argc-3, &argv[3]);
		else
			nRetCode = mainClient.commandLineExec(argv[1], argv[2], 0, NULL);
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
	std::wcout << _T("Running as service...") << std::endl;
	if (!mainClient.StartServiceCtrlDispatcher()) {
		LOG_MESSAGE(_T("We failed to start the service"));
	}
	return nRetCode;
}

void migrate() {}

NSClientT::plugin_info_list NSClientT::get_all_plugins() {
	plugin_info_list ret;
	std::wstring modPath = getBasePath() + _T("modules\\");

	WIN32_FIND_DATA wfd;
	HANDLE hFind = FindFirstFile((modPath + _T("*.dll")).c_str(), &wfd);
	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			plugin_info_type info;
			info.dll = wfd.cFileName;
			try {
				LOG_DEBUG_STD(_T("Attempting to fake load: ") + wfd.cFileName);
				NSCPlugin plugin(modPath + wfd.cFileName);
				plugin.load_dll();
				plugin.load_plugin(NSCAPI::dontStart);
				info.name = plugin.getName();
				info.description = plugin.getDescription();
				plugin.unload();
			} catch (NSPluginException e) {
				LOG_CRITICAL_STD(_T("Error loading: ") + e.file_ + _T(" root cause: ") + e.error_);
			} catch (...) {
				LOG_CRITICAL_STD(_T("Unknown Error loading: ") + wfd.cFileName);
			}
			ret.push_back(info);
		} while (FindNextFile(hFind, &wfd));
	}
	FindClose(hFind);
	return ret;
}


void NSClientT::load_all_plugins(int mode) {
	std::wstring modPath = getBasePath() + _T("modules\\");

	WIN32_FIND_DATA wfd;
	HANDLE hFind = FindFirstFile((modPath + _T("*.dll")).c_str(), &wfd);
	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			if (settings_manager::get_settings()->has_key(MAIN_MODULES_SECTION, wfd.cFileName)) {
				if (settings_manager::get_settings()->get_string(MAIN_MODULES_SECTION, wfd.cFileName) == _T("disabled")) {
					try {
						LOG_DEBUG_STD(_T("Attempting to fake load: ") + wfd.cFileName);
						NSCPlugin plugin(modPath + wfd.cFileName);
						plugin.load_dll();
						plugin.load_plugin(mode);
						plugin.unload();
					} catch (NSPluginException e) {
						LOG_CRITICAL_STD(_T("Error loading: ") + e.file_ + _T(" root cause: ") + e.error_);
					} catch (...) {
						LOG_CRITICAL_STD(_T("Unknown Error loading: ") + wfd.cFileName);
					}
				}
			} else {
				std::wstring desc;
				try {
					NSCPlugin plugin(modPath + wfd.cFileName);
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
					LOG_CRITICAL_STD(_T("Unknown Error loading: ") + wfd.cFileName);
				}
				settings_manager::get_core()->register_key(MAIN_MODULES_SECTION, wfd.cFileName, Settings::SettingsCore::key_string, desc, desc, _T("disabled"), false);
			}
		} while (FindNextFile(hFind, &wfd));
	}
	FindClose(hFind);
}

void NSClientT::HandleSettingsCLI(TCHAR* arg, int argc, TCHAR* argv[]) {
	std::wstring sarg = arg;
	try {
		if (sarg == _T("migrate")) {
			if (argc == 0) {
				LOG_ERROR_STD(_T("In correct syntax: nsclient++ -settings migrate <to>"));
				return;
			}
			std::wstring to = argv[0];
			LOG_DEBUG_STD(_T("Migrating to: ") + to);
			try {
				settings_manager::get_core()->migrate_to(Settings::SettingsCore::string_to_type(to));
			} catch (SettingsException e) {
				LOG_CRITICAL_STD(_T("Failed to migrate settings: ") + e.getError());
			}
		} else if (sarg == _T("generate")) {
			if (argc == 0) {
				LOG_ERROR_STD(_T("In correct syntax: nsclient++ -settings generate <what>"));
				LOG_ERROR_STD(_T("     where <what> is one of ths following:"));
				LOG_ERROR_STD(_T("      trac"));
				LOG_ERROR_STD(_T("      default"));
				LOG_ERROR_STD(_T("      <type>"));
				return;
			}
			std::wstring arg1 = argv[0];
			if (arg1 == _T("default")) {
				try {
					load_all_plugins(NSCAPI::dontStart);
					settings_manager::get_core()->update_defaults();
					settings_manager::get_core()->get()->save();
				} catch (SettingsException e) {
					LOG_CRITICAL_STD(_T("Failed to migrate settings: ") + e.getError());
				}
			} else if (arg1 == _T("trac")) {
				try {
					load_all_plugins(NSCAPI::dontStart);

					Settings::string_list s = settings_manager::get_core()->get_reg_sections();
					for (Settings::string_list::const_iterator cit = s.begin(); cit != s.end(); ++cit) {
						std::wcout << _T("== ") << (*cit) << _T(" ==") << std::endl;
						Settings::string_list k = settings_manager::get_core()->get_reg_keys(*cit);
						bool first = true;
						for (Settings::string_list::const_iterator citk = k.begin(); citk != k.end(); ++citk) {
							Settings::SettingsCore::key_description desc = settings_manager::get_core()->get_registred_key(*cit, *citk);
							if (!desc.advanced) {
								if (first)
									std::wcout << _T("'''Normal settings'''") << std::endl;
								first = false;
								std::wcout << _T("||") << (*citk) << _T("||") << desc.defValue << _T("||") << desc.title << _T(": ") << desc.description
									<< std::endl;
							}
						}
						first = true;
						for (Settings::string_list::const_iterator citk = k.begin(); citk != k.end(); ++citk) {
							Settings::SettingsCore::key_description desc = settings_manager::get_core()->get_registred_key(*cit, *citk);
							if (desc.advanced) {
								if (first)
									std::wcout << _T("'''Advanced settings'''") << std::endl;
								first = false;
								std::wcout << _T("||") << (*citk) << _T("||") << desc.defValue << _T("||") << desc.title << _T(": ") << desc.description
									<< std::endl;
							}
						}
					}
				} catch (SettingsException e) {
					LOG_CRITICAL_STD(_T("Failed to migrate settings: ") + e.getError());
				}
			} else {
				try {
					Settings::SettingsCore::settings_type type = settings_manager::get_core()->string_to_type(arg1);
					load_all_plugins(NSCAPI::dontStart);
					settings_manager::get_core()->update_defaults();
					settings_manager::get_core()->get(type)->save();
				} catch (SettingsException e) {
					LOG_CRITICAL_STD(_T("Failed to migrate settings: ") + e.getError());
				}
			}
		} else {
			LOG_ERROR_STD(_T("In correct syntax: nsclient++ -settings <keyword>"));
			LOG_ERROR_STD(_T(" <keyword> : "));
			LOG_ERROR_STD(_T("   migrate - migrate to a new setings subsystem"));
			LOG_ERROR_STD(_T("   copy    - copy settings from one subsystem to another"));
			LOG_ERROR_STD(_T("   set     - Set a setting system as the default store"));
		}


	} catch (SettingsException e) {
		LOG_CRITICAL_STD(_T("Failed to initialize settings: ") + e.getError());
	} catch (...) {
		LOG_CRITICAL(_T("FATAL ERROR IN SETTINGS SUBSYTEM"));
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
	LOG_MESSAGE(_T("Attempting to start NSCLient++ - " SZVERSION));
	if (!settings_manager::init_settings(getBasePath())) {
		return false;
	}
	try {
		if (debug_)
			settings_manager::get_settings()->set_int(_T("log"), _T("debug"), 1);
			settings_manager::get_settings()->set_int(_T("Settings"), _T("shared_Session"), 1);
		enable_shared_session_ = SETTINGS_GET_BOOL_CORE(settings_def::SHARED_SESSION);
	} catch (SettingsException e) {
		LOG_ERROR_STD(_T("Could not find settings: ") + e.getMessage());
	}

	if (enable_shared_session_) {
		LOG_MESSAGE_STD(_T("Enabling shared session..."));
		if (boot) {
			LOG_MESSAGE_STD(_T("Starting shared session..."));
			try {
				shared_server_.reset(new nsclient_session::shared_server_session(this));
				if (!shared_server_->session_exists()) {
					shared_server_->create_new_session();
				} else {
					LOG_ERROR_STD(_T("Session already exists cant create a new one!"));
				}
				startTrayIcons();
			} catch (nsclient_session::session_exception e) {
				LOG_ERROR_STD(_T("Failed to create new session: ") + e.what());
				shared_server_.reset(NULL);
			} catch (...) {
				LOG_ERROR_STD(_T("Failed to create new session: Unknown exception"));
				shared_server_.reset(NULL);
			}
		} else {
			LOG_MESSAGE_STD(_T("Attaching to shared session..."));
			try {
				std::wstring id = _T("_attached_") + strEx::itos(GetCurrentProcessId()) + _T("_");
				shared_client_.reset(new nsclient_session::shared_client_session(id, this));
				if (shared_client_->session_exists()) {
					shared_client_->attach_to_session(id);
				} else {
					LOG_ERROR_STD(_T("No session was found cant attach!"));
				}
				LOG_ERROR_STD(_T("Session is: ") + shared_client_->get_client_id());
			} catch (nsclient_session::session_exception e) {
				LOG_ERROR_STD(_T("Failed to attach to session: ") + e.what());
				shared_client_.reset(NULL);
			} catch (...) {
				LOG_ERROR_STD(_T("Failed to attach to session: Unknown exception"));
				shared_client_.reset(NULL);
			}
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
	try {
		com_helper_.initialize();
	} catch (com_helper::com_exception e) {
		LOG_ERROR_STD(_T("COM exception: ") + e.getMessage());
		return false;
	} catch (...) {
		LOG_ERROR_STD(_T("Unknown exception iniating COM..."));
		return false;
	}
	if (boot) {
		try {
			Settings::string_list list = settings_manager::get_settings()->get_keys(MAIN_MODULES_SECTION);
			for (Settings::string_list::const_iterator cit = list.begin(); cit != list.end(); ++cit) {
				LOG_DEBUG_STD(_T("Processing plugin: " + *cit));
				try {
					if (settings_manager::get_settings()->get_string(MAIN_MODULES_SECTION, *cit) == _T("disabled")) {
						LOG_DEBUG_STD(_T("Not booting: " + *cit + _T(" since it is disabled.")));
						continue;
					}
				} catch (...) {
					// If we except we load the plugin in as-is
				}
				try {
					loadPlugin(getBasePath() + _T("modules\\") + (*cit));
				} catch(const NSPluginException& e) {
					LOG_ERROR_STD(_T("Exception raised: ") + e.error_ + _T(" in module: ") + e.file_);
					//return false;
				} catch (std::exception e) {
					LOG_ERROR_STD(_T("exception loading plugin: ") + (*cit) + strEx::string_to_wstring(e.what()));
					return false;
				} catch (...) {
					LOG_ERROR_STD(_T("Unknown exception loading plugin: ") + (*cit));
					return false;
				}
			}
		} catch (SettingsException e) {
			LOG_ERROR_STD(_T("Failed to set settings file") + e.getMessage());
		} catch (...) {
			LOG_ERROR_STD(_T("Unknown exception when loading plugins"));
			return false;
		}
		try {
			loadPlugins(boot?NSCAPI::normalStart:NSCAPI::dontStart);
		} catch (...) {
			LOG_ERROR_STD(_T("Unknown exception loading plugins"));
			return false;
		}
		LOG_DEBUG_STD(_T("NSCLient++ - " SZVERSION) + _T(" Started!"));
	}
	LOG_MESSAGE_STD(_T("NSCLient++ - " SZVERSION) + _T(" Started!"));
	return true;
}

/**
 * Service control handler startup point.
 * When the program is started as a service this will be the entry point.
 */
bool NSClientT::InitiateService() {
	if (!initCore(true))
		return false;
/*
	DWORD dwSessionId = remote_processes::getActiveSessionId();
	if (dwSessionId != 0xFFFFFFFF)
		tray_starter::start(dwSessionId);
	else
		LOG_ERROR_STD(_T("Failed to start tray helper:" ) + error::lookup::last_error());
		*/
	return true;
}

void NSClientT::startTrayIcons() {
	if (shared_server_.get() == NULL) {
		LOG_MESSAGE_STD(_T("No master session so tray icons not started"));
		return;
	}
	remote_processes::PWTS_SESSION_INFO list;
	DWORD count;
	if (!remote_processes::_WTSEnumerateSessions(WTS_CURRENT_SERVER_HANDLE , 0, 1, &list, &count)) {
		LOG_ERROR_STD(_T("Failed to enumerate sessions:" ) + error::lookup::last_error());
	} else {
		LOG_DEBUG_STD(_T("Found ") + strEx::itos(count) + _T(" sessions"));
		for (DWORD i=0;i<count;i++) {
			LOG_DEBUG_STD(_T("Found session: ") + strEx::itos(list[i].SessionId) + _T(" state: ") + strEx::itos(list[i].State));
			if (list[i].State == remote_processes::_WTS_CONNECTSTATE_CLASS::WTSActive) {
				startTrayIcon(list[i].SessionId);
			}
		}
	}
}
void NSClientT::startTrayIcon(DWORD dwSessionId) {
	if (shared_server_.get() == NULL) {
		LOG_MESSAGE_STD(_T("No master session so tray icons not started"));
		return;
	}
	if (!shared_server_->re_attach_client(dwSessionId)) {
		if (!tray_starter::start(dwSessionId)) {
			LOG_ERROR_STD(_T("Failed to start session (") + strEx::itos(dwSessionId) + _T("): " ) + error::lookup::last_error());
		}
	}
}

bool NSClientT::exitCore(bool boot) {
	plugins_loaded_ = false;
	LOG_DEBUG(_T("Attempting to stop NSCLient++ - " SZVERSION));
	if (boot) {
		try {
			LOG_DEBUG_STD(_T("Stopping: NON Message Handling Plugins"));
			mainClient.unloadPlugins(false);
		} catch(NSPluginException e) {
			LOG_ERROR_STD(_T("Exception raised when unloading non msg plguins: ") + e.error_ + _T(" in module: ") + e.file_);
		} catch(...) {
			LOG_ERROR_STD(_T("Unknown exception raised when unloading non msg plugins"));
		}
	}
	LOG_DEBUG_STD(_T("Stopping: COM helper"));
	try {
		com_helper_.unInitialize();
	} catch (com_helper::com_exception e) {
		LOG_ERROR_STD(_T("COM exception: ") + e.getMessage());
	} catch (...) {
		LOG_ERROR_STD(_T("Unknown exception uniniating COM..."));
	}
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
	try {
		if (shared_client_.get() != NULL) {
			LOG_DEBUG_STD(_T("Stopping: shared client"));
			shared_client_->set_handler(NULL);
			shared_client_->close_session();
		}
	} catch(nsclient_session::session_exception &e) {
		LOG_ERROR_STD(_T("Exception closing shared client session: ") + e.what());
	} catch(...) {
		LOG_ERROR_STD(_T("Exception closing shared client session: Unknown exception!"));
	}
	try {
		if (shared_server_.get() != NULL) {
			LOG_DEBUG_STD(_T("Stopping: shared server"));
			shared_server_->set_handler(NULL);
			shared_server_->close_session();
		}
	} catch(...) {
		LOG_ERROR_STD(_T("UNknown exception raised: When closing shared session"));
	}
	if (boot) {
		try {
			LOG_DEBUG_STD(_T("Stopping: Message handling Plugins"));
			mainClient.unloadPlugins(true);
		} catch(NSPluginException e) {
			LOG_ERROR_STD(_T("Exception raised when unloading msg plugins: ") + e.error_ + _T(" in module: ") + e.file_);
		} catch(...) {
			LOG_ERROR_STD(_T("UNknown exception raised: When stopping message plguins"));
		}
	}
	LOG_MESSAGE_STD(_T("NSCLient++ - " SZVERSION) + _T(" Stopped succcessfully"));
	return true;
}
/**
 * Service control handler termination point.
 * When the program is stopped as a service this will be the "exit point".
 */
void NSClientT::TerminateService(void) {
	exitCore(true);
}

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
void WINAPI NSClientT::service_ctrl_dispatch(DWORD dwCtrlCode) {
	mainClient.service_ctrl_ex(dwCtrlCode, NULL, NULL, NULL);
}


void NSClientT::service_on_session_changed(DWORD dwSessionId, bool logon, DWORD dwEventType) {
	if (shared_server_.get() == NULL) {
		LOG_DEBUG_STD(_T("No shared session: ignoring change event!"));
		return;
	}
	LOG_DEBUG_STD(_T("Got session change: ") + strEx::itos(dwSessionId));
	if (!logon) {
		LOG_DEBUG_STD(_T("Not a logon event: ") + strEx::itos(dwEventType));
		return;
	}
	tray_starter::start(dwSessionId);
}

//////////////////////////////////////////////////////////////////////////
// Member functions

int NSClientT::commandLineExec(const TCHAR* module, const TCHAR* command, const unsigned int argLen, TCHAR** args) {
	std::wstring sModule = module;
	std::wstring moduleList = _T("");
	{
		boost::shared_lock<boost::shared_mutex> readLock(m_mutexRW, boost::get_system_time() + boost::posix_time::seconds(5));
		if (!readLock.owns_lock()) {
			LOG_ERROR(_T("FATAL ERROR: Could not get read-mutex."));
			return -1;
		}
		for (pluginList::size_type i=0;i<plugins_.size();++i) {
			NSCPlugin *p = plugins_[i];
			if (!moduleList.empty())
				moduleList += _T(", ");
			moduleList += p->getModule();
			if (p->getModule() == sModule) {
				LOG_DEBUG_STD(_T("Found module: ") + p->getName() + _T("..."));
				try {
					return p->commandLineExec(command, argLen, args);
				} catch (NSPluginException e) {
					LOG_ERROR_STD(_T("Could not execute command: ") + e.error_ + _T(" in ") + e.file_);
					return -1;
				}
			}
		}
	}
	try {
		plugin_type plugin = loadPlugin(getBasePath() + _T("modules\\") + module);
		LOG_DEBUG_STD(_T("Loading plugin: ") + plugin->getName() + _T("..."));
		plugin->load_plugin(NSCAPI::dontStart);
		return plugin->commandLineExec(command, argLen, args);
	} catch (NSPluginException e) {
		LOG_MESSAGE_STD(_T("Module (") + e.file_ + _T(") was not found: ") + e.error_);
	}
	try {
		plugin_type plugin = loadPlugin(getBasePath() + _T("modules\\") + module + _T(".dll"));
		LOG_DEBUG_STD(_T("Loading plugin: ") + plugin->getName() + _T("..."));
		plugin->load_plugin(NSCAPI::dontStart);
		return plugin->commandLineExec(command, argLen, args);
	} catch (NSPluginException e) {
		LOG_MESSAGE_STD(_T("Module (") + e.file_ + _T(") was not found: ") + e.error_);
	}
	LOG_ERROR_STD(_T("Module not found: ") + module + _T(" available modules are: ") + moduleList);
	return 0;
}

/**
 * Load a list of plug-ins
 * @param plugins A list with plug-ins (DLL files) to load
 */
void NSClientT::addPlugins(const std::list<std::wstring> plugins) {
	boost::shared_lock<boost::shared_mutex> readLock(m_mutexRW, boost::get_system_time() + boost::posix_time::seconds(10));
	if (!readLock.owns_lock()) {
		LOG_ERROR(_T("FATAL ERROR: Could not get read-mutex."));
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
			LOG_ERROR(_T("FATAL ERROR: Could not get read-mutex."));
			return;
		}
		commandHandlers_.clear();
		if (unloadLoggers)
			messageHandlers_.clear();
	}
	{
		boost::shared_lock<boost::shared_mutex> readLock(m_mutexRW, boost::get_system_time() + boost::posix_time::milliseconds(5000));
		if (!readLock.owns_lock()) {
			LOG_ERROR(_T("FATAL ERROR: Could not get read-mutex."));
			return;
		}
		for (pluginList::reverse_iterator it = plugins_.rbegin(); it != plugins_.rend(); ++it) {
			NSCPlugin *p = *it;
			if (p == NULL)
				continue;
			try {
				if (unloadLoggers || !p->hasMessageHandler()) {
					LOG_DEBUG_STD(_T("Unloading plugin: ") + p->getModule() + _T("..."));
					p->unload();
				} else {
					LOG_DEBUG_STD(_T("Skipping log plugin: ") + p->getModule() + _T("..."));
				}
			} catch(NSPluginException e) {
				LOG_ERROR_STD(_T("Exception raised when unloading plugin: ") + e.error_ + _T(" in module: ") + e.file_);
			} catch(...) {
				LOG_ERROR_STD(_T("Unknown exception raised when unloading plugin"));
			}
		}
	}
	{
		boost::unique_lock<boost::shared_mutex> writeLock(m_mutexRW, boost::get_system_time() + boost::posix_time::seconds(10));
		if (!writeLock.owns_lock()) {
			LOG_ERROR(_T("FATAL ERROR: Could not get read-mutex."));
			return;
		}
		for (pluginList::iterator it = plugins_.begin(); it != plugins_.end();) {
			NSCPlugin *p = (*it);
			try {
				if (p != NULL && (unloadLoggers|| !p->isLoaded())) {
					it = plugins_.erase(it);
					delete p;
					continue;
				}
			} catch(NSPluginException e) {
				LOG_ERROR_STD(_T("Exception raised when unloading plugin: ") + e.error_ + _T(" in module: ") + e.file_);
			} catch(...) {
				LOG_ERROR_STD(_T("Unknown exception raised when unloading plugin"));
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
			LOG_ERROR(_T("FATAL ERROR: Could not get read-mutex."));
			return;
		}
		for (pluginList::iterator it=plugins_.begin(); it != plugins_.end();) {
			LOG_DEBUG_STD(_T("Loading plugin: ") + (*it)->getName() + _T("..."));
			try {
				if (!(*it)->load_plugin(NSCAPI::normalStart)) {
					it = plugins_.erase(it);
					LOG_ERROR_STD(_T("Plugin refused to load: ") + (*it)->getModule());
				}
				++it;
			} catch (NSPluginException e) {
				it = plugins_.erase(it);
				LOG_ERROR_STD(_T("Could not load plugin: ") + e.file_ + _T(": ") + e.error_);
			} catch (...) {
				it = plugins_.erase(it);
				LOG_ERROR_STD(_T("Could not load plugin: ") + (*it)->getModule());
			}
		}
	}
	for (pluginList::iterator it=plugins_.begin(); it != plugins_.end();) {
		LOG_DEBUG_STD(_T("Loading plugin: ") + (*it)->getName() + _T("..."));
		try {
			(*it)->load_plugin(mode);
			++it;
		} catch(NSPluginException e) {
			it = plugins_.erase(it);
			LOG_ERROR_STD(_T("Exception raised when loading plugin: ") + e.error_ + _T(" in module: ") + e.file_ + _T(" plugin has been removed."));
		} catch(...) {
			it = plugins_.erase(it);
			LOG_ERROR_STD(_T("Unknown exception raised when unloading plugin plugin has been removed"));
		}
	}
	plugins_loaded_ = true;
}
/**
 * Load a single plug-in using a DLL filename
 * @param file The DLL file
 */
NSClientT::plugin_type NSClientT::loadPlugin(const std::wstring file) {
	return addPlugin(new NSCPlugin(file));
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
			LOG_ERROR(_T("FATAL ERROR: Could not get read-mutex."));
			return plugin;
		}
		plugins_.insert(plugins_.end(), plugin);
		if (plugin->hasCommandHandler())
			commandHandlers_.insert(commandHandlers_.end(), plugin);
		if (plugin->hasMessageHandler())
			messageHandlers_.insert(messageHandlers_.end(), plugin);
		settings_manager::get_core()->register_key(_T("/modules"), plugin->getFilename(), Settings::SettingsCore::key_string, plugin->getName(), plugin->getDescription(), _T(""), false);
	}
	return plugin;
}


std::wstring NSClientT::describeCommand(std::wstring command) {
	boost::shared_lock<boost::shared_mutex> readLock(m_mutexRWcmdDescriptions, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!readLock.owns_lock()) {
		LOG_ERROR(_T("FATAL ERROR: Could not get read-mutex when trying to get command list."));
		return _T("Failed to get mutex when describing command: ") + command;
	}
	cmdMap::const_iterator cit = cmdDescriptions_.find(command);
	if (cit == cmdDescriptions_.end())
		return _T("Command not found: ") + command + _T(", maybe it has not been register?");
	return (*cit).second;
}
std::list<std::wstring> NSClientT::getAllCommandNames() {
	std::list<std::wstring> lst;
	boost::shared_lock<boost::shared_mutex> readLock(m_mutexRWcmdDescriptions, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!readLock.owns_lock()) {
		LOG_ERROR(_T("FATAL ERROR: Could not get read-mutex when trying to get command list."));
		return lst;
	}
	for (cmdMap::const_iterator cit = cmdDescriptions_.begin(); cit != cmdDescriptions_.end(); ++cit) {
		lst.push_back((*cit).first);
	}
	return lst;
}
void NSClientT::registerCommand(std::wstring cmd, std::wstring desc) {
	boost::unique_lock<boost::shared_mutex> writeLock(m_mutexRWcmdDescriptions, boost::get_system_time() + boost::posix_time::seconds(10));
	if (!writeLock.owns_lock()) {
		LOG_ERROR_STD(_T("FATAL ERROR: Failed to describe command:") + cmd);
		return;
	}
	cmdDescriptions_[cmd] = desc;
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
			LOG_ERROR(_T("Failed to get length: :("));
			return setting_keys::settings_def::PAYLOAD_LEN_DEFAULT;
		}
	}
	return len;
}

NSCAPI::nagiosReturn NSClientT::inject(std::wstring command, std::wstring arguments, TCHAR splitter, bool escape, std::wstring &msg, std::wstring & perf) {
	if (shared_client_.get() != NULL && shared_client_->hasMaster()) {
		try {
			return shared_client_->inject(command, arguments, splitter, escape, msg, perf);
		} catch (nsclient_session::session_exception &e) {
			LOG_ERROR_STD(_T("Failed to inject remote command: ") + e.what());
			return NSCAPI::returnCRIT;
		} catch (...) {
			LOG_ERROR_STD(_T("Failed to inject remote command: Unknown exception"));
			return NSCAPI::returnCRIT;
		}
	} else {
		unsigned int aLen = 0;
		TCHAR ** aBuf = arrayBuffer::split2arrayBuffer(arguments, splitter, aLen, escape);
		unsigned int buf_len = getBufferLength();
		TCHAR * mBuf = new TCHAR[buf_len+1]; mBuf[0] = '\0';
		TCHAR * pBuf = new TCHAR[buf_len+1]; pBuf[0] = '\0';
		NSCAPI::nagiosReturn ret = injectRAW(command.c_str(), aLen, aBuf, mBuf, buf_len, pBuf, buf_len);
		arrayBuffer::destroyArrayBuffer(aBuf, aLen);
		if ( (ret == NSCAPI::returnInvalidBufferLen) || (ret == NSCAPI::returnIgnored) ) {
			delete [] mBuf;
			delete [] pBuf;
			return ret;
		}
		msg = mBuf;
		perf = pBuf;
		delete [] mBuf;
		delete [] pBuf;
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
NSCAPI::nagiosReturn NSClientT::injectRAW(const TCHAR* command, const unsigned int argLen, TCHAR **argument, TCHAR *returnMessageBuffer, unsigned int returnMessageBufferLen, TCHAR *returnPerfBuffer, unsigned int returnPerfBufferLen) {
	if (logDebug()) {
		LOG_DEBUG_STD(_T("Injecting: ") + (std::wstring) command + _T(": ") + arrayBuffer::arrayBuffer2string(argument, argLen, _T(", ")));
	}
	if (shared_client_.get() != NULL && shared_client_->hasMaster()) {
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
	} else {
		boost::shared_lock<boost::shared_mutex> readLock(m_mutexRW, boost::get_system_time() + boost::posix_time::milliseconds(5000));
		if (!readLock.owns_lock()) {
			LOG_ERROR(_T("FATAL ERROR: Could not get read-mutex."));
			return NSCAPI::returnUNKNOWN;
		}
		for (pluginList::size_type i = 0; i < commandHandlers_.size(); i++) {
			try {
				NSCAPI::nagiosReturn c = commandHandlers_[i]->handleCommand(command, argLen, argument, returnMessageBuffer, returnMessageBufferLen, returnPerfBuffer, returnPerfBufferLen);
				switch (c) {
					case NSCAPI::returnInvalidBufferLen:
						LOG_ERROR(_T("UNKNOWN: Return buffer to small to handle this command."));
						return c;
					case NSCAPI::returnIgnored:
						break;
					case NSCAPI::returnOK:
					case NSCAPI::returnWARN:
					case NSCAPI::returnCRIT:
					case NSCAPI::returnUNKNOWN:
						LOG_DEBUG_STD(_T("Injected Result: ") + NSCHelper::translateReturn(c) + _T(" '") + (std::wstring)(returnMessageBuffer) + _T("'"));
						LOG_DEBUG_STD(_T("Injected Performance Result: '") +(std::wstring)(returnPerfBuffer) + _T("'"));
						return c;
					default:
						LOG_ERROR_STD(_T("Unknown error from handleCommand: ") + strEx::itos(c) + _T(" the injected command was: ") + (std::wstring)command);
						return c;
				}
			} catch(const NSPluginException& e) {
				LOG_ERROR_STD(_T("Exception raised: ") + e.error_ + _T(" in module: ") + e.file_);
				return NSCAPI::returnCRIT;
			} catch(...) {
				LOG_ERROR_STD(_T("Unknown exception raised in module"));
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
		LOG_ERROR(_T("FATAL ERROR: Could not get read-mutex."));
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
			LOG_ERROR_STD(_T("Could not load plugin: ") + e.file_ + _T(": ") + e.error_);
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
	OutputDebugString(msg.c_str());
	std::wcout << msg << std::endl;
}
/**
 * Report a message to all logging enabled modules.
 *
 * @param msgType Message type 
 * @param file Filename generally __FILE__
 * @param line  Line number, generally __LINE__
 * @param message The message as a human readable string.
 */
void NSClientT::reportMessage(int msgType, const TCHAR* file, const int line, std::wstring message) {
	try {
		strEx::replace(message, _T("\n"), _T(" "));
		strEx::replace(message, _T("\r"), _T(" "));
		if ((msgType == NSCAPI::debug)&&(!logDebug())) {
			return;
		}
		if (shared_server_.get() != NULL && shared_server_->hasClients()) {
			try {
				shared_server_->sendLogMessageToClients(msgType, file, line, message);
			} catch (nsclient_session::session_exception e) {
				log_broken_message(_T("Failed to send message to clients: ") + e.what());
			}
		}
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
				std::string k = "?";
				switch (msgType) {
				case NSCAPI::critical:
					k ="c";
					break;
				case NSCAPI::warning:
					k ="w";
					break;
				case NSCAPI::error:
					k ="e";
					break;
				case NSCAPI::log:
					k ="l";
					break;
				case NSCAPI::debug:
					k ="d";
					break;
				}	
				std::cout << k << " " << strEx::wstring_to_string(file_stl) << "(" << line << ") " << strEx::wstring_to_string(message) << std::endl;
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
std::wstring NSClientT::getBasePath(void) {
	boost::unique_lock<boost::timed_mutex> lock(internalVariables, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!lock.owns_lock()) {
		LOG_ERROR(_T("FATAL ERROR: Could not get mutex."));
		return _T("FATAL ERROR");
	}
	if (!basePath.empty())
		return basePath;
	unsigned int buf_len = 4096;
	TCHAR* buffer = new TCHAR[buf_len+1];
	GetModuleFileName(NULL, buffer, buf_len);
	std::wstring path = buffer;
	std::wstring::size_type pos = path.rfind('\\');
	basePath = path.substr(0, pos) + _T("\\");
	delete [] buffer;
	try {
		settings_manager::get_core()->set_base(basePath);
	} catch (SettingsException e) {
		LOG_ERROR_STD(_T("Failed to set settings file: ") + e.getMessage());
	} catch (...) {
		LOG_ERROR_STD(_T("Failed to set settings file"));
	}
	return basePath;
}


std::wstring Encrypt(std::wstring str, unsigned int algorithm) {
	unsigned int len = 0;
	NSAPIEncrypt(algorithm, str.c_str(), static_cast<unsigned int>(str.size()), NULL, &len);
	len+=2;
	TCHAR *buf = new TCHAR[len+1];
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
	TCHAR *buf = new TCHAR[len+1];
	NSCAPI::errorReturn ret = NSAPIDecrypt(algorithm, str.c_str(), static_cast<unsigned int>(str.size()), buf, &len);
	if (ret == NSCAPI::isSuccess) {
		std::wstring ret = buf;
		delete [] buf;
		return ret;
	}
	return _T("");
}


