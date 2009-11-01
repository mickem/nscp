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
#include "DebugLogMetrics.h"
#include <strEx.h>
#include <time.h>
#include <config.h>
#include <msvc_wrappers.h>
#include <Psapi.h>

DebugLogMetrics gDebugLogMetrics;

BOOL APIENTRY DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	NSCModuleWrapper::wrapDllMain(hModule, ul_reason_for_call);
	return TRUE;
}

DebugLogMetrics::DebugLogMetrics() : pdhThread(_T("debugThread")) {}
DebugLogMetrics::~DebugLogMetrics() {}

bool DebugLogMetrics::loadModule(NSCAPI::moduleLoadMode mode) {
//	timeout = SETTINGS_GET_INT() NSCModuleHelper::getSettingsInt(EXTSCRIPT_SECTION_TITLE, EXTSCRIPT_SETTINGS_TIMEOUT ,EXTSCRIPT_SETTINGS_TIMEOUT_DEFAULT);
//	scriptDirectory_ = NSCModuleHelper::getSettingsString(EXTSCRIPT_SECTION_TITLE, EXTSCRIPT_SETTINGS_SCRIPTDIR ,EXTSCRIPT_SETTINGS_SCRIPTDIR_DEFAULT);
	root_ = NSCModuleHelper::getBasePath();
	pdhThread.createThread(_T("NSClient++"));
	return true;
}
bool DebugLogMetrics::unloadModule() {
	if (!pdhThread.exitThread(20000)) {
		std::wcout << _T("MAJOR ERROR: Could not unload thread...") << std::endl;
		NSC_LOG_ERROR(_T("Could not exit the thread, memory leak and potential corruption may be the result..."));
	}
	return true;
}


bool DebugLogMetrics::hasCommandHandler() {
	return true;
}
bool DebugLogMetrics::hasMessageHandler() {
	return false;
}
/*
void getMetricsForPid(DWORD pid) {
	HANDLE hProcModule = OpenProcess( PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid );
	if( !hProcModule ) {
		NSC_LOG_ERROR_STD(_T("Failed to open process: ") + error::lookup::last_error());
		return;
	}

	PROCESS_MEMORY_COUNTERS pmc;
	if (!GetProcessMemoryInfo( hProcModule, &pmc,  sizeof(pmc))) {
		NSC_LOG_ERROR_STD(_T("Failed to get process memory info: ") + error::lookup::last_error());
		CloseHandle(hProcModule);
		return;
	}
	NSC_LOG_MESSAGE_STD(_T("Memory: ") + strEx::itos(pmc.WorkingSetSize));
	
	CloseHandle( hProcModule );
}

void getMetricsForName(std::wstring name) {

}
*/

NSCAPI::nagiosReturn DebugLogMetrics::handleCommand(const strEx::blindstr command, const unsigned int argLen, TCHAR **char_args, std::wstring &message, std::wstring &perf) {
	if (pdhThread.hasActiveThread())
		pdhThread.getThread()->store(command.c_str());
	//getMetricsForPid(GetCurrentProcessId());
	return NSCAPI::returnIgnored;
}


NSC_WRAPPERS_MAIN_DEF(gDebugLogMetrics);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gDebugLogMetrics);
NSC_WRAPPERS_HANDLE_CONFIGURATION(gDebugLogMetrics);


MODULE_SETTINGS_START(DebugLogMetrics, _T("NRPE Listener configuration"), _T("...")) 

PAGE(_T("NRPE Listsner configuration")) 

ITEM_EDIT_TEXT(_T("port"), _T("This is the port the DebugLogMetrics.dll will listen to.")) 
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
