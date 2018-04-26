/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "CheckExternalScripts.h"
#include "extscr_cli.h"
#include "script_provider.hpp"

#include <process/execute_process.hpp>

#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_protobuf_nagios.hpp>
#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/nscapi_protobuf.hpp>

#include <config.h>
#include <str/utils.hpp>
#include <str/format.hpp>
#include <file_helpers.hpp>

#include <boost/regex.hpp>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>

namespace sh = nscapi::settings_helper;

CheckExternalScripts::CheckExternalScripts() 
{}
CheckExternalScripts::~CheckExternalScripts() {}

void CheckExternalScripts::addAllScriptsFrom(std::string str_path) {
	std::string pattern = "*.*";
	boost::filesystem::path path(str_path);
	if (!boost::filesystem::is_directory(path)) {
		if (path.has_relative_path())
			path = get_base_path() / path;
		if (!boost::filesystem::is_directory(path)) {
			file_helpers::patterns::pattern_type split_path = file_helpers::patterns::split_pattern(path);
			if (!boost::filesystem::is_directory(split_path.first)) {
				NSC_LOG_ERROR_STD("Path was not found: " + split_path.first.string());
				return;
			}
			path = split_path.first;
			pattern = split_path.second.string();
		}
	}
	NSC_DEBUG_MSG("Using script path: " + path.string());
	boost::regex re;
	try {
		std::string pre = file_helpers::patterns::glob_to_regexp(pattern);
		NSC_DEBUG_MSG("Using regexp: " + pre);
		re = pre;
	} catch (std::exception &e) {
		NSC_LOG_ERROR_EXR("Invalid pattern: " + pattern, e);
		return;
	}
	boost::filesystem::directory_iterator end_itr;
	for (boost::filesystem::directory_iterator itr(path); itr != end_itr; ++itr) {
		if (!is_directory(itr->status())) {
			std::string name = file_helpers::meta::get_filename(itr->path());
			std::string cmd = itr->path().string();
			if (allowArgs_) {
				cmd += " %ARGS%";
			}
			if (regex_match(name, re))
				add_command(name, cmd);
		}
	}
}

bool CheckExternalScripts::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode) {
	try {
		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias(alias, "external scripts");

		aliases_.set_path(settings.alias().get_settings_path("alias"));
		std::string wrappings_path = settings.alias().get_settings_path("wrappings");
		boost::filesystem::path scriptRoot;
		std::string scriptDirectory;
		std::map<std::string, std::string> wrappings;

		settings.alias().add_path_to_settings()

			("wrappings", sh::string_map_path(&wrappings)
				, "Script wrappings", "A list of templates for defining script commands.\nEnter any command line here and they will be expanded by scripts placed under the wrapped scripts section. %SCRIPT% will be replaced by the actual script an %ARGS% will be replaced by any given arguments.",
				"WRAPPING", "An external script wrapping")

			("alias", sh::fun_values_path(boost::bind(&CheckExternalScripts::add_alias, this, _1, _2)),
				"Command aliases", "A list of aliases for already defined commands (with arguments).\n"
				"An alias is an internal command that has been predefined to provide a single command without arguments. Be careful so you don't create loops (ie check_loop=check_a, check_a=check_loop)",
				"ALIAS", "Query alias")

			;

		settings.register_all();
		settings.notify();
		settings.clear();

		if (wrappings.find("ps1") == wrappings.end()) {
			wrappings["ps1"] = "cmd /c echo If (-Not (Test-Path \"scripts\\%SCRIPT%\") ) { Write-Host \"UNKNOWN: Script `\"%SCRIPT%`\" not found.\"; exit(3) }; scripts\\%SCRIPT% $ARGS$; exit($lastexitcode) | powershell.exe /noprofile -command -";
			settings.register_key(wrappings_path, "ps1", "POWERSHELL WRAPPING", "Command line used for executing wrapped ps1 (powershell) scripts", "cmd /c echo If (-Not (Test-Path \"scripts\\%SCRIPT%\") ) { Write-Host \"UNKNOWN: Script `\"%SCRIPT%`\" not found.\"; exit(3) }; scripts\\%SCRIPT% $ARGS$; exit($lastexitcode) | powershell.exe /noprofile -command -", false);
			settings.set_static_key(wrappings_path, "ps1", wrappings["ps1"]);
		}
		if (wrappings.find("vbs") == wrappings.end()) {
			wrappings["vbs"] = "cscript.exe //T:30 //NoLogo scripts\\\\lib\\\\wrapper.vbs %SCRIPT% %ARGS%";
			settings.register_key(wrappings_path, "vbs", "Visual basic script", "Command line used for wrapped vbs scripts", "cscript.exe //T:30 //NoLogo scripts\\\\lib\\\\wrapper.vbs %SCRIPT% %ARGS%", false);
			settings.set_static_key(wrappings_path, "vbs", wrappings["vbs"]);
		}
		if (wrappings.find("bat") == wrappings.end()) {
			wrappings["bat"] = "scripts\\\\%SCRIPT% %ARGS%";
			settings.register_key(wrappings_path, "bat", "Batch file", "Command used for executing wrapped batch files", "scripts\\\\%SCRIPT% %ARGS%", false);
			settings.set_static_key(wrappings_path, "bat", wrappings["bat"]);
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

		settings.alias().add_key_to_settings()
			("timeout", sh::uint_key(&timeout, 60),
				"Command timeout", "The maximum time in seconds that a command can execute. (if more then this execution will be aborted). NOTICE this only affects external commands not internal ones.")

			("allow arguments", sh::bool_key(&allowArgs_, false),
				"Allow arguments when executing external scripts", "This option determines whether or not the we will allow clients to specify arguments to commands that are executed.")

			("allow nasty characters", sh::bool_key(&allowNasty_, false),
				"Allow certain potentially dangerous characters in arguments", "This option determines whether or not the we will allow clients to specify nasty (as in |`&><'\"\\[]{}) characters in arguments.")

			("script path", sh::string_key(&scriptDirectory),
			"Load all scripts in a given folder", "Load all scripts in a given directory and use them as commands.")

			("script root", sh::path_key(&scriptRoot, "${scripts}"),
			"Script root folder", "Root path where all scripts are contained (You can not upload/download scripts outside this folder).")
			;

		settings.register_all();
		settings.notify();
		settings.clear();
		provider_.reset(new script_provider(get_id(), get_core(), settings.alias().get_settings_path("scripts"), scriptRoot, wrappings));


		settings.alias().add_path_to_settings()

			("External script settings", "General settings for the external scripts module (CheckExternalScripts).")

			("scripts", sh::fun_values_path(boost::bind(&CheckExternalScripts::add_command, this, _1, _2)),
			"External scripts", "A list of scripts available to run from the CheckExternalScripts module. Syntax is: `command=script arguments`",
			"SCRIPT", "For more configuration options add a dedicated section (if you add a new section you can customize the user and various other advanced features)")

			("wrapped scripts", sh::fun_values_path(boost::bind(&CheckExternalScripts::add_wrapping, this, _1, _2)),
			"Wrapped scripts", "A list of wrapped scripts (ie. script using a template mechanism).\nThe template used will be defined by the extension of the script. Thus a foo.ps1 will use the ps1 wrapping from the wrappings section.",
			"WRAPPED SCRIPT", "A wrapped script definitions")

			;


		settings.alias().add_templates()
			("scripts", "plus", "Add a simple script",
				"Add binding for a simple script",
				"{"
				"\"fields\": [ "
				" { \"id\": \"alias\",		\"title\" : \"Alias\",		\"type\" : \"input\",		\"desc\" : \"This will identify the command\"} , "
				" { \"id\": \"script\",		\"title\" : \"Script\",		\"type\" : \"data-choice\",	\"desc\" : \"The name of the script\",\"exec\" : \"CheckExternalScripts list --json\" } , "
				" { \"id\": \"args\",		\"title\" : \"Arguments\",	\"type\" : \"input\",		\"desc\" : \"Command line arguments for the script use $ARG1$ to specify arguments\" } , "
				" { \"id\": \"cmd\",		\"key\" : \"command\", \"title\" : \"A\",	\"type\" : \"hidden\",		\"desc\" : \"A\" } "
				" ], "
				"\"events\": { "
				"\"onSave\": \"(function (node) { node.save_path = self.path; var f = node.get_field('cmd'); f.key = node.get_field('alias').value(); var val = node.get_field('script').value(); if (node.get_field('args').value()) { val += ' ' + node.get_field('args').value(); }; f.value(val)})\""
				"}"
				"}")
			("alias", "plus", "Add an alias",
				"Add binding for an alias",
				"{"
				"\"fields\": [ "
				" { \"id\": \"alias\",		\"title\" : \"Alias\",		\"type\" : \"input\",		\"desc\" : \"This will identify the command\"} , "
				" { \"id\": \"command\",	\"title\" : \"Command\",	\"type\" : \"data-choice\",	\"desc\" : \"The name of the command to execute\",\"exec\" : \"CheckExternalScripts list --json --query\" } , "
				" { \"id\": \"args\",		\"title\" : \"Arguments\",	\"type\" : \"input\",		\"desc\" : \"Command line arguments for the command. use $ARG1$ to specify arguments\" } , "
				" { \"id\": \"cmd\",		\"key\" : \"command\", \"title\" : \"A\",	\"type\" : \"hidden\",		\"desc\" : \"A\" } "
				" ], "
				"\"events\": { "
				"\"onSave\": \"(function (node) { node.save_path = self.path; \nvar f = node.get_field('cmd'); \nf.key = node.get_field('alias').value(); \nvar val = node.get_field('command').value(); \nif (node.get_field('args').value()) { \nval += ' ' + node.get_field('args').value(); }; \nf.value(val)})\""
				"}"
				"}")
			;

		settings.register_all();
		settings.notify();

		if (!scriptDirectory.empty()) {
			addAllScriptsFrom(scriptDirectory);
		}

		aliases_.add_samples(get_settings_proxy());
		aliases_.add_missing(get_settings_proxy(), "default", "");

		root_ = get_base_path();

		nscapi::core_helper core(get_core(), get_id());
		BOOST_FOREACH(const boost::shared_ptr<alias::command_object> &o, aliases_.get_object_list()) {
			core.register_alias(o->get_alias(), "Alias for: " + o->command);
		}
	} catch (...) {
		NSC_LOG_ERROR_EX("loading");
		return false;
	}
	return true;
}
bool CheckExternalScripts::unloadModule() {
	process::kill_all();
	return true;
}

bool CheckExternalScripts::commandLineExec(const int target_mode, const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response, const Plugin::ExecuteRequestMessage &request_message) {
	std::string command = request.command();
	if (command == "ext-scr" && request.arguments_size() > 0)
		command = request.arguments(0);
	else if (command.empty() && target_mode == NSCAPI::target_module && request.arguments_size() > 0)
		command = request.arguments(0);
	else if (command.empty() && target_mode == NSCAPI::target_module)
		command = "help";
	try {
		if (command == "help") {
			nscapi::protobuf::functions::set_response_bad(*response, "Usage: nscp ext-scr [add|list|show|install|delete] --help");
			return true;
		} else {
			if (!provider_) {
				nscapi::protobuf::functions::set_response_bad(*response, "Failed to create provider");
			}
			extscr_cli client(provider_);
			return client.run(command, request, response);
		}
	} catch (const std::exception &e) {
		nscapi::protobuf::functions::set_response_bad(*response, "Error: " + utf8::utf8_from_native(e.what()));
		return true;
	} catch (...) {
		nscapi::protobuf::functions::set_response_bad(*response, "Error: ");
		return true;
	}
	return false;
}

void CheckExternalScripts::add_command(std::string key, std::string arg) {
	try {
		if (!provider_) {
			NSC_LOG_ERROR("Failed to add (no provider): " + key);
			return;
		}
		provider_->add_command(key, arg);
		if (arg.find("$ARG") != std::string::npos) {
			if (!allowArgs_) {
				NSC_DEBUG_MSG_STD("Detected a $ARG??$ expression with allowed arguments flag set to false (perhaps this is not the intent)");
			}
		}
		if (arg.find("%ARG") != std::string::npos) {
			if (!allowArgs_) {
				NSC_DEBUG_MSG_STD("Detected a %ARG??% expression with allowed arguments flag set to false (perhaps this is not the intent)");
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
		aliases_.add(get_settings_proxy(), key, arg);
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to add: " + key, e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to add: " + key);
	}
}


void CheckExternalScripts::add_wrapping(std::string key, std::string command) {
	if (!provider_) {
		NSC_LOG_ERROR("Failed to add: " + key);
		return;
	}
	add_command(key, provider_->generate_wrapped_command(command));
}

void CheckExternalScripts::query_fallback(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response, const Plugin::QueryRequestMessage &) {
	if (!provider_) {
		NSC_LOG_ERROR_STD("No provider found: " + request.command());
		nscapi::protobuf::functions::set_response_bad(*response, "No command or alias found matching: " + request.command());
		return;
	}
	commands::command_object_instance command_def = provider_->find_command(request.command());

	std::list<std::string> args;
	for (int i = 0; i < request.arguments_size(); ++i) {
		args.push_back(request.arguments(i));
	}
	if (command_def) {
		handle_command(*command_def, args, response);
		return;
	}
	alias::command_object_instance alias_def = aliases_.find_object(request.command());
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
		int i = 1;
		BOOST_FOREACH(const std::string &str, args) {
			if (!allowNasty_ && str.find_first_of(NASTY_METACHARS) != std::string::npos) {
				nscapi::protobuf::functions::set_response_bad(*response, "Request contained illegal characters set /settings/external scripts/allow nasty characters=true!");
				return;
			}
			str::utils::replace(cmdline, "$ARG" + str::xtos(i) + "$", str);
			str::utils::replace(cmdline, "%ARG" + str::xtos(i) + "%", str);
			str::format::append_list(all, str, " ");
			str::format::append_list(allesc, "\"" + str + "\"", " ");
			i++;
		}
		str::utils::replace(cmdline, "$ARGS$", all);
		str::utils::replace(cmdline, "%ARGS%", all);
		str::utils::replace(cmdline, "$ARGS\"$", allesc);
		str::utils::replace(cmdline, "%ARGS\"%", allesc);
	} else if (args.size() > 0) {
		NSC_LOG_ERROR_STD("Arguments not allowed in CheckExternalScripts set /settings/external scripts/allow arguments=true");
		nscapi::protobuf::functions::set_response_bad(*response, "Arguments not allowed see nsclient.log for details");
		return;
	}

	if (cmdline.find("$ARG") != std::string::npos) {
		NSC_DEBUG_MSG_STD("Possible missing argument in: " + cmdline);
	}
	if (cmdline.find("%ARG") != std::string::npos) {
		NSC_DEBUG_MSG_STD("Possible missing argument in: " + cmdline);
	}
	NSC_TRACE_ENABLED() {
		NSC_TRACE_MSG(cd.get_alias() + " command line: " + cmdline);
	}

	process::exec_arguments arg(root_, cmdline, timeout, cd.encoding, cd.session, cd.display, !cd.no_fork);
	if (!cd.user.empty()) {
		arg.user = cd.user;
		arg.domain = cd.domain;
		arg.password = cd.password;
	}
	arg.alias = cd.get_alias();
	arg.ignore_perf = cd.ignore_perf;
	arg.session = cd.session;
	arg.display = cd.display;
	std::string output;
	int result = process::execute_process(arg, output);
	NSC_TRACE_ENABLED() {
		NSC_TRACE_MSG(cd.get_alias() + " return code: " + str::xtos(result));
		NSC_TRACE_MSG(cd.get_alias() + " output: " + output);
	}
	if (!nscapi::plugin_helper::isNagiosReturnCode(result)) {
		nscapi::protobuf::functions::set_response_bad(*response, "The command (" + cd.get_alias() + ") returned an invalid return code: " + str::xtos(result));
		return;
	}
	std::string message, perf;
	std::string::size_type pos = output.find_last_not_of("\n\r ");
	if (pos != std::string::npos) {
		if (pos == output.size())
			output = output.substr(0, pos);
		else
			output = output.substr(0, pos + 1);
	}
	if (output.empty())
		output = "No output available from command (" + arg.alias + ").";

	if (!arg.ignore_perf) {
		pos = output.find('|');
		if (pos != std::string::npos) {
			::Plugin::QueryResponseMessage_Response_Line* line = response->add_lines();
			line->set_message(output.substr(0, pos));
			nscapi::protobuf::functions::parse_performance_data(line, output.substr(pos + 1));
		} else {
			response->add_lines()->set_message(output);
		}
	} else {
		response->add_lines()->set_message(output);
	}

	response->set_result(nscapi::protobuf::functions::nagios_status_to_gpb(result));
}

void CheckExternalScripts::handle_alias(const alias::command_object &cd, const std::list<std::string> &src_args, Plugin::QueryResponseMessage::Response *response) {
	std::list<std::string> args = cd.arguments;
	bool missing_args = false;
	BOOST_FOREACH(const std::string &s, src_args) {
		if (s == "help-pb") {
			std::stringstream ss;
			int i = 1;
			// TODO: CHange this top use protobuffer!
			bool found = true;
			while (found) {
				found = false;
				BOOST_FOREACH(std::string &arg, args) {
					if (arg.find("$ARG" + str::xtos(i++) + "$") != std::string::npos) {
						ss << "$ARG" << str::xtos(i++) << "$,false,," << arg << "\n";
						found = true;
					}
					if (arg.find("%ARG" + str::xtos(i++) + "%") != std::string::npos) {
						ss << "%ARG" << str::xtos(i++) << "%,false,," << arg << "\n";
						found = true;
					}
				}
			}
			nscapi::protobuf::functions::set_response_good(*response, ss.str());
			return;
		}
	}

	BOOST_FOREACH(std::string &arg, args) {
		int i = 1;
		BOOST_FOREACH(const std::string &str, src_args) {
			str::utils::replace(arg, "$ARG" + str::xtos(i) + "$", str);
			str::utils::replace(arg, "%ARG" + str::xtos(i) + "%", str);
			i++;
		}
		if (arg.find("$ARG") != std::string::npos)
			missing_args = true;
		if (arg.find("%ARG") != std::string::npos)
			missing_args = true;
	}
	if (missing_args) {
		NSC_DEBUG_MSG("Potential missing argument for: " + cd.get_alias());
	}
	std::string buffer;
	nscapi::core_helper ch(get_core(), get_id());
	if (!ch.simple_query(cd.command, args, buffer)) {
		nscapi::protobuf::functions::set_response_bad(*response, "Failed to execute: " + cd.get_alias());
		return;
	}
	Plugin::QueryResponseMessage tmp;
	tmp.ParseFromString(buffer);
	if (tmp.payload_size() != 1) {
		nscapi::protobuf::functions::set_response_bad(*response, "Invalid response from command: " + cd.get_alias());
		return;
	}
	response->CopyFrom(tmp.payload(0));
}

