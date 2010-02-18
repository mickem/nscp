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

CheckExternalScripts::CheckExternalScripts() {}
CheckExternalScripts::~CheckExternalScripts() {}

void CheckExternalScripts::addAllScriptsFrom(std::wstring str_path) {
	boost::filesystem::wpath path = str_path;
	if (path.has_relative_path())
		path = GET_CORE()->getBasePath() / path;
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
	std::list<std::wstring> commands = GET_CORE()->getSettingsSection(setting_keys::external_scripts::SCRIPT_SECTION_PATH);
	for (it = commands.begin(); it != commands.end(); ++it) {
		if ((*it).empty())
			continue;
		NSC_DEBUG_MSG_STD(_T("Looking under: ") + setting_keys::external_scripts::SCRIPT_SECTION_PATH + _T(", ") + (*it));
		std::wstring s = GET_CORE()->getSettingsString(setting_keys::external_scripts::SCRIPT_SECTION_PATH, (*it), _T(""));
		if (s.empty()) {
			NSC_LOG_ERROR_STD(_T("Invalid command definition: ") + (*it));
		} else {
			strEx::token tok = strEx::getToken(s, ' ', true);
			addCommand((*it).c_str(), tok.first, tok.second);
		}
	}

	commands = GET_CORE()->getSettingsSection(setting_keys::external_scripts::ALIAS_SECTION_PATH);
	for (it = commands.begin(); it != commands.end(); ++it) {
		if ((*it).empty())
			continue;
		std::wstring s = GET_CORE()->getSettingsString(setting_keys::external_scripts::ALIAS_SECTION_PATH, (*it), _T(""));
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
	root_ = GET_CORE()->getBasePath();
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
		BOOST_FOREACH(std::wstring str, arguments) {
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
		return GET_CORE()->InjectSplitAndCommand(cd.command, args, ' ', message, perf, true);
	} else {
		int result = process::executeProcess(process::exec_arguments(root_, cd.command + _T(" ") + args, timeout), message, perf);
		if (!nscapi::plugin_helper::isNagiosReturnCode(result)) {
			NSC_LOG_ERROR_STD(_T("The command (") + cd.command + _T(") returned an invalid return code: ") + strEx::itos(result));
			return NSCAPI::returnUNKNOWN;
		}
		return nscapi::plugin_helper::int2nagios(result);
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


NSC_WRAP_DLL();
NSC_WRAPPERS_MAIN_DEF(gCheckExternalScripts);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gCheckExternalScripts);

