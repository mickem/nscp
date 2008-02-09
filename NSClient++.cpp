//////////////////////////////////////////////////////////////////////////
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
#include "stdafx.h"
#include <winsvc.h>
#include "NSClient++.h"
#include <Settings.h>
#include <charEx.h>
#include <Socket.h>
#include <b64/b64.h>
#include <config.h>
#include <msvc_wrappers.h>


NSClient mainClient;	// Global core instance.
bool g_bConsoleLog = false;
//////////////////////////////////////////////////////////////////////////
// Startup code

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
	int nRetCode = 0;
	if ( (argc > 1) && ((*argv[1] == '-') || (*argv[1] == '/')) ) {
		if ( _wcsicmp( _T("install"), argv[1]+1 ) == 0 ) {
			g_bConsoleLog = true;
			try {
				serviceControll::Install(SZSERVICENAME, SZSERVICEDISPLAYNAME, SZDEPENDENCIES);
			} catch (const serviceControll::SCException& e) {
				LOG_MESSAGE_STD(_T("Service installation failed: ") + e.error_);
				return -1;
			}
			try {
				serviceControll::SetDescription(SZSERVICENAME, SZSERVICEDESCRIPTION);
			} catch (const serviceControll::SCException& e) {
				LOG_MESSAGE_STD(_T("Couldn't set service description: ") + e.error_);
			}
			LOG_MESSAGE(_T("Service installed!"));
		} else if ( _wcsicmp( _T("uninstall"), argv[1]+1 ) == 0 ) {
			g_bConsoleLog = true;
			try {
				serviceControll::Uninstall(SZSERVICENAME);
			} catch (const serviceControll::SCException& e) {
				LOG_MESSAGE_STD(_T("Service deinstallation failed; ") + e.error_);
				return -1;
			}
		} else if ( _wcsicmp( _T("encrypt"), argv[1]+1 ) == 0 ) {
			g_bConsoleLog = true;
			std::wstring password;
			try {
				Settings::getInstance()->setFile(mainClient.getBasePath(), _T("NSC.ini"));
			} catch (SettingsException e) {
				std::wcout << _T("Could not find settings: ") << e.getMessage() << std::endl;;
				return 1;
			}
			std::wcout << _T("Enter password to encrypt (has to be a single word): ");
			std::wcin >> password;
			std::wstring xor_pwd = Encrypt(password);
			std::wcout << _T("obfuscated_password=") << xor_pwd << std::endl;
			std::wstring outPasswd = Decrypt(xor_pwd);
			if (password != outPasswd) 
				std::wcout << _T("ERROR: Password did not match: ") << outPasswd<< std::endl;
			Settings::destroyInstance();
			return 0;
		} else if ( _wcsicmp( _T("start"), argv[1]+1 ) == 0 ) {
			g_bConsoleLog = true;
			try {
				serviceControll::Start(SZSERVICENAME);
			} catch (const serviceControll::SCException& e) {
				LOG_MESSAGE_STD(_T("Service failed to start: ") + e.error_);
				return -1;
			}
		} else if ( _wcsicmp( _T("stop"), argv[1]+1 ) == 0 ) {
			g_bConsoleLog = true;
			try {
				serviceControll::Stop(SZSERVICENAME);
			} catch (const serviceControll::SCException& e) {
				LOG_MESSAGE_STD(_T("Service failed to stop: ") + e.error_);
				return -1;
			}
		} else if ( _wcsicmp( _T("about"), argv[1]+1 ) == 0 ) {
			g_bConsoleLog = true;
			LOG_MESSAGE(SZAPPNAME _T(" (C) Michael Medin"));
			LOG_MESSAGE(_T("Version ") SZVERSION);
		} else if ( _wcsicmp( _T("version"), argv[1]+1 ) == 0 ) {
			g_bConsoleLog = true;
			LOG_MESSAGE(SZAPPNAME _T(" Version: ") SZVERSION _T(", Plattform: ") SZARCH);
		} else if ( _wcsicmp( _T("noboot"), argv[1]+1 ) == 0 ) {
			g_bConsoleLog = true;
			mainClient.enableDebug(true);
			int nRetCode = -1;
			if (argc>=4)
				nRetCode = mainClient.commandLineExec(argv[2], argv[3], argc-4, &argv[4]);
			else if (argc>=3)
				nRetCode = mainClient.commandLineExec(argv[2], argv[3], 0, NULL);
			return nRetCode;
		} else if ( _wcsicmp( _T("test"), argv[1]+1 ) == 0 ) {
			std::wcout << "Launching test mode..." << std::endl;
			try {
				if (serviceControll::isStarted(SZSERVICENAME)) {
					std::wcerr << "Service seems to be started, this is probably not a good idea..." << std::endl;
				}
			} catch (const serviceControll::SCException& e) {
				// Empty by design
			}
			g_bConsoleLog = true;
			mainClient.enableDebug(true);
			if (!mainClient.InitiateService()) {
				LOG_ERROR_STD(_T("Service *NOT* started!"));
				return -1;
			}
			LOG_MESSAGE_STD(_T("Using settings from: ") + Settings::getInstance()->getActiveType());
			LOG_MESSAGE(_T("Enter command to inject or exit to terminate..."));
			std::wstring s = _T("");
			std::wstring buff = _T("");
			std::wcin >> s;
			while (s != _T("exit")) {
				if (std::cin.peek() < 15) {
					buff += s;
					strEx::token t = strEx::getToken(buff, ' ');
					std::wstring msg, perf;
					NSCAPI::nagiosReturn ret = mainClient.inject(t.first, t.second, ' ', true, msg, perf);
					if (perf.empty())
						std::wcout << NSCHelper::translateReturn(ret) << _T(":") << msg << std::endl;
					else
						std::wcout << NSCHelper::translateReturn(ret) << _T(":") << msg << _T("|") << perf << std::endl;
					buff = _T("");
				} else {
					buff += s + _T(" ");
				}
				std::wcin >> s;
			}
			mainClient.TerminateService();
			return 0;
		} else {
			std::wcerr << _T("Usage: -version, -about, -install, -uninstall, -start, -stop, -encrypt") << std::endl;
			std::wcerr << _T("Usage: [-noboot] <ModuleName> <commnd> [arguments]") << std::endl;
			return -1;
		}
		return nRetCode;
	} else if (argc > 2) {
		g_bConsoleLog = true;
		mainClient.InitiateService();
		if (argc>=3)
			nRetCode = mainClient.commandLineExec(argv[1], argv[2], argc-3, &argv[3]);
		else
			nRetCode = mainClient.commandLineExec(argv[1], argv[2], 0, NULL);
		mainClient.TerminateService();
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

//////////////////////////////////////////////////////////////////////////
// Service functions

/**
 * Service control handler startup point.
 * When the program is started as a service this will be the entry point.
 */
bool NSClientT::InitiateService() {
	try {
		Settings::getInstance()->setFile(getBasePath(), _T("NSC.ini"));
		if (debug_) {
			Settings::getInstance()->setInt(_T("log"), _T("debug"), 1);
		}
	} catch (SettingsException e) {
		LOG_ERROR_STD(_T("Could not find settings: ") + e.getMessage());
		return false;
	} catch (...) {
		LOG_ERROR_STD(_T("Unknown exception reading settings..."));
		return false;
	}

	try {
		simpleSocket::WSAStartup();
	} catch (simpleSocket::SocketException e) {
		LOG_ERROR_STD(_T("Uncaught exception: ") + e.getMessage());
		return false;
	} catch (...) {
		LOG_ERROR_STD(_T("Unknown exception iniating socket..."));
		return false;
	}
	try {
		SettingsT::sectionList list = Settings::getInstance()->getSection(_T("modules"));
		for (SettingsT::sectionList::iterator it = list.begin(); it != list.end(); it++) {
			try {
				loadPlugin(getBasePath() + _T("modules\\") + (*it));
			} catch(const NSPluginException& e) {
				LOG_ERROR_STD(_T("Exception raised: ") + e.error_ + _T(" in module: ") + e.file_);
				//return false;
			} catch (...) {
				LOG_ERROR_STD(_T("Unknown exception loading plugin: ") + (*it));
				return false;
			}
		}
	} catch (SettingsException e) {
		NSC_LOG_ERROR_STD(_T("Failed to set settings file") + e.getMessage());
	}
	try {
		loadPlugins();
	} catch (...) {
		LOG_ERROR_STD(_T("Unknown exception loading plugins"));
		return false;
	}
	return true;
}
/**
 * Service control handler termination point.
 * When the program is stopped as a service this will be the "exit point".
 */
void NSClientT::TerminateService(void) {
	try {
		mainClient.unloadPlugins();
	} catch(NSPluginException &e) {
		std::wcout << _T("Exception raised: ") << e.error_ << _T(" in module: ") << e.file_ << std::endl;;
	}
	try {
		simpleSocket::WSACleanup();
	} catch (simpleSocket::SocketException e) {
		LOG_ERROR_STD(_T("Uncaught exception: ") + e.getMessage());
	}
	Settings::destroyInstance();
}

/**
 * Forward this to the main service dispatcher helper class
 * @param dwArgc 
 * @param *lpszArgv 
 */
void WINAPI NSClientT::service_main_dispatch(DWORD dwArgc, LPTSTR *lpszArgv) {
	mainClient.service_main(dwArgc, lpszArgv);
}
/**
 * Forward this to the main service dispatcher helper class
 * @param dwCtrlCode 
 */
void WINAPI NSClientT::service_ctrl_dispatch(DWORD dwCtrlCode) {
	mainClient.service_ctrl(dwCtrlCode);
}

//////////////////////////////////////////////////////////////////////////
// Member functions

int NSClientT::commandLineExec(const TCHAR* module, const TCHAR* command, const unsigned int argLen, TCHAR** args) {
	std::wstring sModule = module;
	std::wstring moduleList = _T("");
	{
		ReadLock readLock(&m_mutexRW, true, 10000);
		if (!readLock.IsLocked()) {
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
	LOG_MESSAGE_STD(_T("Module was not loaded, attempt to load it"));
	try {
		plugin_type plugin = loadPlugin(getBasePath() + _T("modules\\") + module);
		LOG_DEBUG_STD(_T("Loading plugin: ") + plugin->getName() + _T("..."));
		plugin->load_plugin();
		return plugin->commandLineExec(command, argLen, args);
	} catch (NSPluginException e) {
		LOG_MESSAGE_STD(_T("Module (") + e.file_ + _T(") was not found: ") + e.error_);
	}
	try {
		plugin_type plugin = loadPlugin(getBasePath() + _T("modules\\") + module + _T(".dll"));
		LOG_DEBUG_STD(_T("Loading plugin: ") + plugin->getName() + _T("..."));
		plugin->load_plugin();
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
	ReadLock readLock(&m_mutexRW, true, 10000);
	if (!readLock.IsLocked()) {
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
void NSClientT::unloadPlugins() {
	{
		WriteLock writeLock(&m_mutexRW, true, 10000);
		if (!writeLock.IsLocked()) {
			LOG_ERROR(_T("FATAL ERROR: Could not get read-mutex."));
			return;
		}
		commandHandlers_.clear();
		messageHandlers_.clear();
	}
	{
		ReadLock readLock(&m_mutexRW, true, 10000);
		if (!readLock.IsLocked()) {
			LOG_ERROR(_T("FATAL ERROR: Could not get read-mutex."));
			return;
		}
		for (pluginList::size_type i=plugins_.size();i>0;i--) {
			NSCPlugin *p = plugins_[i-1];
			LOG_DEBUG_STD(_T("Unloading plugin: ") + p->getName() + _T("..."));
			p->unload();
		}
	}
	{
		WriteLock writeLock(&m_mutexRW, true, 10000);
		if (!writeLock.IsLocked()) {
			LOG_ERROR(_T("FATAL ERROR: Could not get read-mutex."));
			return;
		}
		for (pluginList::size_type i=plugins_.size();i>0;i--) {
			NSCPlugin *p = plugins_[i-1];
			plugins_[i-1] = NULL;
			delete p;
		}
		plugins_.clear();
	}
}

void NSClientT::loadPlugins() {
	ReadLock readLock(&m_mutexRW, true, 10000);
	if (!readLock.IsLocked()) {
		LOG_ERROR(_T("FATAL ERROR: Could not get read-mutex."));
		return;
	}
	for (pluginList::iterator it=plugins_.begin(); it != plugins_.end(); ++it) {
		LOG_DEBUG_STD(_T("Loading plugin: ") + (*it)->getName() + _T("..."));
		(*it)->load_plugin();
	}
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
 * @param *plugin The plug-ininstance to load. The pointer is managed by the 
 */
NSClientT::plugin_type NSClientT::addPlugin(plugin_type plugin) {
	plugin->load_dll();
	{
		WriteLock writeLock(&m_mutexRW, true, 10000);
		if (!writeLock.IsLocked()) {
			LOG_ERROR(_T("FATAL ERROR: Could not get read-mutex."));
			return plugin;
		}
		plugins_.insert(plugins_.end(), plugin);
		if (plugin->hasCommandHandler())
			commandHandlers_.insert(commandHandlers_.end(), plugin);
		if (plugin->hasMessageHandler())
			messageHandlers_.insert(messageHandlers_.end(), plugin);
	}
	return plugin;
}


std::wstring NSClientT::describeCommand(std::wstring command) {
	ReadLock readLock(&m_mutexRWcmdDescriptions, true, 5000);
	if (!readLock.IsLocked()) {
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
	ReadLock readLock(&m_mutexRWcmdDescriptions, true, 5000);
	if (!readLock.IsLocked()) {
		LOG_ERROR(_T("FATAL ERROR: Could not get read-mutex when trying to get command list."));
		return lst;
	}
	for (cmdMap::const_iterator cit = cmdDescriptions_.begin(); cit != cmdDescriptions_.end(); ++cit) {
		lst.push_back((*cit).first);
	}
	return lst;
}
void NSClientT::registerCommand(std::wstring cmd, std::wstring desc) {
	WriteLock writeLock(&m_mutexRWcmdDescriptions, true, 10000);
	if (!writeLock.IsLocked()) {
		LOG_ERROR_STD(_T("FATAL ERROR: Failed to describe command:") + cmd);
		return;
	}
	cmdDescriptions_[cmd] = desc;
}

NSCAPI::nagiosReturn NSClientT::inject(std::wstring command, std::wstring arguments, TCHAR splitter, bool escape, std::wstring &msg, std::wstring & perf) {
	unsigned int aLen = 0;
	TCHAR ** aBuf = arrayBuffer::split2arrayBuffer(arguments, splitter, aLen, escape);
	TCHAR * mBuf = new TCHAR[1024]; mBuf[0] = '\0';
	TCHAR * pBuf = new TCHAR[1024]; pBuf[0] = '\0';
	NSCAPI::nagiosReturn ret = injectRAW(command.c_str(), aLen, aBuf, mBuf, 1023, pBuf, 1023);
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
	ReadLock readLock(&m_mutexRW, true, 5000);
	if (!readLock.IsLocked()) {
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
		}
	}
	LOG_MESSAGE_STD(_T("No handler for command: '") + command + _T("'"));
	return NSCAPI::returnIgnored;
}

bool NSClientT::logDebug() {
	typedef enum status {unknown, debug, nodebug };
	static status d = unknown;
	if (d == unknown) {
		try {
			if (Settings::getInstance()->getInt(_T("log"), _T("debug"), 0) == 1)
				d = debug;
			else
				d = nodebug;
		} catch (SettingsException e) {
			d = debug;
		}
	}
	return (d == debug);
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
	if ((msgType == NSCAPI::debug)&&(!logDebug())) {
		return;
	}
	std::wstring file_stl = file;
	std::wstring::size_type pos = file_stl.find_last_of(_T("\\"));
	if (pos != std::wstring::npos)
		file_stl = file_stl.substr(pos);
	{
		ReadLock readLock(&m_mutexRW, true, 5000);
		if (!readLock.IsLocked()) {
			std::wcout << _T("Message was lost as the core was locked...") << std::endl;
			return;
		}
		MutexLock lock(messageMutex);
		if (!lock.hasMutex()) {
			std::wcout << _T("Message was lost as the core was locked...") << std::endl;
			std::wcout << message << std::endl;
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
			}
			std::wcout << k << _T(" ") << file_stl << _T("(") << line << _T(") ") << message << std::endl;
		}
		for (pluginList::size_type i = 0; i< messageHandlers_.size(); i++) {
			try {
				messageHandlers_[i]->handleMessage(msgType, file, line, message.c_str());
			} catch(const NSPluginException& e) {
				// Here we are pretty much fucked! (as logging this might cause a loop :)
				std::wcout << _T("Caught: ") << e.error_ << _T(" when trying to log a message...") << std::endl;
				std::wcout << _T("This is *really really* bad, now the world is about to end...") << std::endl;
			}
		}
	}
}
std::wstring NSClientT::getBasePath(void) {
	MutexLock lock(internalVariables);
	if (!lock.hasMutex()) {
		LOG_ERROR(_T("FATAL ERROR: Could not get mutex."));
		return _T("FATAL ERROR");
	}
	if (!basePath.empty())
		return basePath;
	TCHAR* buffer = new TCHAR[1024];
	GetModuleFileName(NULL, buffer, 1023);
	std::wstring path = buffer;
	std::wstring::size_type pos = path.rfind('\\');
	basePath = path.substr(0, pos) + _T("\\");
	delete [] buffer;
	try {
		Settings::getInstance()->setFile(basePath, _T("NSC.ini"));
	} catch (SettingsException e) {
		NSC_LOG_ERROR_STD(_T("Failed to set settings file") + e.getMessage());
	}
	return basePath;
}


NSCAPI::errorReturn NSAPIGetSettingsString(const TCHAR* section, const TCHAR* key, const TCHAR* defaultValue, TCHAR* buffer, unsigned int bufLen) {
	return NSCHelper::wrapReturnString(buffer, bufLen, Settings::getInstance()->getString(section, key, defaultValue), NSCAPI::isSuccess);
}
int NSAPIGetSettingsInt(const TCHAR* section, const TCHAR* key, int defaultValue) {
	try {
		return Settings::getInstance()->getInt(section, key, defaultValue);
	} catch (SettingsException e) {
		NSC_LOG_ERROR_STD(_T("Failed to set settings file") + e.getMessage());
		return defaultValue;
	}
}
NSCAPI::errorReturn NSAPIGetBasePath(TCHAR*buffer, unsigned int bufLen) {
	return NSCHelper::wrapReturnString(buffer, bufLen, mainClient.getBasePath(), NSCAPI::isSuccess);
}
NSCAPI::errorReturn NSAPIGetApplicationName(TCHAR*buffer, unsigned int bufLen) {
	return NSCHelper::wrapReturnString(buffer, bufLen, SZAPPNAME, NSCAPI::isSuccess);
}
NSCAPI::errorReturn NSAPIGetApplicationVersionStr(TCHAR*buffer, unsigned int bufLen) {
	return NSCHelper::wrapReturnString(buffer, bufLen, SZVERSION, NSCAPI::isSuccess);
}
void NSAPIMessage(int msgType, const TCHAR* file, const int line, const TCHAR* message) {
	mainClient.reportMessage(msgType, file, line, message);
}
void NSAPIStopServer(void) {
	serviceControll::StopNoWait(SZSERVICENAME);
}
NSCAPI::nagiosReturn NSAPIInject(const TCHAR* command, const unsigned int argLen, TCHAR **argument, TCHAR *returnMessageBuffer, unsigned int returnMessageBufferLen, TCHAR *returnPerfBuffer, unsigned int returnPerfBufferLen) {
	return mainClient.injectRAW(command, argLen, argument, returnMessageBuffer, returnMessageBufferLen, returnPerfBuffer, returnPerfBufferLen);
}
NSCAPI::errorReturn NSAPIGetSettingsSection(const TCHAR* section, TCHAR*** aBuffer, unsigned int * bufLen) {
	unsigned int len = 0;
	*aBuffer = arrayBuffer::list2arrayBuffer(Settings::getInstance()->getSection(section), len);
	*bufLen = len;
	return NSCAPI::isSuccess;
}
NSCAPI::errorReturn NSAPIReleaseSettingsSectionBuffer(TCHAR*** aBuffer, unsigned int * bufLen) {
	arrayBuffer::destroyArrayBuffer(*aBuffer, *bufLen);
	*bufLen = 0;
	*aBuffer = NULL;
	return NSCAPI::isSuccess;
}

NSCAPI::boolReturn NSAPICheckLogMessages(int messageType) {
	if (mainClient.logDebug())
		return NSCAPI::istrue;
	return NSCAPI::isfalse;
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

NSCAPI::errorReturn NSAPIEncrypt(unsigned int algorithm, const TCHAR* inBuffer, unsigned int inBufLen, TCHAR* outBuf, unsigned int *outBufLen) {
	if (algorithm != NSCAPI::xor) {
		LOG_ERROR(_T("Unknown algortihm requested."));
		return NSCAPI::hasFailed;
	}
	std::wstring key = Settings::getInstance()->getString(MAIN_SECTION_TITLE, MAIN_MASTERKEY, MAIN_MASTERKEY_DEFAULT);
	int tcharInBufLen = 0;
	char *c = charEx::tchar_to_char(inBuffer, inBufLen, tcharInBufLen);
	std::wstring::size_type j=0;
	for (int i=0;i<tcharInBufLen;i++,j++) {
		if (j > key.size())
			j = 0;
		c[i] ^= key[j];
	}
	size_t cOutBufLen = b64::b64_encode(reinterpret_cast<void*>(c), tcharInBufLen, NULL, NULL);
	if (!outBuf) {
		*outBufLen = static_cast<unsigned int>(cOutBufLen*2); // TODO: Guessing wildly here but no proper way to tell without a lot of extra work
		return NSCAPI::isSuccess;
	}
	char *cOutBuf = new char[cOutBufLen+1];
	size_t len = b64::b64_encode(reinterpret_cast<void*>(c), tcharInBufLen, cOutBuf, cOutBufLen);
	delete [] c;
	if (len == 0) {
		LOG_ERROR(_T("Invalid out buffer length."));
		return NSCAPI::isInvalidBufferLen;
	}
	int realOutLen;
	TCHAR *realOut = charEx::char_to_tchar(cOutBuf, cOutBufLen, realOutLen);
	if (static_cast<unsigned int>(realOutLen) >= *outBufLen) {
		LOG_ERROR_STD(_T("Invalid out buffer length: ") + strEx::itos(realOutLen) + _T(" was needed but only ") + strEx::itos(*outBufLen) + _T(" was allocated."));
		return NSCAPI::isInvalidBufferLen;
	}
	wcsncpy_s(outBuf, *outBufLen, realOut, realOutLen);
	delete [] realOut;
	outBuf[realOutLen] = 0;
	*outBufLen = static_cast<unsigned int>(realOutLen);
	return NSCAPI::isSuccess;
}

NSCAPI::errorReturn NSAPIDecrypt(unsigned int algorithm, const TCHAR* inBuffer, unsigned int inBufLen, TCHAR* outBuf, unsigned int *outBufLen) {
	if (algorithm != NSCAPI::xor) {
		LOG_ERROR(_T("Unknown algortihm requested."));
		return NSCAPI::hasFailed;
	}
	int inBufLenC = 0;
	char *inBufferC = charEx::tchar_to_char(inBuffer, inBufLen, inBufLenC);
	size_t cOutLen =  b64::b64_decode(inBufferC, inBufLenC, NULL, NULL);
	if (!outBuf) {
		*outBufLen = static_cast<unsigned int>(cOutLen*2); // TODO: Guessing wildly here but no proper way to tell without a lot of extra work
		return NSCAPI::isSuccess;
	}
	char *cOutBuf = new char[cOutLen+1];
	size_t len = b64::b64_decode(inBufferC, inBufLenC, reinterpret_cast<void*>(cOutBuf), cOutLen);
	delete [] inBufferC;
	if (len == 0) {
		LOG_ERROR(_T("Invalid out buffer length."));
		return NSCAPI::isInvalidBufferLen;
	}
	int realOutLen;

	std::wstring key = Settings::getInstance()->getString(MAIN_SECTION_TITLE, MAIN_MASTERKEY, MAIN_MASTERKEY_DEFAULT);
	std::wstring::size_type j=0;
	for (int i=0;i<cOutLen;i++,j++) {
		if (j > key.size())
			j = 0;
		cOutBuf[i] ^= key[j];
	}

	TCHAR *realOut = charEx::char_to_tchar(cOutBuf, cOutLen, realOutLen);
	if (static_cast<unsigned int>(realOutLen) >= *outBufLen) {
		LOG_ERROR_STD(_T("Invalid out buffer length: ") + strEx::itos(realOutLen) + _T(" was needed but only ") + strEx::itos(*outBufLen) + _T(" was allocated."));
		return NSCAPI::isInvalidBufferLen;
	}
	wcsncpy_s(outBuf, *outBufLen, realOut, realOutLen);
	delete [] realOut;
	outBuf[realOutLen] = 0;
	*outBufLen = static_cast<unsigned int>(realOutLen);
	return NSCAPI::isSuccess;
}

NSCAPI::errorReturn NSAPISetSettingsString(const TCHAR* section, const TCHAR* key, const TCHAR* value) {
	Settings::getInstance()->setString(section, key, value);
	return NSCAPI::isSuccess;
}
NSCAPI::errorReturn NSAPISetSettingsInt(const TCHAR* section, const TCHAR* key, int value) {
	Settings::getInstance()->setInt(section, key, value);
	return NSCAPI::isSuccess;
}
NSCAPI::errorReturn NSAPIWriteSettings(int type) {
	try {
		if (type == NSCAPI::settings_registry)
			Settings::getInstance()->write(REGSettings::getType());
		else if (type == NSCAPI::settings_inifile)
			Settings::getInstance()->write(INISettings::getType());
		else
			Settings::getInstance()->write();
	} catch (SettingsException e) {
		LOG_ERROR_STD(e.getMessage());
		return NSCAPI::hasFailed;
	}
	return NSCAPI::isSuccess;
}
NSCAPI::errorReturn NSAPIReadSettings(int type) {
	try {
		if (type == NSCAPI::settings_registry)
			Settings::getInstance()->read(REGSettings::getType());
		else if (type == NSCAPI::settings_inifile)
			Settings::getInstance()->read(INISettings::getType());
		else
			Settings::getInstance()->read();
	} catch (SettingsException e) {
		LOG_ERROR_STD(e.getMessage());
		return NSCAPI::hasFailed;
	}
	return NSCAPI::isSuccess;
}
NSCAPI::errorReturn NSAPIRehash(int flag) {
	return NSCAPI::hasFailed;
}
NSCAPI::errorReturn NSAPIDescribeCommand(const TCHAR* command, TCHAR* buffer, unsigned int bufLen) {
	return NSCHelper::wrapReturnString(buffer, bufLen, mainClient.describeCommand(command), NSCAPI::isSuccess);
}
NSCAPI::errorReturn NSAPIGetAllCommandNames(arrayBuffer::arrayBuffer* aBuffer, unsigned int *bufLen) {
	unsigned int len = 0;
	*aBuffer = arrayBuffer::list2arrayBuffer(mainClient.getAllCommandNames(), len);
	*bufLen = len;
	return NSCAPI::isSuccess;
}
NSCAPI::errorReturn NSAPIReleaseAllCommandNamessBuffer(TCHAR*** aBuffer, unsigned int * bufLen) {
	arrayBuffer::destroyArrayBuffer(*aBuffer, *bufLen);
	*bufLen = 0;
	*aBuffer = NULL;
	return NSCAPI::isSuccess;
}
NSCAPI::errorReturn NSAPIRegisterCommand(const TCHAR* cmd,const TCHAR* desc) {
	mainClient.registerCommand(cmd, desc);
	return NSCAPI::isSuccess;
}


LPVOID NSAPILoader(TCHAR*buffer) {
	if (_wcsicmp(buffer, _T("NSAPIGetApplicationName")) == 0)
		return &NSAPIGetApplicationName;
	if (_wcsicmp(buffer, _T("NSAPIGetApplicationVersionStr")) == 0)
		return &NSAPIGetApplicationVersionStr;
	if (_wcsicmp(buffer, _T("NSAPIGetSettingsString")) == 0)
		return &NSAPIGetSettingsString;
	if (_wcsicmp(buffer, _T("NSAPIGetSettingsSection")) == 0)
		return &NSAPIGetSettingsSection;
	if (_wcsicmp(buffer, _T("NSAPIReleaseSettingsSectionBuffer")) == 0)
		return &NSAPIReleaseSettingsSectionBuffer;
	if (_wcsicmp(buffer, _T("NSAPIGetSettingsInt")) == 0)
		return &NSAPIGetSettingsInt;
	if (_wcsicmp(buffer, _T("NSAPIMessage")) == 0)
		return &NSAPIMessage;
	if (_wcsicmp(buffer, _T("NSAPIStopServer")) == 0)
		return &NSAPIStopServer;
	if (_wcsicmp(buffer, _T("NSAPIInject")) == 0)
		return &NSAPIInject;
	if (_wcsicmp(buffer, _T("NSAPIGetBasePath")) == 0)
		return &NSAPIGetBasePath;
	if (_wcsicmp(buffer, _T("NSAPICheckLogMessages")) == 0)
		return &NSAPICheckLogMessages;
	if (_wcsicmp(buffer, _T("NSAPIEncrypt")) == 0)
		return &NSAPIEncrypt;
	if (_wcsicmp(buffer, _T("NSAPIDecrypt")) == 0)
		return &NSAPIDecrypt;
	if (_wcsicmp(buffer, _T("NSAPISetSettingsString")) == 0)
		return &NSAPISetSettingsString;
	if (_wcsicmp(buffer, _T("NSAPISetSettingsInt")) == 0)
		return &NSAPISetSettingsInt;
	if (_wcsicmp(buffer, _T("NSAPIWriteSettings")) == 0)
		return &NSAPIWriteSettings;
	if (_wcsicmp(buffer, _T("NSAPIReadSettings")) == 0)
		return &NSAPIReadSettings;
	if (_wcsicmp(buffer, _T("NSAPIRehash")) == 0)
		return &NSAPIRehash;
	if (_wcsicmp(buffer, _T("NSAPIDescribeCommand")) == 0)
		return &NSAPIDescribeCommand;
	if (_wcsicmp(buffer, _T("NSAPIGetAllCommandNames")) == 0)
		return &NSAPIGetAllCommandNames;
	if (_wcsicmp(buffer, _T("NSAPIReleaseAllCommandNamessBuffer")) == 0)
		return &NSAPIReleaseAllCommandNamessBuffer;
	if (_wcsicmp(buffer, _T("NSAPIRegisterCommand")) == 0)
		return &NSAPIRegisterCommand;

	return NULL;
}
