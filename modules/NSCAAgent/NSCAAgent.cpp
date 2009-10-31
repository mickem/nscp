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

#include "stdafx.h"
#include "NSCAAgent.h"
#include <utils.h>
#include <list>
#include <string>

NSCAAgent gNSCAAgent;

/**
 * DLL Entry point
 * @param hModule 
 * @param ul_reason_for_call 
 * @param lpReserved 
 * @return 
 */
BOOL APIENTRY DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	NSCModuleWrapper::wrapDllMain(hModule, ul_reason_for_call);
	return TRUE;
}

/**
 * Default c-tor
 * @return 
 */
NSCAAgent::NSCAAgent() {}
/**
 * Default d-tor
 * @return 
 */
NSCAAgent::~NSCAAgent() {}
/**
 * Load (initiate) module.
 * Start the background collector thread and let it run until unloadModule() is called.
 * @return true
 */
bool NSCAAgent::loadModule(NSCAPI::moduleLoadMode mode) {
	try {

		if (SETTINGS_GET_BOOL(settings_def::COMPATIBLITY)) {
			NSC_DEBUG_MSG(_T("Using compatiblity mode in: NSCA module"));

#define NSCA_AGENT_SECTION_TITLE _T("NSCA Agent")
#define NSCA_CMD_SECTION_TITLE _T("NSCA Commands")

#define NSCA_INTERVAL _T("interval")
#define NSCA_HOSTNAME _T("hostname")
#define NSCA_SERVER _T("nsca_host")
#define NSCA_PORT _T("nsca_port")
#define NSCA_ENCRYPTION _T("encryption_method")
#define NSCA_PASSWORD _T("password")
#define NSCA_DEBUG_THREADS _T("debug_threads")
#define NSCA_CACHE_HOST _T("cache_hostname")

			SETTINGS_MAP_KEY_A(nsca::INTERVAL,		NSCA_AGENT_SECTION_TITLE, NSCA_INTERVAL);
			SETTINGS_MAP_KEY_A(nsca::HOSTNAME,		NSCA_AGENT_SECTION_TITLE, NSCA_HOSTNAME);
			SETTINGS_MAP_KEY_A(nsca::SERVER_HOST,	NSCA_AGENT_SECTION_TITLE, NSCA_SERVER);
			SETTINGS_MAP_KEY_A(nsca::SERVER_PORT,	NSCA_AGENT_SECTION_TITLE, NSCA_PORT);
			SETTINGS_MAP_KEY_A(nsca::ENCRYPTION,	NSCA_AGENT_SECTION_TITLE, NSCA_ENCRYPTION);
			SETTINGS_MAP_KEY_A(nsca::PASSWORD,		NSCA_AGENT_SECTION_TITLE, NSCA_PASSWORD);
			SETTINGS_MAP_KEY_A(nsca::THREADS,		NSCA_AGENT_SECTION_TITLE, NSCA_DEBUG_THREADS);
			SETTINGS_MAP_KEY_A(nsca::CACHE_HOST,	NSCA_AGENT_SECTION_TITLE, NSCA_CACHE_HOST);

			SETTINGS_MAP_SECTION_A(nsca::CMD_SECTION,	NSCA_CMD_SECTION_TITLE);
		}

		SETTINGS_REG_PATH(nsca::SECTION);
		SETTINGS_REG_PATH(nsca::SERVER_SECTION);
		SETTINGS_REG_PATH(nsca::CMD_SECTION);

		SETTINGS_REG_KEY_I(nsca::INTERVAL);
		SETTINGS_REG_KEY_S(nsca::HOSTNAME);
		SETTINGS_REG_KEY_S(nsca::SERVER_HOST);
		SETTINGS_REG_KEY_I(nsca::SERVER_PORT);
		SETTINGS_REG_KEY_I(nsca::ENCRYPTION);
		SETTINGS_REG_KEY_S(nsca::PASSWORD);
		SETTINGS_REG_KEY_I(nsca::THREADS);
		SETTINGS_REG_KEY_B(nsca::CACHE_HOST);


	} catch (NSCModuleHelper::NSCMHExcpetion &e) {
		NSC_LOG_ERROR_STD(_T("Failed to register command: ") + e.msg_);
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to register command."));
	}

	if (mode == NSCAPI::normalStart) {
		int e_threads = SETTINGS_GET_INT(nsca::THREADS);

		for (int i=0;i<e_threads;i++) {
			std::wstring id = _T("nsca_t_") + strEx::itos(i);
			NSCAThreadImpl *thread = new NSCAThreadImpl(id);
			extra_threads.push_back(thread);
		}
		for (std::list<NSCAThreadImpl*>::const_iterator cit=extra_threads.begin();cit != extra_threads.end(); ++cit) {
			(*cit)->createThread(reinterpret_cast<LPVOID>(rand()));
		}
	}
	
	return true;
}
/**
 * Unload (terminate) module.
 * Attempt to stop the background processing thread.
 * @return true if successfully, false if not (if not things might be bad)
 */
bool NSCAAgent::unloadModule() {
	/*
	if (!pdhThread.exitThread(20000)) {
		std::wcout << _T("MAJOR ERROR: Could not unload thread...") << std::endl;
		NSC_LOG_ERROR(_T("Could not exit the thread, memory leak and potential corruption may be the result..."));
	}
	*/
	for (std::list<NSCAThreadImpl*>::iterator it=extra_threads.begin();it != extra_threads.end(); ++it) {
		if (!(*it)->exitThread(20000)) {
			std::wcout << _T("MAJOR ERROR: Could not unload thread...") << std::endl;
			NSC_LOG_ERROR(_T("Could not exit the thread, memory leak and potential corruption may be the result..."));
		}
	}
	return true;
}
/**
 * Check if we have a command handler.
 * @return true (as we have a command handler)
 */
bool NSCAAgent::hasCommandHandler() {
	return false;
}
/**
 * Check if we have a message handler.
 * @return false as we have no message handler
 */
bool NSCAAgent::hasMessageHandler() {
	return false;
}

int NSCAAgent::commandLineExec(const TCHAR* command, const unsigned int argLen, TCHAR** args) {
	return -1;
}


/**
 * Main command parser and delegator.
 * This also handles a lot of the simpler responses (though some are deferred to other helper functions)
 *
 *
 * @param command 
 * @param argLen 
 * @param **args 
 * @return 
 */
NSCAPI::nagiosReturn NSCAAgent::handleCommand(const strEx::blindstr command, const unsigned int argLen, TCHAR **char_args, std::wstring &msg, std::wstring &perf) {
	return NSCAPI::returnIgnored;
}
std::wstring NSCAAgent::getCryptos() {
	std::wstring ret = _T("{");
	for (int i=0;i<LAST_ENCRYPTION_ID;i++) {
		if (nsca_encrypt::hasEncryption(i)) {
			std::wstring name;
			try {
				nsca_encrypt::any_encryption *core = nsca_encrypt::get_encryption_core(i);
				if (core == NULL)
					name = _T("Broken<NULL>");
				else
					name = core->getName();
			} catch (nsca_encrypt::encryption_exception &e) {
				name = e.getMessage();
			}
			if (ret.size() > 1)
				ret += _T(", ");
			ret += strEx::itos(i) + _T("=") + name;
		}
	}
	return ret + _T("}");
}


NSC_WRAPPERS_MAIN_DEF(gNSCAAgent);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gNSCAAgent);
NSC_WRAPPERS_HANDLE_CONFIGURATION(gNSCAAgent);
NSC_WRAPPERS_CLI_DEF(gNSCAAgent);



MODULE_SETTINGS_START(NSCAAgent, _T("NRPE Listener configuration"), _T("...")) 

PAGE(_T("NRPE Listsner configuration")) 

ITEM_EDIT_TEXT(_T("port"), _T("This is the port the NRPEListener.dll will listen to.")) 
ITEM_MAP_TO(_T("basic_ini_text_mapper")) 
OPTION(_T("section"), _T("NRPE")) 
OPTION(_T("key"), _T("port")) 
OPTION(_T("default"), _T("5666")) 
ITEM_END()

ITEM_CHECK_BOOL(_T("allow_arguments"), _T("This option determines whether or not the NRPE daemon will allow clients to specify arguments to commands that are executed.")) 
ITEM_MAP_TO(_T("basic_ini_bool_mapper")) 
OPTION(_T("section"), _T("NRPE")) 
OPTION(_T("key"), _T("allow_arguments")) 
OPTION(_T("default"), _T("false")) 
OPTION(_T("true_value"), _T("1")) 
OPTION(_T("false_value"), _T("0")) 
ITEM_END()

ITEM_CHECK_BOOL(_T("allow_nasty_meta_chars"), _T("This might have security implications (depending on what you do with the options)")) 
ITEM_MAP_TO(_T("basic_ini_bool_mapper")) 
OPTION(_T("section"), _T("NRPE")) 
OPTION(_T("key"), _T("allow_nasty_meta_chars")) 
OPTION(_T("default"), _T("false")) 
OPTION(_T("true_value"), _T("1")) 
OPTION(_T("false_value"), _T("0")) 
ITEM_END()

ITEM_CHECK_BOOL(_T("use_ssl"), _T("This option will enable SSL encryption on the NRPE data socket (this increases security somwhat.")) 
ITEM_MAP_TO(_T("basic_ini_bool_mapper")) 
OPTION(_T("section"), _T("NRPE")) 
OPTION(_T("key"), _T("use_ssl")) 
OPTION(_T("default"), _T("true")) 
OPTION(_T("true_value"), _T("1")) 
OPTION(_T("false_value"), _T("0")) 
ITEM_END()

PAGE_END()
ADVANCED_PAGE(_T("Access configuration")) 

ITEM_EDIT_OPTIONAL_LIST(_T("Allow connection from:"), _T("This is the hosts that will be allowed to poll performance data from the NRPE server.")) 
OPTION(_T("disabledCaption"), _T("Use global settings (defined previously)")) 
OPTION(_T("enabledCaption"), _T("Specify hosts for NRPE server")) 
OPTION(_T("listCaption"), _T("Add all IP addresses (not hosts) which should be able to connect:")) 
OPTION(_T("separator"), _T(",")) 
OPTION(_T("disabled"), _T("")) 
ITEM_MAP_TO(_T("basic_ini_text_mapper")) 
OPTION(_T("section"), _T("NRPE")) 
OPTION(_T("key"), _T("allowed_hosts")) 
OPTION(_T("default"), _T("")) 
ITEM_END()

PAGE_END()
MODULE_SETTINGS_END()
