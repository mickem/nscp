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
#include "Settings.h"
#include <charEx.h>
#include <Socket.h>

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
int main(int argc, TCHAR* argv[], TCHAR* envp[])
{
	int nRetCode = 0;

	if ( (argc > 1) && ((*argv[1] == '-') || (*argv[1] == '/')) ) {
		if ( _stricmp( "install", argv[1]+1 ) == 0 ) {
			g_bConsoleLog = true;
			try {
				serviceControll::Install(SZSERVICENAME, SZSERVICEDISPLAYNAME, SZDEPENDENCIES);
			} catch (const serviceControll::SCException& e) {
				LOG_MESSAGE_STD("Service installation failed: " + e.error_);
				return -1;
			}
			LOG_MESSAGE("Service installed!");
		} else if ( _stricmp( "uninstall", argv[1]+1 ) == 0 ) {
			g_bConsoleLog = true;
			try {
				serviceControll::Uninstall(SZSERVICENAME);
			} catch (const serviceControll::SCException& e) {
				LOG_MESSAGE_STD("Service deinstallation failed; " + e.error_);
				return -1;
			}
		} else if ( _stricmp( "start", argv[1]+1 ) == 0 ) {
			g_bConsoleLog = true;
			serviceControll::Start(SZSERVICENAME);
		} else if ( _stricmp( "stop", argv[1]+1 ) == 0 ) {
			g_bConsoleLog = true;
			serviceControll::Stop(SZSERVICENAME);
		} else if ( _stricmp( "about", argv[1]+1 ) == 0 ) {
			g_bConsoleLog = true;
			LOG_MESSAGE(SZAPPNAME " (C) Michael Medin");
			LOG_MESSAGE("Version " SZVERSION);
		} else if ( _stricmp( "version", argv[1]+1 ) == 0 ) {
			g_bConsoleLog = true;
			LOG_MESSAGE(SZAPPNAME " Version: " SZVERSION);
		} else if ( _stricmp( "test", argv[1]+1 ) == 0 ) {
#ifdef _DEBUG
			strEx::run_test_getToken();
			strEx::run_test_replace();
			charEx::run_test_getToken();
			arrayBuffer::run_testArrayBuffer();
#endif

			g_bConsoleLog = true;
			mainClient.InitiateService();
			LOG_MESSAGE("Enter command to inject or exit to terminate...");
			std::string s = "";
			std::cin >> s;
			while (s != "exit") {
				strEx::token t = strEx::getToken(s, ',');
				std::string msg, perf;
				NSCAPI::nagiosReturn ret = mainClient.inject(t.first, t.second, ',', msg, perf);
				if (perf.empty())
					std::cout << NSCHelper::translateReturn(ret) << ":" << msg << std::endl;
				else
					std::cout << NSCHelper::translateReturn(ret) << ":" << msg << "|" << perf << std::endl;
				std::cin >> s;
			}
			mainClient.TerminateService();
			LOG_MESSAGE("DONE!");

			return 0;
		} else {
			LOG_MESSAGE("Usage: -version, -about, -install, -uninstall, -start, -stop");
		}
		return nRetCode;
	}
	mainClient.StartServiceCtrlDispatcher();
	return nRetCode;
}

//////////////////////////////////////////////////////////////////////////
// Service functions

/**
 * Service control handler startup point.
 * When the program is started as a service this will be the entry point.
 */
void NSClientT::InitiateService(void) {
	Settings::getInstance()->setFile(getBasePath() + "NSC.ini");

	try {
		simpleSocket::WSAStartup();
	} catch (simpleSocket::SocketException e) {
		LOG_ERROR_STD("Uncaught exception: " + e.getMessage());
	}

	SettingsT::sectionList list = Settings::getInstance()->getSection("modules");
	for (SettingsT::sectionList::iterator it = list.begin(); it != list.end(); it++) {
		try {
			LOG_DEBUG_STD("Loading: " + getBasePath() + "modules\\" + (*it));
			loadPlugin(getBasePath() + "modules\\" + (*it));
		} catch(const NSPluginException& e) {
			LOG_ERROR_STD("Exception raised: " + e.error_ + " in module: " + e.file_);
		}
	}
}
/**
 * Service control handler termination point.
 * When the program is stopped as a service this will be the "exit point".
 */
void NSClientT::TerminateService(void) {
	try {
		mainClient.unloadPlugins();
		LOG_DEBUG("Plugins unloaded...");
	} catch(NSPluginException *e) {
		std::cout << "Exception raised: " << e->error_ << " in module: " << e->file_ << std::endl;;
	}
	try {
		simpleSocket::WSACleanup();
	} catch (simpleSocket::SocketException e) {
		LOG_ERROR_STD("Uncaught exception: " + e.getMessage());
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


/**
 * Load a list of plug-ins
 * @param plugins A list with plug-ins (DLL files) to load
 */
void NSClientT::loadPlugins(const std::list<std::string> plugins) {
	ReadLock readLock(&m_mutexRW, true, 10000);
	if (!readLock.IsLocked()) {
		LOG_ERROR("FATAL ERROR: Could not get read-mutex.");
		return;
	}
	/*
	MutexLock lock(pluginMutex);
	if (!lock.hasMutex()) {
		LOG_ERROR("FATAL ERROR: Could not get mutex.");
		return;
	}
	*/
	std::list<std::string>::const_iterator it;
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
			LOG_ERROR("FATAL ERROR: Could not get read-mutex.");
			return;
		}
		/*	MutexLock lock(pluginMutex,20000);
		if (!lock.hasMutex()) {
		LOG_ERROR("FATAL ERROR: Could not get mutex.");
		return;
		}
		*/
		commandHandlers_.clear();
		{
			/*		MutexLock lock(messageMutex,20000);
			if (!lock.hasMutex()) {
			LOG_ERROR("FATAL ERROR: Could not get mutex (we will now crash BTW).");
			} else {
			messageHandlers_.clear();
			}
			*/
			messageHandlers_.clear();
		}
	}
	{
		ReadLock readLock(&m_mutexRW, true, 10000);
		if (!readLock.IsLocked()) {
			LOG_ERROR("FATAL ERROR: Could not get read-mutex.");
			return;
		}
		for (pluginList::size_type i=plugins_.size();i>0;i--) {
			NSCPlugin *p = plugins_[i-1];
			LOG_DEBUG_STD("Unloading plugin: " + p->getName() + "...");
			p->unload();
		}
	}

	{
		WriteLock writeLock(&m_mutexRW, true, 10000);
		if (!writeLock.IsLocked()) {
			LOG_ERROR("FATAL ERROR: Could not get read-mutex.");
			return;
		}
		for (unsigned int i=plugins_.size();i>0;i--) {
			NSCPlugin *p = plugins_[i-1];
			plugins_[i-1] = NULL;
			delete p;
		}
		plugins_.clear();
	}
}
/**
 * Load a single plug-in using a DLL filename
 * @param file The DLL file
 */
void NSClientT::loadPlugin(const std::string file) {
	LOG_DEBUG_STD("Loading: " + file);
	addPlugin(new NSCPlugin(file));
}
/**
 * Load and add a plugin to various internal structures
 * @param *plugin The plug-ininstance to load. The pointer is managed by the 
 */
void NSClientT::addPlugin(plugin_type plugin) {
	{
		ReadLock readLock(&m_mutexRW, true, 5000);
		if (!readLock.IsLocked()) {
			LOG_ERROR("FATAL ERROR: Could not get read-mutex.");
			return;
		}
		plugin->load();
	}
	/*
	MutexLock lock(pluginMutex);
	if (!lock.hasMutex()) {
		LOG_ERROR("FATAL ERROR: Could not get mutex.");
		return;
	}
	*/

	{
		WriteLock writeLock(&m_mutexRW, true, 10000);
		if (!writeLock.IsLocked()) {
			LOG_ERROR("FATAL ERROR: Could not get read-mutex.");
			return;
		}
		// @todo Catch here and unload if we fail perhaps ?
		plugins_.insert(plugins_.end(), plugin);
		if (plugin->hasCommandHandler())
			commandHandlers_.insert(commandHandlers_.end(), plugin);
		if (plugin->hasMessageHandler())
			messageHandlers_.insert(messageHandlers_.end(), plugin);
	}

}

NSCAPI::nagiosReturn NSClientT::inject(std::string command, std::string arguments, char splitter, std::string &msg, std::string & perf) {
	unsigned int aLen = 0;
	char ** aBuf = arrayBuffer::split2arrayBuffer(arguments, splitter, aLen);
	char * mBuf = new char[1024];
	char * pBuf = new char[1024];
	NSCAPI::nagiosReturn ret = injectRAW(command.c_str(), aLen, aBuf, mBuf, 1023, pBuf, 1023);
	arrayBuffer::destroyArrayBuffer(aBuf, aLen);
	if ( (ret == NSCAPI::returnInvalidBufferLen) || (ret == NSCAPI::returnIgnored) )
		return ret;
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
NSCAPI::nagiosReturn NSClientT::injectRAW(const char* command, const unsigned int argLen, char **argument, char *returnMessageBuffer, unsigned int returnMessageBufferLen, char *returnPerfBuffer, unsigned int returnPerfBufferLen) {
	ReadLock readLock(&m_mutexRW, true, 5000);
	if (!readLock.IsLocked()) {
		LOG_ERROR("FATAL ERROR: Could not get read-mutex.");
		return NSCAPI::returnUNKNOWN;
	}
/*	MutexLock lock(pluginMutex);
	if (!lock.hasMutex()) {
		LOG_ERROR_STD("Failed to get mutex (" + strEx::itos(lock.getWaitResult()) + "), command ignored...");
		return NSCAPI::returnUNKNOWN;
	}
*/
	for (pluginList::size_type i = 0; i < commandHandlers_.size(); i++) {
		try {
			NSCAPI::nagiosReturn c = commandHandlers_[i]->handleCommand(command, argLen, argument, returnMessageBuffer, returnMessageBufferLen, returnPerfBuffer, returnPerfBufferLen);
			switch (c) {
				case NSCAPI::returnInvalidBufferLen:
					LOG_ERROR("Return buffer to small to handle this command.");
					return c;
				case NSCAPI::returnIgnored:
					break;
				case NSCAPI::returnOK:
				case NSCAPI::returnWARN:
				case NSCAPI::returnCRIT:
				case NSCAPI::returnUNKNOWN:
//					LOG_DEBUG_STD("Injected Result: " +(std::string) returnMessageBuffer);
//					LOG_DEBUG_STD("Injected Performance Result: " +(std::string) returnPerfBuffer);
					return c;
				default:
					LOG_ERROR_STD("Unknown error from handleCommand: " + strEx::itos(c));
					return c;
			}
		} catch(const NSPluginException& e) {
			LOG_ERROR_STD("Exception raised: " + e.error_ + " in module: " + e.file_);
			return NSCAPI::returnCRIT;
		}
	}
	LOG_MESSAGE_STD("No handler for command: " + command);
	return NSCAPI::returnIgnored;
}
/**
 * Report a message to all logging enabled modules.
 *
 * @param msgType Message type 
 * @param file Filename generally __FILE__
 * @param line  Line number, generally __LINE__
 * @param message The message as a human readable string.
 */
void NSClientT::reportMessage(int msgType, const char* file, const int line, std::string message) {
	ReadLock readLock(&m_mutexRW, true, 5000);
	if (!readLock.IsLocked()) {
		std::cout << "Message was lost as the core was locked..." << std::endl;
		return;
	}
	MutexLock lock(messageMutex);
	if (!lock.hasMutex()) {
		std::cout << "Message was lost as the core was locked..." << std::endl;
		std::cout << message << std::endl;
		return;
	}
	/*
	MutexLock lock(messageMutex);
	if (!lock.hasMutex()) {
		std::cout << "Message was lost as the core was locked..." << std::endl;
		std::cout << message << std::endl;
		return;
	}
	*/
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
		std::cout << k << " " << file << "(" << line << ") " << message << std::endl;
	}
	if (msgType == NSCAPI::debug) {
		typedef enum status {unknown, debug, nodebug };
		static status d = unknown;
		if (d == unknown) {
			if (Settings::getInstance()->getInt("log", "debug", 0) == 1)
				d = debug;
			else
				d = nodebug;
		}
		if (d == nodebug)
			return;
	}
	for (pluginList::size_type i = 0; i< messageHandlers_.size(); i++) {
		try {
			messageHandlers_[i]->handleMessage(msgType, file, line, message.c_str());
		} catch(const NSPluginException& e) {
			// Here we are pretty much fucked! (as logging this might cause a loop :)
			std::cout << "Caught: " << e.error_ << " when trying to log a message..." << std::endl;
			std::cout << "This is *really really* bad, now the world is about to end..." << std::endl;
		}
	}
}
std::string NSClientT::getBasePath(void) {
	MutexLock lock(internalVariables);
	if (!lock.hasMutex()) {
		LOG_ERROR("FATAL ERROR: Could not get mutex.");
		return "FATAL ERROR";
	}
	if (!basePath.empty())
		return basePath;
	char* buffer = new char[1024];
	GetModuleFileName(NULL, buffer, 1023);
	std::string path = buffer;
	std::string::size_type pos = path.rfind('\\');
	basePath = path.substr(0, pos) + "\\";
	delete [] buffer;
	Settings::getInstance()->setFile(basePath + "NSC.ini");
	return basePath;
}


NSCAPI::errorReturn NSAPIGetSettingsString(const char* section, const char* key, const char* defaultValue, char* buffer, unsigned int bufLen) {
	return NSCHelper::wrapReturnString(buffer, bufLen, Settings::getInstance()->getString(section, key, defaultValue), NSCAPI::isSuccess);
}
int NSAPIGetSettingsInt(const char* section, const char* key, int defaultValue) {
	return Settings::getInstance()->getInt(section, key, defaultValue);
}
NSCAPI::errorReturn NSAPIGetBasePath(char*buffer, unsigned int bufLen) {
	return NSCHelper::wrapReturnString(buffer, bufLen, mainClient.getBasePath(), NSCAPI::isSuccess);
}
NSCAPI::errorReturn NSAPIGetApplicationName(char*buffer, unsigned int bufLen) {
	return NSCHelper::wrapReturnString(buffer, bufLen, SZAPPNAME, NSCAPI::isSuccess);
}
NSCAPI::errorReturn NSAPIGetApplicationVersionStr(char*buffer, unsigned int bufLen) {
	return NSCHelper::wrapReturnString(buffer, bufLen, SZVERSION, NSCAPI::isSuccess);
}
void NSAPIMessage(int msgType, const char* file, const int line, const char* message) {
	mainClient.reportMessage(msgType, file, line, message);
}
void NSAPIStopServer(void) {
	serviceControll::Stop(SZSERVICENAME);
}
NSCAPI::nagiosReturn NSAPIInject(const char* command, const unsigned int argLen, char **argument, char *returnMessageBuffer, unsigned int returnMessageBufferLen, char *returnPerfBuffer, unsigned int returnPerfBufferLen) {
	return mainClient.injectRAW(command, argLen, argument, returnMessageBuffer, returnMessageBufferLen, returnPerfBuffer, returnPerfBufferLen);
}
NSCAPI::errorReturn NSAPIGetSettingsSection(const char* section, char*** aBuffer, unsigned int * bufLen) {
	unsigned int len = 0;
	*aBuffer = arrayBuffer::list2arrayBuffer(Settings::getInstance()->getSection(section), len);
	*bufLen = len;
	return NSCAPI::isSuccess;
}


LPVOID NSAPILoader(char*buffer) {
	if (stricmp(buffer, "NSAPIGetApplicationName") == 0)
		return &NSAPIGetApplicationName;
	if (stricmp(buffer, "NSAPIGetApplicationVersionStr") == 0)
		return &NSAPIGetApplicationVersionStr;
	if (stricmp(buffer, "NSAPIGetSettingsSection") == 0)
		return &NSAPIGetSettingsSection;
	if (stricmp(buffer, "NSAPIGetSettingsString") == 0)
		return &NSAPIGetSettingsString;
	if (stricmp(buffer, "NSAPIGetSettingsInt") == 0)
		return &NSAPIGetSettingsInt;
	if (stricmp(buffer, "NSAPIMessage") == 0)
		return &NSAPIMessage;
	if (stricmp(buffer, "NSAPIStopServer") == 0)
		return &NSAPIStopServer;
	if (stricmp(buffer, "NSAPIInject") == 0)
		return &NSAPIInject;
	if (stricmp(buffer, "NSAPIGetBasePath") == 0)
		return &NSAPIGetBasePath;
	return NULL;
}
