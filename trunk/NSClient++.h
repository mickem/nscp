#pragma once

#include "resource.h"
#include <ServiceCmd.h>
#include <NTService.h>
#include "NSCPlugin.h"
#include "TCPSocketResponder.h"
#include <Mutex.h>
#include <NSCAPI.h>


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
private:
	typedef NSCPlugin* plugin_type;
	typedef std::list<plugin_type> pluginList;
	pluginList plugins_;
	pluginList commandHandlers_;
	pluginList messageHandlers_;
	TCPSocketResponderThread socketThread;
	std::string basePath;
	MutexHandler messageMutex;

public:
	// c-tor, d-tor
	NSClientT(void) {}
	virtual ~NSClientT(void) {}

	// Service helper functions
	void InitiateService(void);
	void TerminateService(void);
	static void WINAPI service_main_dispatch(DWORD dwArgc, LPTSTR *lpszArgv);
	static void WINAPI service_ctrl_dispatch(DWORD dwCtrlCode);

	// Member functions
	static std::string getPassword(void);
	std::string getBasePath(void);
	std::string inject(std::string buffer);
	std::string execute(std::string password, std::string cmd, std::list<std::string> args);
	void reportMessage(int msgType, const char* file, const int line, std::string message);

	void loadPlugins(std::list<std::string> plugins);
	void loadPlugin(std::string plugin);
	void loadPlugins(void);
	void unloadPlugins(void);

private:
	void addPlugin(plugin_type plugin);

};

typedef NTService<NSClientT> NSClient;

//////////////////////////////////////////////////////////////////////////
// Various NSAPI callback functions (available for plug-ins to make calls back to the core.
// <b>NOTICE</b> No threading is allowed so technically every thread is responsible for marshaling things back. 
// Though I think this is not the case at the moment.
//

LPVOID NSAPILoader(char*buffer);
int NSAPIGetApplicationName(char*buffer, unsigned int bufLen);
int NSAPIGetBasePath(char*buffer, unsigned int bufLen);
int NSAPIGetApplicationVersionStr(char*buffer, unsigned int bufLen);
int NSAPIGetSettingsString(const char* section, const char* key, const char* defaultValue, char* buffer, unsigned int bufLen);
int NSAPIGetSettingsInt(const char* section, const char* key, int defaultValue);
void NSAPIMessage(int msgType, const char* file, const int line, const char* message);
void NSAPIStopServer(void);
int NSAPIInject(const char* command, char* buffer, unsigned int bufLen);

//////////////////////////////////////////////////////////////////////////
// Log macros to simplify logging
// Generally names are of the form LOG_<severity>[_STD] 
// Where _STD indicates that strings are force wrapped inside a std::string
//
#define LOG_ERROR_STD(msg) LOG_ERROR(((std::string)msg).c_str())
#define LOG_ERROR(msg) \
	NSAPIMessage(NSCAPI::error, __FILE__, __LINE__, msg)
#define LOG_CRITICAL_STD(msg) LOG_CRITICAL(((std::string)msg).c_str())
#define LOG_CRITICAL(msg) \
	NSAPIMessage(NSCAPI::critical, __FILE__, __LINE__, msg)
#define LOG_MESSAGE_STD(msg) LOG_MESSAGE(((std::string)msg).c_str())
#define LOG_MESSAGE(msg) \
	NSAPIMessage(NSCAPI::log, __FILE__, __LINE__, msg)
#ifdef _DEBUG
#define LOG_DEBUG_STD(msg) LOG_DEBUG(((std::string)msg).c_str())
#define LOG_DEBUG(msg) \
	NSAPIMessage(NSCAPI::debug, __FILE__, __LINE__, msg)
#else
#define LOG_DEBUG_STD(msg)
#define LOG_DEBUG(msg)
#endif

