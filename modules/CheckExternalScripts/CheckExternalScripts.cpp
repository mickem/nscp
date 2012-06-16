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

#include <strEx.h>
#include <file_helpers.hpp>

#include <boost/regex.hpp>
#include <boost/filesystem.hpp>

#include <settings/client/settings_client.hpp>
#include <nscapi/functions.hpp>
#include <nscapi/nscapi_core_helper.hpp>

#include <config.h>

namespace sh = nscapi::settings_helper;

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

		commands_path = settings.alias().get_settings_path(_T("scripts"));
		aliases_path = settings.alias().get_settings_path(_T("alias"));
		std::wstring wrappings_path = settings.alias().get_settings_path(_T("wrappings"));

		settings.alias().add_path_to_settings()

			(_T("wrappings"), sh::wstring_map_path(&wrappings_)
			, _T("EXTERNAL SCRIPT WRAPPINGS SECTION"), _T("A list of templates for wrapped scripts"))

			(_T("alias"), sh::fun_values_path(boost::bind(&CheckExternalScripts::add_alias, this, _1, _2)), 
			_T("EXTERNAL SCRIPT ALIAS SECTION"), _T("A list of aliases available. An alias is an internal command that has been \"wrapped\" (to add arguments). Be careful so you don't create loops (ie check_loop=check_a, check_a=check_loop)"))

			;

		settings.register_all();
		settings.notify();
		settings.clear();

		if (wrappings_.empty()) {
			NSC_DEBUG_MSG(_T("No wrappings found (adding default: vbs, ps1 and bat)"));
			wrappings_[_T("vbs")] = _T("cscript.exe //T:30 //NoLogo scripts\\\\lib\\\\wrapper.vbs %SCRIPT% %ARGS%");
			wrappings_[_T("ps1")] = _T("cmd /c echo scripts\\\\%SCRIPT% %ARGS%; exit($lastexitcode) | powershell.exe -command -");
			wrappings_[_T("bat")] = _T("scripts\\\\%SCRIPT% %ARGS%");
			get_core()->settings_register_key(wrappings_path, _T("vbs"), NSCAPI::key_string, _T("VISUAL BASIC WRAPPING"), _T(""), wrappings_[_T("vbs")], false);
			get_core()->settings_register_key(wrappings_path, _T("ps1"), NSCAPI::key_string, _T("POWERSHELL WRAPPING"), _T(""), wrappings_[_T("ps1")], false);
			get_core()->settings_register_key(wrappings_path, _T("bat"), NSCAPI::key_string, _T("BATCH FILE WRAPPING"), _T(""), wrappings_[_T("bat")], false);
		}

		if (aliases_.empty()) {
			NSC_DEBUG_MSG(_T("No aliases found (adding default)"));

			add_alias(_T("alias_cpu"), _T("checkCPU warn=80 crit=90 time=5m time=1m time=30s"));
			add_alias(_T("alias_cpu_ex"), _T("checkCPU warn=$ARG1$ crit=$ARG2$ time=5m time=1m time=30s"));
			add_alias(_T("alias_mem"), _T("checkMem MaxWarn=80% MaxCrit=90% ShowAll=long type=physical type=virtual type=paged type=page"));
			add_alias(_T("alias_up"), _T("checkUpTime MinWarn=1d MinWarn=1h"));
			add_alias(_T("alias_disk"), _T("CheckDriveSize MinWarn=10% MinCrit=5% CheckAll FilterType=FIXED"));
			add_alias(_T("alias_disk_loose"), _T("CheckDriveSize MinWarn=10% MinCrit=5% CheckAll FilterType=FIXED ignore-unreadable"));
			add_alias(_T("alias_volumes"), _T("CheckDriveSize MinWarn=10% MinCrit=5% CheckAll=volumes FilterType=FIXED"));
			add_alias(_T("alias_volumes_loose"), _T("CheckDriveSize MinWarn=10% MinCrit=5% CheckAll=volumes FilterType=FIXED ignore-unreadable "));
			add_alias(_T("alias_service"), _T("checkServiceState CheckAll"));
			add_alias(_T("alias_service_ex"), _T("checkServiceState CheckAll \"exclude=Net Driver HPZ12\" \"exclude=Pml Driver HPZ12\" exclude=stisvc"));
			add_alias(_T("alias_process"), _T("checkProcState \"$ARG1$=started\""));
			add_alias(_T("alias_process_stopped"), _T("checkProcState \"$ARG1$=stopped\""));
			add_alias(_T("alias_process_count"), _T("checkProcState MaxWarnCount=$ARG2$ MaxCritCount=$ARG3$ \"$ARG1$=started\""));
			add_alias(_T("alias_process_hung"), _T("checkProcState MaxWarnCount=1 MaxCritCount=1 \"$ARG1$=hung\""));
			add_alias(_T("alias_event_log"), _T("CheckEventLog file=application file=system MaxWarn=1 MaxCrit=1 \"filter=generated gt -2d AND severity NOT IN ('success', 'informational') AND source != 'SideBySide'\" truncate=800 unique descriptions \"syntax=%severity%: %source%: %message% (%count%)\""));
			add_alias(_T("alias_file_size"), _T("CheckFiles \"filter=size > $ARG2$\" \"path=$ARG1$\" MaxWarn=1 MaxCrit=1 \"syntax=%filename% %size%\" max-dir-depth=10"));
			add_alias(_T("alias_file_age"), _T("checkFile2 filter=out \"file=$ARG1$\" filter-written=>1d MaxWarn=1 MaxCrit=1 \"syntax=%filename% %write%\""));
			add_alias(_T("alias_sched_all"), _T("CheckTaskSched \"filter=exit_code ne 0\" \"syntax=%title%: %exit_code%\" warn=>0"));
			add_alias(_T("alias_sched_long"), _T("CheckTaskSched \"filter=status = 'running' AND most_recent_run_time < -$ARG1$\" \"syntax=%title% (%most_recent_run_time%)\" warn=>0"));
			add_alias(_T("alias_sched_task"), _T("CheckTaskSched \"filter=title eq '$ARG1$' AND exit_code ne 0\" \"syntax=%title% (%most_recent_run_time%)\" warn=>0"));
			add_alias(_T("alias_updates"), _T("check_updates -warning 0 -critical 0"));
			add_alias(_T("check_ok"), _T("CheckOK Everything is fine!"));
		}

		settings.alias().add_path_to_settings()
			(_T("EXTERNAL SCRIPT SECTION"), _T("Section for external scripts configuration options (CheckExternalScripts)."))

			(_T("scripts"), sh::fun_values_path(boost::bind(&CheckExternalScripts::add_command, this, _1, _2)), 
			_T("EXTERNAL SCRIPT SCRIPT SECTION"), _T("A list of scripts available to run from the CheckExternalScripts module. Syntax is: <command>=<script> <arguments>"))

			(_T("wrapped scripts"), sh::fun_values_path(boost::bind(&CheckExternalScripts::add_wrapping, this, _1, _2)), 
			_T("EXTERNAL SCRIPT WRAPPED SCRIPTS SECTION"), _T("A list of wrappped scripts (ie. using the template mechanism)"))

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

		BOOST_FOREACH(const commands::command_handler::object_list_type::value_type &o, commands_.object_list) {
			NSC_DEBUG_MSG(_T("Registring command: ") + o.second.to_wstring());
			register_command(o.second.alias, _T("Alias for: ") + o.second.alias);
		}
		BOOST_FOREACH(const commands::command_handler::object_list_type::value_type &o, aliases_.object_list) {
			NSC_DEBUG_MSG(_T("Registring alias: ") + o.second.to_wstring());
			register_command(o.second.alias, _T("Alias for: ") + o.second.alias);
		}


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

void CheckExternalScripts::add_command(std::wstring key, std::wstring arg) {
	try {
		commands_.add(get_settings_proxy(), commands_path, key, arg, key == _T("default"));
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Failed to add command: ") + key + _T(", ") + utf8::to_unicode(e.what()));
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to add command: ") + key);
	}
}
void CheckExternalScripts::add_alias(std::wstring key, std::wstring arg) {
	try {
		aliases_.add(get_settings_proxy(), aliases_path, key, arg, key == _T("default"));
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Failed to add alias: ") + key + _T(", ") + utf8::to_unicode(e.what()));
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to add alias: ") + key);
	}
}


bool CheckExternalScripts::hasCommandHandler() {
	return true;
}
bool CheckExternalScripts::hasMessageHandler() {
	return false;
}

NSCAPI::nagiosReturn CheckExternalScripts::handleRAWCommand(const wchar_t* char_command, const std::string &request, std::string &response) {
	nscapi::functions::decoded_simple_command_data data = nscapi::functions::parse_simple_query_request(char_command, request);

	commands::optional_command_object cmd = commands_.find_object(data.command);
	bool isAlias = !cmd;
	if (!cmd)
		cmd = aliases_.find_object(data.command);
	if (!cmd)
		return NSCAPI::returnIgnored;

	const commands::command_object cd = *cmd;
	std::list<std::wstring> args = cd.arguments;
	bool first = true;
	if (isAlias || allowArgs_) {
		BOOST_FOREACH(std::wstring &arg, args) {
			int i=1;
			BOOST_FOREACH(std::wstring str, data.args) {
				if (first && !isAlias && !allowNasty_) {
					if (str.find_first_of(NASTY_METACHARS_W) != std::wstring::npos) {
						return nscapi::functions::create_simple_query_response_unknown(data.command, _T("Request contained illegal characters!"), _T(""), response);
					}
				}
				strEx::replace(arg, _T("$ARG") + strEx::itos(i++) + _T("$"), str);
			}
		}
	} else if (!allowArgs_ && data.args.size() > 0) {
		NSC_LOG_ERROR_STD(_T("Arguments not allowed in CheckExternalScripts set /settings/external scripts/allow arguments=true"))
	}


	std::wstring xargs;
	BOOST_FOREACH(std::wstring s, args) {
		if (!xargs.empty())
			xargs += _T(" ");
		xargs += s;
	}

	NSC_DEBUG_MSG(_T("Arguments: ") + xargs);


	if (isAlias) {
		std::wstring message;
		try {
			int result = nscapi::core_helper::simple_query(cd.command, args, response);
			if (result == NSCAPI::returnIgnored) {
				nscapi::functions::create_simple_query_response_unknown(data.command, _T("No handler for command: ") + cd.command, response);
				return NSCAPI::returnUNKNOWN;
			}
			return result;
		} catch (boost::escaped_list_error &e) {
			NSC_LOG_MESSAGE(_T("Failed to parse alias expression: ") + strEx::string_to_wstring(e.what()));
			NSC_LOG_MESSAGE(_T("We will now try parsing the old syntax instead..."));
			return nscapi::core_helper::simple_query(cd.command, args, response);
		}
	} else {
		std::wstring message, perf;
		process::exec_arguments args(root_, cd.command + _T(" ") + xargs, timeout);
		if (!cd.user.empty()) {
			args.user = cd.user;
			args.domain = cd.domain;
			args.password = cd.password;
		}
		int result = process::executeProcess(args, message, perf);
		if (!nscapi::plugin_helper::isNagiosReturnCode(result)) {
			nscapi::functions::create_simple_query_response_unknown(data.command, _T("The command (") + args.command + _T(") returned an invalid return code: ") + strEx::itos(result), _T(""), response);
			return NSCAPI::returnUNKNOWN;
		}
		nscapi::functions::create_simple_query_response(data.command, nscapi::plugin_helper::int2nagios(result), message, perf, response);
		return result;
	}
}
NSC_WRAP_DLL();
NSC_WRAPPERS_MAIN_DEF(CheckExternalScripts);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF();

