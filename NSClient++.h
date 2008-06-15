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

#include <config.h>
#include <ServiceCmd.h>
#include <NTService.h>
#include "NSCPlugin.h"
#include <Mutex.h>
#include <NSCAPI.h>
#include <MutexRW.h>
#include <map>
#include <com_helpers.hpp>


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
	struct plugin_info_type {
		std::wstring dll;
		std::wstring name;
		std::wstring version;
		std::wstring description;
	};
	typedef std::list<plugin_info_type> plugin_info_list;
private:
	typedef NSCPlugin* plugin_type;
	typedef std::vector<plugin_type> pluginList;
	typedef std::map<std::wstring,std::wstring> cmdMap;
	pluginList plugins_;
	pluginList commandHandlers_;
	pluginList messageHandlers_;
	std::wstring basePath;
	MutexHandler internalVariables;
	MutexHandler messageMutex;
	MutexRW  m_mutexRW;
	MutexRW  m_mutexRWcmdDescriptions;
	cmdMap cmdDescriptions_;
	typedef enum log_status {log_unknown, log_debug, log_nodebug };
	log_status debug_;
	bool boot_;
	com_helper::initialize_com com_helper_;


public:
	// c-tor, d-tor
	NSClientT(void) : debug_(log_unknown), boot_(true) {}
	virtual ~NSClientT(void) {}
	void enableDebug(bool debug = true) {
		if (debug)
			debug_ = log_debug;
		else
			debug_ = log_nodebug;
	}
	void setBoot(bool boot = true) {
		boot_ = boot;
	}

	// Service helper functions
	bool InitiateService();
	void TerminateService(void);
	static void WINAPI service_main_dispatch(DWORD dwArgc, LPTSTR *lpszArgv);
	static void WINAPI service_ctrl_dispatch(DWORD dwCtrlCode);

	// Member functions
	std::wstring getBasePath(void);
	NSCAPI::nagiosReturn injectRAW(const TCHAR* command, const unsigned int argLen, TCHAR **argument, TCHAR *returnMessageBuffer, unsigned int returnMessageBufferLen, TCHAR *returnPerfBuffer, unsigned int returnPerfBufferLen);
	NSCAPI::nagiosReturn NSClientT::inject(std::wstring command, std::wstring arguments, TCHAR splitter, bool escape, std::wstring &msg, std::wstring & perf);
//	std::wstring inject(const std::wstring buffer);
	std::wstring execute(std::wstring password, std::wstring cmd, std::list<std::wstring> args);
	void reportMessage(int msgType, const TCHAR* file, const int line, std::wstring message);
	int commandLineExec(const TCHAR* module, const TCHAR* command, const unsigned int argLen, TCHAR** args);

	void addPlugins(const std::list<std::wstring> plugins);
	plugin_type loadPlugin(const std::wstring plugin);
	void loadPlugins(void);
	void unloadPlugins(void);
	std::wstring describeCommand(std::wstring command);
	std::list<std::wstring> getAllCommandNames();
	void registerCommand(std::wstring cmd, std::wstring desc);
	unsigned int getBufferLength();
	void HandleSettingsCLI(TCHAR* arg, int argc, TCHAR* argv[]);

	bool logDebug();
	void listPlugins();
	plugin_info_list get_all_plugins();

private:
	plugin_type addPlugin(plugin_type plugin);
	void load_all_plugins(int mode);
};

typedef NTService<NSClientT> NSClient;


std::wstring Encrypt(std::wstring str, unsigned int algorithm = NSCAPI::xor);
std::wstring Decrypt(std::wstring str, unsigned int algorithm = NSCAPI::xor);

//////////////////////////////////////////////////////////////////////////
// Various NSAPI callback functions (available for plug-ins to make calls back to the core.
// <b>NOTICE</b> No threading is allowed so technically every thread is responsible for marshaling things back. 
// Though I think this is not the case at the moment.
//

LPVOID NSAPILoader(TCHAR*buffer);
NSCAPI::errorReturn NSAPIGetApplicationName(TCHAR*buffer, unsigned int bufLen);
NSCAPI::errorReturn NSAPIGetBasePath(TCHAR*buffer, unsigned int bufLen);
NSCAPI::errorReturn NSAPIGetApplicationVersionStr(TCHAR*buffer, unsigned int bufLen);
NSCAPI::errorReturn NSAPIGetSettingsString(const TCHAR* section, const TCHAR* key, const TCHAR* defaultValue, TCHAR* buffer, unsigned int bufLen);
int NSAPIGetSettingsInt(const TCHAR* section, const TCHAR* key, int defaultValue);
void NSAPIMessage(int msgType, const TCHAR* file, const int line, const TCHAR* message);
void NSAPIStopServer(void);
NSCAPI::nagiosReturn NSAPIInject(const TCHAR* command, const unsigned int argLen, TCHAR **argument, TCHAR *returnMessageBuffer, unsigned int returnMessageBufferLen, TCHAR *returnPerfBuffer, unsigned int returnPerfBufferLen);
NSCAPI::errorReturn NSAPIGetSettingsSection(const TCHAR*, TCHAR***, unsigned int *);
NSCAPI::errorReturn NSAPIReleaseSettingsSectionBuffer(TCHAR*** aBuffer, unsigned int * bufLen);
NSCAPI::boolReturn NSAPICheckLogMessages(int messageType);
NSCAPI::errorReturn NSAPIEncrypt(unsigned int algorithm, const TCHAR* inBuffer, unsigned int inBufLen, TCHAR* outBuf, unsigned int *outBufLen);
NSCAPI::errorReturn NSAPIDecrypt(unsigned int algorithm, const TCHAR* inBuffer, unsigned int inBufLen, TCHAR* outBuf, unsigned int *outBufLen);
NSCAPI::errorReturn NSAPISetSettingsString(const TCHAR* section, const TCHAR* key, const TCHAR* value);
NSCAPI::errorReturn NSAPISetSettingsInt(const TCHAR* section, const TCHAR* key, int value);
NSCAPI::errorReturn NSAPIWriteSettings(int type);
NSCAPI::errorReturn NSAPIReadSettings(int type);
NSCAPI::errorReturn NSAPIRehash(int flag);
NSCAPI::errorReturn NSAPIDescribeCommand(const TCHAR*,TCHAR*,unsigned int);
NSCAPI::errorReturn NSAPIGetAllCommandNames(TCHAR***, unsigned int *);
NSCAPI::errorReturn NSAPIReleaseAllCommandNamessBuffer(TCHAR***, unsigned int *);
NSCAPI::errorReturn NSAPIRegisterCommand(const TCHAR*,const TCHAR*);
NSCAPI::errorReturn NSAPISettingsAddKeyMapping(const TCHAR*, const TCHAR*, const TCHAR*, const TCHAR*);
NSCAPI::errorReturn NSAPISettingsAddPathMapping(const TCHAR*, const TCHAR*);
NSCAPI::errorReturn NSAPISettingsRegKey(const TCHAR*, const TCHAR*, int, const TCHAR*, const TCHAR*, const TCHAR*, int);
NSCAPI::errorReturn NSAPISettingsRegPath(const TCHAR*, const TCHAR*, const TCHAR*, int);
NSCAPI::errorReturn NSAPIGetPluginList(int*, NSCAPI::plugin_info*[]);
NSCAPI::errorReturn NSAPIReleasePluginList(int,NSCAPI::plugin_info*[]);
NSCAPI::errorReturn NSAPISettingsSave(void);



//////////////////////////////////////////////////////////////////////////
// Log macros to simplify logging
// Generally names are of the form LOG_<severity>[_STD] 
// Where _STD indicates that strings are force wrapped inside a std::wstring
//
#define LOG_ERROR_STD(msg) LOG_ERROR(((std::wstring)msg).c_str())
#define LOG_ERROR(msg) \
	NSAPIMessage(NSCAPI::error, _T(__FILE__), __LINE__, msg)
#define LOG_CRITICAL_STD(msg) LOG_CRITICAL(((std::wstring)msg).c_str())
#define LOG_CRITICAL(msg) \
	NSAPIMessage(NSCAPI::critical, _T(__FILE__), __LINE__, msg)
#define LOG_MESSAGE_STD(msg) LOG_MESSAGE(((std::wstring)msg).c_str())
#define LOG_MESSAGE(msg) \
	NSAPIMessage(NSCAPI::log, _T(__FILE__), __LINE__, msg)

#define LOG_DEBUG_STD(msg) LOG_DEBUG(((std::wstring)msg).c_str())
#define LOG_DEBUG(msg) \
	NSAPIMessage(NSCAPI::debug, _T(__FILE__), __LINE__, msg)
/*
#define LOG_DEBUG_STD(msg)
#define LOG_DEBUG(msg)
*/