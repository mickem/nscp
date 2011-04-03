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
#include <string>

#include <msvc_wrappers.h>
#include <strEx.h>
#include <file_helpers.hpp>

#include <boost/regex.hpp>
#include <boost/filesystem.hpp>

namespace sh = nscapi::settings_helper;


CheckExternalScripts gCheckExternalScripts;

CheckExternalScripts::CheckExternalScripts() {}
CheckExternalScripts::~CheckExternalScripts() {}

void CheckExternalScripts::addAllScriptsFrom(std::wstring str_path) {
	boost::filesystem::wpath path = str_path;
	if (path.has_relative_path())
		path = get_core()->getBasePath() / path;
	file_helpers::patterns::pattern_type split_path = file_helpers::patterns::split_pattern(path);
	if (!boost::filesystem::is_directory(split_path.first))
		NSC_LOG_ERROR_STD(_T("Path was not found: ") + split_path.first.string());

	boost::wregex pattern(split_path.second);
	boost::filesystem::wdirectory_iterator end_itr; // default construction yields past-the-end
	for ( boost::filesystem::wdirectory_iterator itr( split_path.first ); itr != end_itr; ++itr ) {
		if ( !is_directory(itr->status()) ) {
			std::wstring name = itr->path().leaf();
			if (regex_match(name, pattern))
				add_command(name.c_str(), (split_path.first / name).string());
		}
	}
}


/**
 * Load (initiate) module.
 * Start the background collector thread and let it run until unloadModule() is called.
 * @return true
 */
bool CheckExternalScripts::loadModule() {
	return false;
}



bool CheckExternalScripts::loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode) {
	try {

		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias(alias, _T("external scripts"));

		settings.alias().add_path_to_settings()
			(_T("EXTERNAL SCRIPT SECTION"), _T("Section for external scripts configuration options (CheckExternalScripts)."))

			(_T("scripts"), sh::fun_values_path(boost::bind(&CheckExternalScripts::add_command, this, _1, _2)), 
			_T("EXTERNAL SCRIPT SCRIPT SECTION"), _T("A list of scripts available to run from the CheckExternalScripts module. Syntax is: <command>=<script> <arguments>"))

			(_T("alias"), sh::fun_values_path(boost::bind(&CheckExternalScripts::add_alias, this, _1, _2)), 
			_T("EXTERNAL SCRIPT ALIAS SECTION"), _T("A list of aliases available. An alias is an internal command that has been \"wrapped\" (to add arguments). Be careful so you don't create loops (ie check_loop=check_a, check_a=check_loop)"))

			(_T("wrappings"), sh::wstring_map_path(&wrappings_)
			, _T("EXTERNAL SCRIPT WRAPPINGS SECTION"), _T(""))

			(_T("wrapped scripts"), sh::fun_values_path(boost::bind(&CheckExternalScripts::add_wrapping, this, _1, _2)), 
			_T("EXTERNAL SCRIPT WRAPPED SCRIPTS SECTION"), _T(""))
			;

		settings.alias().add_key_to_settings()
			(_T("timeout"), sh::uint_key(&timeout, 60),
			_T("COMMAND TIMEOUT"), _T("The maximum time in seconds that a command can execute. (if more then this execution will be aborted). NOTICE this only affects external commands not internal ones."))

			(_T("allow arguments"), sh::bool_key(&allowArgs_, false),
			_T("COMMAND ARGUMENT PROCESSING"), _T("This option determines whether or not the we will allow clients to specify arguments to commands that are executed."))

			(_T("allow nasty characters"), sh::bool_key(&allowNasty_, false),
			_T("COMMAND ALLOW NASTY META CHARS"), _T("This option determines whether or not the we will allow clients to specify nasty (as in |`&><'\"\\[]{}) characters in arguments."))

			(_T("script path"), sh::wstring_key(&scriptDirectory_),
			_T("SCRIPT DIRECTORY"), _T("Load all scripts in a directory and use them as commands. Probably dangerous but useful if you have loads of scripts :)"))
			;

		settings.register_all();
		settings.notify();

		if (!scriptDirectory_.empty()) {
			addAllScriptsFrom(scriptDirectory_);
		}
		root_ = get_core()->getBasePath();

// 	} catch (nrpe::server::nrpe_exception &e) {
// 		NSC_LOG_ERROR_STD(_T("Exception caught: ") + e.what());
// 		return false;
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Exception caught: <UNKNOWN EXCEPTION>"));
		return false;
	}
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

NSCAPI::nagiosReturn CheckExternalScripts::handleRAWCommand(const wchar_t* char_command, const std::string &request, std::string &response) {
	nscapi::functions::decoded_simple_command_data data = nscapi::functions::process_simple_command_request(char_command, request);

	command_list::const_iterator cit = commands.find(data.command);
	bool isAlias = false;
	if (cit == commands.end()) {
		cit = alias.find(data.command);
		if (cit == alias.end())
			return NSCAPI::returnIgnored;
		isAlias = true;
	}

	const command_data cd = (*cit).second;
	std::list<std::wstring> args = cd.arguments;
	bool first = true;
	if (isAlias || allowArgs_) {
		BOOST_FOREACH(std::wstring &arg, args) {
			int i=1;
			BOOST_FOREACH(std::wstring str, data.args) {
				if (first && !isAlias && !allowNasty_) {
					if (str.find_first_of(NASTY_METACHARS) != std::wstring::npos) {
						return nscapi::functions::process_simple_command_result(data.command, NSCAPI::returnUNKNOWN, _T("Request contained illegal characters!"), _T(""), response);
					}
				}
				strEx::replace(arg, _T("$ARG") + strEx::itos(i++) + _T("$"), str);
			}
		}
	}


	std::wstring xargs;
	BOOST_FOREACH(std::wstring s, args) {
		if (!xargs.empty())
			xargs += _T(" ");
		xargs += s;
	}

	NSC_LOG_MESSAGE(_T("Arguments: ") + xargs);


	if (isAlias) {
		std::wstring message;
		try {
			return GET_CORE()->InjectCommand(cd.command, args, response);
		} catch (boost::escaped_list_error &e) {
			NSC_LOG_MESSAGE(_T("Failed to parse alias expression: ") + strEx::string_to_wstring(e.what()));
			NSC_LOG_MESSAGE(_T("We will now try parsing the old syntax instead..."));
			return GET_CORE()->InjectCommand(cd.command, args, response);
		}
	} else {
		std::wstring message, perf;
		int result = process::executeProcess(process::exec_arguments(root_, cd.command + _T(" ") + xargs, timeout), message, perf);
		if (!nscapi::plugin_helper::isNagiosReturnCode(result)) {
			return nscapi::functions::process_simple_command_result(data.command, NSCAPI::returnUNKNOWN, _T("The command (") + cd.command + _T(") returned an invalid return code: ") + strEx::itos(result), _T(""), response);
		}
		return nscapi::functions::process_simple_command_result(data.command, nscapi::plugin_helper::int2nagios(result), message, perf, response);
	}
}
// 
// 
// 
// 	std::wstring msg, perf;
// 	NSCAPI::nagiosReturn ret = handleCommand(data.command, data.args, msg, perf);
// 	return nscapi::functions::process_simple_command_result(data.command, ret, msg, perf);
// }
// 
// NSCAPI::nagiosReturn CheckExternalScripts::handleCommand(const std::wstring command, std::list<std::wstring> arguments, std::wstring &message, std::wstring &perf) {
// 	command_list::const_iterator cit = commands.find(command);
// 	bool isAlias = false;
// 	if (cit == commands.end()) {
// 		cit = alias.find(command);
// 		if (cit == alias.end())
// 			return NSCAPI::returnIgnored;
// 		isAlias = true;
// 	}
// 
// 	const command_data cd = (*cit).second;
// 	std::wstring args = cd.arguments;
// 	if (isAlias || allowArgs_) {
// 		int i=1;
// 		BOOST_FOREACH(std::wstring str, arguments) {
// 			if (!isAlias && !allowNasty_) {
// 				if (str.find_first_of(NASTY_METACHARS) != std::wstring::npos) {
// 					NSC_LOG_ERROR(_T("Request string contained illegal metachars!"));
// 					return NSCAPI::returnIgnored;
// 				}
// 			}
// 			strEx::replace(args, _T("$ARG") + strEx::itos(i++) + _T("$"), str);
// 		}
// 	}
// 	if (isAlias) {
// 		try {
// 			return GET_CORE()->InjectSplitAndCommand(cd.command, args, ' ', message, perf, true);
// 		} catch (boost::escaped_list_error &e) {
// 			NSC_LOG_MESSAGE(_T("Failed to parse alias expression: ") + strEx::string_to_wstring(e.what()));
// 			NSC_LOG_MESSAGE(_T("We will now try parsing the old syntax instead..."));
// 			return GET_CORE()->InjectSplitAndCommand(cd.command, args, ' ', message, perf, false);
// 		}
// 	} else {
// 		int result = process::executeProcess(process::exec_arguments(root_, cd.command + _T(" ") + args, timeout), message, perf);
// 		if (!nscapi::plugin_helper::isNagiosReturnCode(result)) {
// 			NSC_LOG_ERROR_STD(_T("The command (") + cd.command + _T(") returned an invalid return code: ") + strEx::itos(result));
// 			return NSCAPI::returnUNKNOWN;
// 		}
// 		return nscapi::plugin_helper::int2nagios(result);
// 	}
// 
// }


NSC_WRAP_DLL();
NSC_WRAPPERS_MAIN_DEF(gCheckExternalScripts);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gCheckExternalScripts);

