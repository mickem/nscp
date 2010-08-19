
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
#ifdef WIN32
#include <com_helpers.hpp>
#endif

#include <types.hpp>
#include <config.h>
#include <service/system_service.hpp>

#include "NSCPlugin.h"
#include "commands.hpp"
#include "channels.hpp"
#include "logger.hpp"

//#include <nsclient_session.hpp>


class NSClientT;
typedef service_helper::impl<NSClientT>::system_service NSClient;

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
class NSClientT : public nsclient::logger /*: public nsclient_session::session_handler_interface*/ {

public:
	struct plugin_info_type {
		std::wstring dll;
		std::wstring name;
		std::wstring version;
		std::wstring description;
	};
	typedef std::list<plugin_info_type> plugin_info_list;
private:

	class NSException {
		std::wstring what_;
	public:
		NSException(std::wstring what) : what_(what){}
		std::wstring what() {
			return what_;
		}
	};
	struct cached_log_entry {
		cached_log_entry(int msgType_, std::wstring file_, int line_, std::wstring message_) 
			: msgType(msgType_),
			file(file_),
			line(line_),
			message(message_)
		{}
		int msgType;
		std::wstring file;
		int line;
		std::wstring message;
	};

	typedef boost::shared_ptr<NSCPlugin> plugin_type;
	typedef std::vector<plugin_type> pluginList;
	//typedef std::map<std::wstring,std::wstring> cmdMap;
	typedef std::list<cached_log_entry> log_cache_type;
	pluginList plugins_;
	pluginList messageHandlers_;
	boost::filesystem::wpath  basePath;
	boost::timed_mutex internalVariables;
	boost::timed_mutex messageMutex;
	boost::shared_mutex m_mutexRW;
	//boost::shared_mutex m_mutexRWcmdDescriptions;
	//cmdMap cmdDescriptions_;
	typedef enum log_status {log_unknown, log_looking, log_debug, log_nodebug };
	log_status debug_;
#ifdef WIN32
	com_helper::initialize_com com_helper_;
#endif
	/*
	std::auto_ptr<nsclient_session::shared_client_session> shared_client_;
	std::auto_ptr<nsclient_session::shared_server_session> shared_server_;
	*/
	log_cache_type log_cache_;
	bool plugins_loaded_;
	bool enable_shared_session_;
	nsclient::commands commands_;
	nsclient::channels channels_;
	unsigned int next_plugin_id_;


public:
	// c-tor, d-tor
	NSClientT(void) : debug_(log_unknown), plugins_loaded_(false), enable_shared_session_(false), commands_(this), channels_(this), next_plugin_id_(0) {}
	virtual ~NSClientT(void) {}
	void enableDebug(bool debug = true) {
		if (debug)
			debug_ = log_debug;
		else
			debug_ = log_nodebug;
	}

	// Service helper functions
	bool initCore(bool boot);
	bool exitCore(bool boot);
#ifdef WIN32x
	static void WINAPI service_main_dispatch(DWORD dwArgc, LPTSTR *lpszArgv);
	static void WINAPI service_ctrl_dispatch(DWORD dwCtrlCode);
	static DWORD WINAPI service_ctrl_dispatch_ex(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext);
#endif
	void service_on_session_changed(DWORD dwSessionId, bool logon, DWORD dwEventType);


	// Logger impl
	void nsclient_log_error(std::wstring file, int line, std::wstring error);

	// Service API
	static NSClient* get_global_instance();
	void handle_error(unsigned int line, wchar_t *file, std::wstring message) {
		reportMessage(NSCAPI::error, file, line, message);
	}
	void handle_startup();
	void handle_shutdown();
#ifdef _WIN32
	void handle_session_change(unsigned long dwSessionId, bool logon);
#endif


	// Member functions
	boost::filesystem::wpath getBasePath(void);
	NSCAPI::errorReturn send_notification(const wchar_t* channel, const wchar_t* command, NSCAPI::nagiosReturn code, char* result, unsigned int result_len);
	NSCAPI::nagiosReturn injectRAW(const wchar_t* command, std::string &request, std::string &response);
	NSCAPI::nagiosReturn inject(std::wstring command, std::wstring arguments, std::wstring &msg, std::wstring & perf);
//	std::wstring inject(const std::wstring buffer);
	std::wstring execute(std::wstring password, std::wstring cmd, std::list<std::wstring> args);
	void reportMessage(int msgType, const wchar_t* file, const int line, std::wstring message);
	int commandLineExec(const wchar_t* module, const unsigned int argLen, wchar_t** args);

	plugin_type loadPlugin(const boost::filesystem::wpath plugin, std::wstring alias);
	void loadPlugins(NSCAPI::moduleLoadMode mode);
	void unloadPlugins(bool unloadLoggers);
	std::wstring describeCommand(std::wstring command);
	std::list<std::wstring> getAllCommandNames();
	void registerCommand(unsigned int id, std::wstring cmd, std::wstring desc);
	unsigned int getBufferLength();
	void startTrayIcons();
	void startTrayIcon(DWORD dwSessionId);

	bool logDebug();
	void listPlugins();
	plugin_info_list get_all_plugins();
	std::list<std::wstring> list_commands();

	// Shared session interface:
	void session_error(std::wstring file, unsigned int line, std::wstring msg);
	void session_info(std::wstring file, unsigned int line, std::wstring msg);
	void session_log_message(int msgType, const wchar_t* file, const int line, std::wstring message) {
		reportMessage(msgType, file, line, message);
	}
	int session_inject(std::wstring command, std::wstring arguments, wchar_t splitter, bool escape, std::wstring &msg, std::wstring & perf) {
		return 0; // TODO: Readd this!!! inject(command, arguments, splitter, escape, msg, perf);
	}
	std::pair<std::wstring,std::wstring> session_get_name() {
		return std::pair<std::wstring,std::wstring>(SZAPPNAME,SZVERSION);
	}

	std::wstring expand_path(std::wstring file);


	public:
		void load_all_plugins(int mode);


	private:
		plugin_type addPlugin(plugin_type plugin);
};


extern NSClient mainClient;	// Global core instance forward declaration.


std::wstring Encrypt(std::wstring str, unsigned int algorithm = NSCAPI::encryption_xor);
std::wstring Decrypt(std::wstring str, unsigned int algorithm = NSCAPI::encryption_xor);

/*
#define LOG_DEBUG_STD(msg)
#define LOG_DEBUG(msg)
*/
