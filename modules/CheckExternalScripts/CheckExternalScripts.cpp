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
#include "CheckExternalScripts.h"
#include <strEx.h>
#include <time.h>
#include <config.h>
#include <msvc_wrappers.h>
#include <file_helpers.hpp>

CheckExternalScripts gCheckExternalScripts;

BOOL APIENTRY DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	NSCModuleWrapper::wrapDllMain(hModule, ul_reason_for_call);
	return TRUE;
}

CheckExternalScripts::CheckExternalScripts() {}
CheckExternalScripts::~CheckExternalScripts() {}

void CheckExternalScripts::addAllScriptsFrom(std::wstring path) {
	file_helpers::patterns::pattern_type pattern = file_helpers::patterns::split_pattern(path);
	if (!file_helpers::checks::exists(pattern.first)) 
		pattern.first = NSCModuleHelper::getBasePath() + _T("\\") + pattern.first;
	if (!file_helpers::checks::exists(pattern.first))
		NSC_LOG_ERROR_STD(_T("Path was not found: ") + pattern.first);
/* TODO: do we need this?
	std::wstring::size_type pos = path.find_last_of('*');
	if (pos == std::wstring::npos) {
		path += _T("*.*");
	}
	*/
	WIN32_FIND_DATA wfd;
	std::wstring real_path = file_helpers::patterns::combine_pattern(pattern);
	HANDLE hFind = FindFirstFile(real_path.c_str(), &wfd);
	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			if ((wfd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) != FILE_ATTRIBUTE_DIRECTORY) {
				addCommand(wfd.cFileName, pattern.first + _T("\\") + wfd.cFileName, _T(""));
			}
		} while (FindNextFile(hFind, &wfd));
	} else {
		NSC_LOG_ERROR_STD(_T("No scripts found in path: ") + real_path);
		return;
	}
	FindClose(hFind);
}

bool CheckExternalScripts::loadModule() {
	timeout = NSCModuleHelper::getSettingsInt(EXTSCRIPT_SECTION_TITLE, EXTSCRIPT_SETTINGS_TIMEOUT ,EXTSCRIPT_SETTINGS_TIMEOUT_DEFAULT);
	scriptDirectory_ = NSCModuleHelper::getSettingsString(EXTSCRIPT_SECTION_TITLE, EXTSCRIPT_SETTINGS_SCRIPTDIR ,EXTSCRIPT_SETTINGS_SCRIPTDIR_DEFAULT);
	std::list<std::wstring>::const_iterator it;
	std::list<std::wstring> commands = NSCModuleHelper::getSettingsSection(EXTSCRIPT_SCRIPT_SECTION_TITLE);
	for (it = commands.begin(); it != commands.end(); ++it) {
		if ((*it).empty())
			continue;
		std::wstring s = NSCModuleHelper::getSettingsString(EXTSCRIPT_SCRIPT_SECTION_TITLE, (*it), _T(""));
		if (s.empty()) {
			NSC_LOG_ERROR_STD(_T("Invalid command definition: ") + (*it));
		} else {
			strEx::token tok = strEx::getToken(s, ' ', true);
			addCommand((*it).c_str(), tok.first, tok.second);
		}
	}

	commands = NSCModuleHelper::getSettingsSection(EXTSCRIPT_ALIAS_SECTION_TITLE);
	for (it = commands.begin(); it != commands.end(); ++it) {
		if ((*it).empty())
			continue;
		std::wstring s = NSCModuleHelper::getSettingsString(EXTSCRIPT_ALIAS_SECTION_TITLE, (*it), _T(""));
		if (s.empty()) {
			NSC_LOG_ERROR_STD(_T("Invalid command definition: ") + (*it));
		} else {
			strEx::token tok = strEx::getToken(s, ' ', true);
			addAlias((*it).c_str(), tok.first, tok.second);
		}
	}

	if (!scriptDirectory_.empty()) {
		addAllScriptsFrom(scriptDirectory_);
	}
	root_ = NSCModuleHelper::getBasePath();
	return true;
}
bool CheckExternalScripts::unloadModule() {
	return true;
}


bool CheckExternalScripts::hasCommandHandler() {
	return true;
}
bool CheckExternalScripts::hasMessageHandler() {
	return false;
}


NSCAPI::nagiosReturn CheckExternalScripts::handleCommand(const strEx::blindstr command, const unsigned int argLen, TCHAR **char_args, std::wstring &message, std::wstring &perf) {
	command_list::const_iterator cit = commands.find(command);
	bool isAlias = false;
	if (cit == commands.end()) {
		cit = alias.find(command);
		if (cit == alias.end())
			return NSCAPI::returnIgnored;
		isAlias = true;
	}

	const command_data cd = (*cit).second;
	std::wstring args = cd.arguments;
	if (isAlias || NSCModuleHelper::getSettingsInt(EXTSCRIPT_SECTION_TITLE, EXTSCRIPT_SETTINGS_ALLOW_ARGUMENTS, EXTSCRIPT_SETTINGS_ALLOW_ARGUMENTS_DEFAULT) == 1) {
		arrayBuffer::arrayList arr = arrayBuffer::arrayBuffer2list(argLen, char_args);
		arrayBuffer::arrayList::const_iterator cit2 = arr.begin();
		int i=1;

		for (;cit2!=arr.end();cit2++,i++) {
			if ((!isAlias) && (NSCModuleHelper::getSettingsInt(EXTSCRIPT_SECTION_TITLE, EXTSCRIPT_SETTINGS_ALLOW_NASTY_META, EXTSCRIPT_SETTINGS_ALLOW_NASTY_META_DEFAULT) == 0)) {
				if ((*cit2).find_first_of(NASTY_METACHARS) != std::wstring::npos) {
					NSC_LOG_ERROR(_T("Request string contained illegal metachars!"));
					return NSCAPI::returnIgnored;
				}
			}
			strEx::replace(args, _T("$ARG") + strEx::itos(i) + _T("$"), (*cit2));
		}
	}
	if (isAlias) {
		return NSCModuleHelper::InjectSplitAndCommand(cd.command, args, ' ', message, perf, true);
	} else {
		int result = process::executeProcess(root_, cd.command + _T(" ") + args, message, perf, timeout);
		if (!NSCHelper::isNagiosReturnCode(result)) {
			NSC_LOG_ERROR_STD(_T("The command (") + cd.command + _T(") returned an invalid return code: ") + strEx::itos(result));
			return NSCAPI::returnUNKNOWN;
		}
		return NSCHelper::int2nagios(result);
		/*
	} else if (cd.type == script_dir) {
		std::wstring args = arrayBuffer::arrayBuffer2string(char_args, argLen, _T(" "));
		std::wstring cmd = scriptDirectory_ + command.c_str() + _T(" ") +args;
		return executeNRPECommand(cmd, message, perf);
	} else {
		NSC_LOG_ERROR_STD(_T("Unknown script type: ") + command.c_str());
		return NSCAPI::critical;
		*/
	}

}


NSC_WRAPPERS_MAIN_DEF(gCheckExternalScripts);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gCheckExternalScripts);
NSC_WRAPPERS_HANDLE_CONFIGURATION(gCheckExternalScripts);


MODULE_SETTINGS_START(CheckExternalScripts, _T("NRPE Listener configuration"), _T("...")) 

PAGE(_T("NRPE Listsner configuration")) 

ITEM_EDIT_TEXT(_T("port"), _T("This is the port the CheckExternalScripts.dll will listen to.")) 
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
