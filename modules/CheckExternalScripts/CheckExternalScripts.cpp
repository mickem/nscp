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
#include "CheckExternalScripts.h"
#include <time.h>
#include <string>

#include <strEx.h>
#include <file_helpers.hpp>
#include <common.hpp>

#include <boost/regex.hpp>
#include <boost/filesystem.hpp>

#include <settings/client/settings_client.hpp>
#include <nscapi/functions.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_program_options.hpp>

#include <file_helpers.hpp>

namespace sh = nscapi::settings_helper;

CheckExternalScripts::CheckExternalScripts() {}
CheckExternalScripts::~CheckExternalScripts() {}

void CheckExternalScripts::addAllScriptsFrom(std::string str_path) {
	boost::filesystem::path path(str_path);
	if (path.has_relative_path())
		path = get_base_path() / path;
	file_helpers::patterns::pattern_type split_path = file_helpers::patterns::split_pattern(path);
	if (!boost::filesystem::is_directory(split_path.first))
		NSC_LOG_ERROR_STD("Path was not found: " + split_path.first.string());

	boost::regex pattern(split_path.second.string());
	boost::filesystem::directory_iterator end_itr; // default construction yields past-the-end
	for ( boost::filesystem::directory_iterator itr( split_path.first ); itr != end_itr; ++itr ) {
		if ( !is_directory(itr->status()) ) {
			std::string name = file_helpers::meta::get_filename(itr->path());
			if (regex_match(name, pattern))
				add_command(name, (split_path.first / name).string());
		}
	}
}

bool CheckExternalScripts::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode) {
	try {

		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias(alias, "external scripts");

		commands_path = settings.alias().get_settings_path("scripts");
		aliases_path = settings.alias().get_settings_path("alias");
		std::string wrappings_path = settings.alias().get_settings_path("wrappings");

		settings.alias().add_path_to_settings()

			("wrappings", sh::string_map_path(&wrappings_)
			, "EXTERNAL SCRIPT WRAPPINGS SECTION", "A list of templates for wrapped scripts",
			"SCRIPT", "For more configuration options add a dedicated section")

			("alias", sh::fun_values_path(boost::bind(&CheckExternalScripts::add_alias, this, _1, _2)), 
			"ALIAS SECTION", "A list of aliases available.\n"
			"An alias is an internal command that has been \"wrapped\" (to add arguments). Be careful so you don't create loops (ie check_loop=check_a, check_a=check_loop)",
			"ALIAS", "For more configuration options add a dedicated section")

			;

		settings.register_all();
		settings.notify();
		settings.clear();

		if (wrappings_.empty()) {
			NSC_DEBUG_MSG("No wrappings found (adding default: vbs, ps1 and bat)");
			wrappings_["vbs"] = "cscript.exe //T:30 //NoLogo scripts\\\\lib\\\\wrapper.vbs %SCRIPT% %ARGS%";
			wrappings_["ps1"] = "cmd /c echo scripts\\\\%SCRIPT% %ARGS%; exit($lastexitcode) | powershell.exe -command -";
			wrappings_["bat"] = "scripts\\\\%SCRIPT% %ARGS%";
			settings.register_key(wrappings_path, "vbs", NSCAPI::key_string, "VISUAL BASIC WRAPPING", "", wrappings_["vbs"], false);
			settings.register_key(wrappings_path, "ps1", NSCAPI::key_string, "POWERSHELL WRAPPING", "", wrappings_["ps1"], false);
			settings.register_key(wrappings_path, "bat", NSCAPI::key_string, "BATCH FILE WRAPPING", "", wrappings_["bat"], false);
		}

		if (aliases_.empty()) {
			NSC_DEBUG_MSG("No aliases found (adding default)");

			add_alias("alias_cpu", "check_cpu");
			add_alias("alias_cpu_ex", "check_cpu \"warn=load > $ARG1$\" \"crit=load > $ARG2$\" time=5m time=1m time=30s");
			add_alias("alias_mem", "check_memory");
			add_alias("alias_up", "check_uptime");
			add_alias("alias_disk", "check_drivesize");
			add_alias("alias_disk_loose", "check_drivesize");
			add_alias("alias_volumes", "check_drivesize");
			add_alias("alias_volumes_loose", "check_drivesize");
			add_alias("alias_service", "check_service");
			add_alias("alias_service_ex", "check_service \"exclude=Net Driver HPZ12\" \"exclude=Pml Driver HPZ12\" exclude=stisvc");
			add_alias("alias_process", "check_process \"process=$ARG1$\" \"crit=state != 'started'\"");
			add_alias("alias_process_stopped", "check_process \"process=$ARG1$\" \"crit=state != 'stopped'\"");
			add_alias("alias_process_count", "check_process \"process=$ARG1$\" \"warn=count > $ARG2$\" \"crit=count > $ARG3$\"");
			add_alias("alias_process_hung", "check_process \"filter=is_hung\" \"crit=count>0\"");
			add_alias("alias_event_log", "check_eventlog");
			add_alias("alias_file_size", "check_files \"path=$ARG1$\" \"crit=size > $ARG2$\" \"top-syntax=${list}\" \"detail-syntax=${filename] ${size}\" max-dir-depth=10");
			add_alias("alias_file_age", "check_files \"path=$ARG1$\" \"crit=written > $ARG2$\" \"top-syntax=${list}\" \"detail-syntax=${filename] ${written}\" max-dir-depth=10");
			add_alias("alias_sched_all", "check_tasksched show-all \"syntax=${title}: ${exit_code}\" \"crit=exit_code ne 0\"");
			add_alias("alias_sched_long", "check_tasksched \"filter=status = 'running'\" \"detail-syntax=${title} (${most_recent_run_time})\" \"crit=most_recent_run_time < -$ARG1$\"");
			add_alias("alias_sched_task", "check_tasksched show-all \"filter=title eq '$ARG1$'\" \"detail-syntax=${title} (${exit_code})\" \"crit=exit_code ne 0\"");
//			add_alias("alias_updates", "check_updates -warning 0 -critical 0");
		}

		settings.alias().add_path_to_settings()
			("EXTERNAL SCRIPT SECTION", "Section for external scripts configuration options (CheckExternalScripts).")

			("scripts", sh::fun_values_path(boost::bind(&CheckExternalScripts::add_command, this, _1, _2)),
			"SCRIPT SECTION", "A list of scripts available to run from the CheckExternalScripts module. Syntax is: <command>=<script> <arguments>",
			"SCRIPT", "For more configuration options add a dedicated section")

			("wrapped scripts", sh::fun_values_path(boost::bind(&CheckExternalScripts::add_wrapping, this, _1, _2)), 
			"WRAPPED SCRIPTS SECTION", "A list of wrapped scripts (ie. using the template mechanism)",
			"WRAPPED SCRIPT", "For more configuration options add a dedicated section")

			;

		settings.alias().add_key_to_settings()
			("timeout", sh::uint_key(&timeout, 60),
			"COMMAND TIMEOUT", "The maximum time in seconds that a command can execute. (if more then this execution will be aborted). NOTICE this only affects external commands not internal ones.")

			("allow arguments", sh::bool_key(&allowArgs_, false),
			"COMMAND ARGUMENT PROCESSING", "This option determines whether or not the we will allow clients to specify arguments to commands that are executed.")

			("allow nasty characters", sh::bool_key(&allowNasty_, false),
			"COMMAND ALLOW NASTY META CHARS", "This option determines whether or not the we will allow clients to specify nasty (as in |`&><'\"\\[]{}) characters in arguments.")

			("script path", sh::string_key(&scriptDirectory_),
			"SCRIPT DIRECTORY", "Load all scripts in a directory and use them as commands. Probably dangerous but useful if you have loads of scripts :)")
			;

		settings.register_all();
		settings.notify();


		if (!scriptDirectory_.empty()) {
			addAllScriptsFrom(scriptDirectory_);
		}
		root_ = get_base_path();

		nscapi::core_helper::core_proxy core(get_core(), get_id());
		BOOST_FOREACH(const commands::command_handler::object_list_type::value_type &o, commands_.object_list) {
			core.register_command(o.second.tpl.alias, "Alias for: " + o.second.tpl.alias);
		}
		BOOST_FOREACH(const alias::command_handler::object_list_type::value_type &o, aliases_.object_list) {
			core.register_command(o.second.tpl.alias, "Alias for: " + o.second.tpl.alias);
		}
	} catch (...) {
		NSC_LOG_ERROR_EX("loading");
		return false;
	}
	return true;
}
bool CheckExternalScripts::unloadModule() {
	return true;
}


bool CheckExternalScripts::commandLineExec(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response, const Plugin::ExecuteRequestMessage &request_message) {
	try {
		if (request.arguments_size() > 0 && request.arguments(0) == "add")
			add_script(request, response);
		else if (request.arguments_size() > 0 && request.arguments(0) == "help") {

		} else 
			return false;
	} catch (const std::exception &e) {
		nscapi::protobuf::functions::set_response_bad(*response, "Error: " + utf8::utf8_from_native(e.what()));
	} catch (...) {
		nscapi::protobuf::functions::set_response_bad(*response, "Error: ");
	}
	return true;
}


void CheckExternalScripts::add_script(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response) {

	namespace po = boost::program_options;
	namespace pf = nscapi::protobuf::functions;
	po::variables_map vm;
	po::options_description desc;
	std::string script, arguments, alias;

	desc.add_options()
		("help", "Show help.")

		("script", po::value<std::string>(&script), 
		"Script to add")

		("alias", po::value<std::string>(&alias), 
		"Name of command to execute script (defaults to basename of script)")

		("arguments", po::value<std::string>(&arguments), 
		"Arguments for script.")

		;

	try {
		nscapi::program_options::basic_command_line_parser cmd(request);
		cmd.options(desc);

		po::parsed_options parsed = cmd.run();
		po::store(parsed, vm);
		po::notify(vm);
	} catch (const std::exception &e) {
			return nscapi::program_options::invalid_syntax(desc, request.command(), "Invalid command line: " + utf8::utf8_from_native(e.what()), *response);
	}

	if (vm.count("help")) {
		nscapi::protobuf::functions::set_response_good(*response, nscapi::program_options::help(desc));
		return;
	}
	boost::filesystem::path file = get_core()->expand_path(script);
	if (!boost::filesystem::is_regular(file)) {
		nscapi::protobuf::functions::set_response_bad(*response, "Script not found: " + file.string());
		return;
	}
	if (alias.empty()) {
		alias = boost::filesystem::basename(file.filename());
	}

	nscapi::protobuf::functions::settings_query s(get_id());
	s.set("/settings/external scripts/scripts", alias, script + " " + arguments);
	s.set("/modules", "CheckExternalScripts", "enabled");
	s.save();
	get_core()->settings_query(s.request(), s.response());
	if (!s.validate_response()) {
		nscapi::protobuf::functions::set_response_bad(*response, s.get_response_error());
		return;
	}
	nscapi::protobuf::functions::set_response_good(*response, "Added " + alias + " as " + script);
}


void CheckExternalScripts::add_command(std::string key, std::string arg) {
	try {
		commands_.add(get_settings_proxy(), commands_path, key, arg, key == "default");
		if (arg.find("$ARG") != std::string::npos) {
			if (!allowArgs_) {
				NSC_DEBUG_MSG_STD("Detected a $ARG??$ expression with allowed arguments flag set to false (perhaps this is not the intent)");
			}
		}
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to add: " + key, e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to add: " + key);
	}
}
void CheckExternalScripts::add_alias(std::string key, std::string arg) {
	try {
		aliases_.add(get_settings_proxy(), aliases_path, key, arg, key == "default");
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to add: " + key, e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to add: " + key);
	}
}


void CheckExternalScripts::query_fallback(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response, const Plugin::QueryRequestMessage &) {
		//nscapi::functions::decoded_simple_command_data data = nscapi::functions::parse_simple_query_request(char_command, request);

		commands::optional_command_object command_def = commands_.find_object(request.command());

		std::list<std::string> args;
		for (int i=0;i<request.arguments_size();++i) {
			args.push_back(request.arguments(i));
		}
		if (command_def) {
			handle_command(*command_def, args, response);
			return;
		}
		alias::optional_command_object alias_def = aliases_.find_object(request.command());
		if (alias_def) {
			handle_alias(*alias_def, args, response);
			return;
		}
		NSC_LOG_ERROR_STD("No command or alias found matching: " + request.command());
		nscapi::protobuf::functions::set_response_bad(*response, "No command or alias found matching: " + request.command());
	}



void CheckExternalScripts::handle_command(const commands::command_object &cd, const std::list<std::string> &args, Plugin::QueryResponseMessage::Response *response) {
	std::string cmdline = cd.command;
	std::string all, allesc;
	if (allowArgs_) {
		int i=1;
		BOOST_FOREACH(const std::string &str, args) {
			if (!allowNasty_ && str.find_first_of(NASTY_METACHARS) != std::string::npos) {
				nscapi::protobuf::functions::set_response_bad(*response, "Request contained illegal characters set /settings/external scripts/allow nasty characters=true!");
				return;
			}
			strEx::replace(cmdline, "$ARG" + strEx::s::xtos(i++) + "$", str);
			strEx::append_list(all, str, " ");
			strEx::append_list(allesc, "\"" + str + "\"", " ");
		}
		strEx::replace(cmdline, "$ARGS$", all);
		strEx::replace(cmdline, "$ARGS\"$", allesc);
	} else if (args.size() > 0) {
		NSC_LOG_ERROR_STD("Arguments not allowed in CheckExternalScripts set /settings/external scripts/allow arguments=true");
		nscapi::protobuf::functions::set_response_bad(*response, "Arguments not allowed see nsclient.log for details");
		return;
	}

	if (cmdline.find("$ARG") != std::string::npos) {
		NSC_DEBUG_MSG_STD("Possible missing argument in: " + cmdline);
	}
	NSC_DEBUG_MSG("Command line: " + cmdline);

	process::exec_arguments arg(root_, cmdline, timeout, cd.encoding);
	if (!cd.user.empty()) {
		arg.user = cd.user;
		arg.domain = cd.domain;
		arg.password = cd.password;
	}
	arg.alias = cd.tpl.alias;
	arg.ignore_perf = cd.ignore_perf;
	std::string output;
	int result = process::execute_process(arg, output);
	if (!nscapi::plugin_helper::isNagiosReturnCode(result)) {
		nscapi::protobuf::functions::set_response_bad(*response, "The command (" + cd.tpl.alias + ") returned an invalid return code: " + strEx::s::xtos(result));
		return;
	}
	std::string message, perf;
	std::string::size_type pos = output.find_last_not_of("\n\r ");
	if (pos != std::string::npos) {
		if (pos == output.size())
			output = output.substr(0,pos);
		else
			output = output.substr(0,pos+1);
	}
	if (output.empty())
		output = "No output available from command (" + cd.command + ").";

	if (!arg.ignore_perf) {
		pos = output.find('|');
		if (pos != std::string::npos) {
			response->set_message(output.substr(0, pos));
			nscapi::protobuf::functions::parse_performance_data(response, output.substr(pos+1));
		} else {
			response->set_message(output);
		}
	} else {
		response->set_message(output);
	}

	response->set_result(nscapi::protobuf::functions::nagios_status_to_gpb(result));
}

void CheckExternalScripts::handle_alias(const alias::command_object &cd, const std::list<std::string> &src_args, Plugin::QueryResponseMessage::Response *response) {
	std::list<std::string> args = cd.arguments;
	bool missing_args = false;
	BOOST_FOREACH(const std::string &s, src_args) {
		if (s == "help-csv") {
			std::stringstream ss;
			int i=1;
			bool found = true;
			while (found) {
				found = false;
				BOOST_FOREACH(std::string &arg, args) {
					if (arg.find("$ARG" + strEx::s::xtos(i++) + "$") != std::string::npos) {
						ss << "$ARG" << strEx::s::xtos(i++) << "$,false,," << arg << "\n";
						found = true;
					}
				}
			}
			response->set_result(::Plugin::Common_ResultCode_OK);
			response->set_message(ss.str());
			return;
		}
	}

	BOOST_FOREACH(std::string &arg, args) {
		int i=1;
		BOOST_FOREACH(const std::string &str, src_args) {
			strEx::replace(arg, "$ARG" + strEx::s::xtos(i++) + "$", str);
		}
		if (arg.find("$ARG") != std::string::npos)
			missing_args = true;
	}
	if (missing_args) {
		NSC_DEBUG_MSG("Potential missing argument for: " + cd.tpl.alias);
	}
	std::string buffer;
	int result = nscapi::core_helper::simple_query(cd.command, args, buffer);
	if (result == NSCAPI::returnIgnored) {
		nscapi::protobuf::functions::set_response_bad(*response, "No handler for command: " + cd.command);
		return;
	}
	Plugin::QueryResponseMessage tmp;
	tmp.ParseFromString(buffer);
	if (tmp.payload_size() != 1) {
		nscapi::protobuf::functions::set_response_bad(*response, "Invalid response from command: " + cd.command);
		return;
	}
	response->CopyFrom(tmp.payload(0));
}
