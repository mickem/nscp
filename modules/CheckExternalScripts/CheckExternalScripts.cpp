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
#include <time.h>

#include <settings/macros.h>
#include <msvc_wrappers.h>
#include <config.h>
#include <strEx.h>
#include <file_helpers.hpp>
#include <file_helpers.hpp>

#include <boost/regex.hpp>
#include <boost/filesystem.hpp>


CheckExternalScripts gCheckExternalScripts;
#ifdef _WIN32
BOOL APIENTRY DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	NSCModuleWrapper::wrapDllMain(hModule, ul_reason_for_call);
	return TRUE;
}
#endif

CheckExternalScripts::CheckExternalScripts() {}
CheckExternalScripts::~CheckExternalScripts() {}

void CheckExternalScripts::addAllScriptsFrom(std::wstring str_path) {
	boost::filesystem::wpath path = str_path;
	if (path.has_relative_path())
		path = NSCModuleHelper::getBasePath() / path;
	file_helpers::patterns::pattern_type split_path = file_helpers::patterns::split_pattern(path);
	if (!boost::filesystem::is_directory(split_path.first))
		NSC_LOG_ERROR_STD(_T("Path was not found: ") + split_path.first.string());

	boost::wregex pattern(split_path.second);
	boost::filesystem::wdirectory_iterator end_itr; // default construction yields past-the-end
	for ( boost::filesystem::wdirectory_iterator itr( split_path.first ); itr != end_itr; ++itr ) {
		if ( !is_directory(itr->status()) ) {
			std::wstring name = itr->path().leaf();
			if (regex_match(name, pattern))
				addCommand(name, (split_path.first / name).string(), _T(""));
		}
	}
}

bool CheckExternalScripts::loadModule(NSCAPI::moduleLoadMode mode) {
	SETTINGS_REG_PATH(external_scripts::SECTION);
	SETTINGS_REG_PATH(external_scripts::SCRIPT_SECTION);
	SETTINGS_REG_PATH(external_scripts::ALIAS_SECTION);
	SETTINGS_REG_KEY_I(external_scripts::TIMEOUT);
	SETTINGS_REG_KEY_S(external_scripts::SCRIPT_PATH);
	SETTINGS_REG_KEY_B(external_scripts::ALLOW_ARGS);
	SETTINGS_REG_KEY_B(external_scripts::ALLOW_NASTY);

	timeout = SETTINGS_GET_INT(external_scripts::TIMEOUT);
	scriptDirectory_ = SETTINGS_GET_STRING(external_scripts::SCRIPT_PATH);
	allowArgs_ = SETTINGS_GET_BOOL(nrpe::ALLOW_ARGS);
	allowNasty_ = SETTINGS_GET_BOOL(nrpe::ALLOW_NASTY);
	std::list<std::wstring>::const_iterator it;
	std::list<std::wstring> commands = NSCModuleHelper::getSettingsSection(setting_keys::external_scripts::SCRIPT_SECTION_PATH);
	for (it = commands.begin(); it != commands.end(); ++it) {
		if ((*it).empty())
			continue;
		NSC_DEBUG_MSG_STD(_T("Looking under: ") + setting_keys::external_scripts::SCRIPT_SECTION_PATH + _T(", ") + (*it));
		std::wstring s = NSCModuleHelper::getSettingsString(setting_keys::external_scripts::SCRIPT_SECTION_PATH, (*it), _T(""));
		if (s.empty()) {
			NSC_LOG_ERROR_STD(_T("Invalid command definition: ") + (*it));
		} else {
			strEx::token tok = strEx::getToken(s, ' ', true);
			addCommand((*it).c_str(), tok.first, tok.second);
		}
	}

	commands = NSCModuleHelper::getSettingsSection(setting_keys::external_scripts::ALIAS_SECTION_PATH);
	for (it = commands.begin(); it != commands.end(); ++it) {
		if ((*it).empty())
			continue;
		std::wstring s = NSCModuleHelper::getSettingsString(setting_keys::external_scripts::ALIAS_SECTION_PATH, (*it), _T(""));
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


NSCAPI::nagiosReturn CheckExternalScripts::handleCommand(const std::wstring command, std::list<std::wstring> arguments, std::wstring &message, std::wstring &perf) {
	std::wstring cmd = command.c_str();
	boost::to_lower(cmd);
	command_list::const_iterator cit = commands.find(cmd);
	bool isAlias = false;
	if (cit == commands.end()) {
		cit = alias.find(cmd);
		if (cit == alias.end())
			return NSCAPI::returnIgnored;
		isAlias = true;
	}

	const command_data cd = (*cit).second;
	std::wstring args = cd.arguments;
	if (isAlias || allowArgs_) {
		int i=1;
		BOOST_FOREACH(wstring str, arguments) {
			if (isAlias || allowNasty_) {
				if (str.find_first_of(NASTY_METACHARS) != std::wstring::npos) {
					NSC_LOG_ERROR(_T("Request string contained illegal metachars!"));
					return NSCAPI::returnIgnored;
				}
			}
			strEx::replace(args, _T("$ARG") + strEx::itos(i) + _T("$"), str);
		}
	}
	if (isAlias) {
		return NSCModuleHelper::InjectSplitAndCommand(cd.command, args, ' ', message, perf, true);
	} else {
		int result = process::executeProcess(process::exec_arguments(root_, cd.command + _T(" ") + args, timeout), message, perf);
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
