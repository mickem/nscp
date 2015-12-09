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

#include <nscapi/functions.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/nscapi_settings_helper.hpp>

#include <nscapi/nscapi_protobuf.hpp>

#include <json_spirit.h>

#include <settings/config.hpp>

#include <file_helpers.hpp>

namespace sh = nscapi::settings_helper;

CheckExternalScripts::CheckExternalScripts() {}
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
			if (regex_match(name, re))
				add_command(name, itr->path().string());
		}
	}
}

bool CheckExternalScripts::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode) {
	try {
		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias(alias, "external scripts");

		commands_.set_path(settings.alias().get_settings_path("scripts"));
		aliases_.set_path(settings.alias().get_settings_path("alias"));
		std::string wrappings_path = settings.alias().get_settings_path("wrappings");

		settings.alias().add_path_to_settings()

			("wrappings", sh::string_map_path(&wrappings_)
				, "EXTERNAL SCRIPT WRAPPINGS SECTION", "A list of templates for wrapped scripts.\n%SCRIPT% will be replaced by the actual script an %ARGS% will be replaced by any given arguments.",
				"WRAPPING", "An external script wrapping")

			("alias", sh::fun_values_path(boost::bind(&CheckExternalScripts::add_alias, this, _1, _2)),
				"ALIAS SECTION", "A list of aliases available.\n"
				"An alias is an internal command that has been predefined to provide a single command without arguments. Be careful so you don't create loops (ie check_loop=check_a, check_a=check_loop)",
				"ALIAS", "Query alias")

			;

		settings.register_all();
		settings.notify();
		settings.clear();

		if (wrappings_.find("ps1") == wrappings_.end()) {
			wrappings_["ps1"] = "cmd /c echo If (-Not (Test-Path \"scripts\\%SCRIPT%\") ) { Write-Host \"UNKNOWN: Script `\"%SCRIPT%`\" not found.\"; exit(3) }; scripts\\%SCRIPT% $ARGS$; exit($lastexitcode) | powershell.exe /noprofile -command -";
			settings.register_key(wrappings_path, "ps1", NSCAPI::key_string, "POWERSHELL WRAPPING", "", "", false);
			settings.set_static_key(wrappings_path, "ps1", wrappings_["ps1"]);
		}
		if (wrappings_.find("vbs") == wrappings_.end()) {
			wrappings_["vbs"] = "cscript.exe //T:30 //NoLogo scripts\\\\lib\\\\wrapper.vbs %SCRIPT% %ARGS%";
			settings.register_key(wrappings_path, "vbs", NSCAPI::key_string, "VISUAL BASIC WRAPPING", "", "", false);
			settings.set_static_key(wrappings_path, "vbs", wrappings_["vbs"]);
		}
		if (wrappings_.find("bat") == wrappings_.end()) {
			wrappings_["bat"] = "scripts\\\\%SCRIPT% %ARGS%";
			settings.register_key(wrappings_path, "bat", NSCAPI::key_string, "BATCH FILE WRAPPING", "", "", false);
			settings.set_static_key(wrappings_path, "bat", wrappings_["bat"]);
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
				"SCRIPT", "For more configuration options add a dedicated section (if you add a new section you can customize the user and various other advanced features)")

			("wrapped scripts", sh::fun_values_path(boost::bind(&CheckExternalScripts::add_wrapping, this, _1, _2)),
				"WRAPPED SCRIPTS SECTION", "A list of wrapped scripts (ie. scruts using a template mechanism). The template used will be defined by the extension of the script.",
				"WRAPPED SCRIPT", "A wrapped script defenitions")

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

		std::string scripts_path = settings.alias().get_settings_path("scripts");
		std::string alias_path = settings.alias().get_settings_path("alias");

		commands_.add_samples(get_settings_proxy());
		commands_.add_missing(get_settings_proxy(), "default", "", true);

		aliases_.add_samples(get_settings_proxy());
		aliases_.add_missing(get_settings_proxy(), "default", "", true);

		if (!scriptDirectory_.empty()) {
			addAllScriptsFrom(scriptDirectory_);
		}
		root_ = get_base_path();

		nscapi::core_helper core(get_core(), get_id());
		BOOST_FOREACH(const boost::shared_ptr<commands::command_object> &o, commands_.get_object_list()) {
			core.register_alias(o->get_alias(), "External script: " + o->command);
		}
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
		if (command == "add")
			add_script(request, response);
		else if (command == "install")
			configure(request, response);
		else if (command == "list")
			list(request, response);
		else if (command == "help") {
			nscapi::protobuf::functions::set_response_bad(*response, "Usage: nscp ext-scr [add|list|install] --help");
		} else
			return false;
		return true;
	} catch (const std::exception &e) {
		nscapi::protobuf::functions::set_response_bad(*response, "Error: " + utf8::utf8_from_native(e.what()));
	} catch (...) {
		nscapi::protobuf::functions::set_response_bad(*response, "Error: ");
	}
	return false;
}

void CheckExternalScripts::list(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response) {
	namespace po = boost::program_options;
	namespace pf = nscapi::protobuf::functions;
	po::variables_map vm;
	po::options_description desc;
	bool json = false, query = false, lib = false;

	desc.add_options()
		("help", "Show help.")

		("json", po::bool_switch(&json),
			"Return the list in json format.")
		("query", po::bool_switch(&query),
			"List queries instead of scripts (for aliases).")
		("include-lib", po::bool_switch(&lib),
			"Do not ignore any lib folders.")

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
	std::string resp;
	json_spirit::Array data;

	if (query) {
		Plugin::RegistryRequestMessage rrm;
		Plugin::RegistryResponseMessage response;
		nscapi::protobuf::functions::create_simple_header(rrm.mutable_header());
		Plugin::RegistryRequestMessage::Request *payload = rrm.add_payload();
		payload->mutable_inventory()->set_fetch_all(true);
		payload->mutable_inventory()->add_type(Plugin::Registry_ItemType_QUERY);
		std::string pb_response;
		get_core()->registry_query(rrm.SerializeAsString(), pb_response);
		response.ParseFromString(pb_response);
		BOOST_FOREACH(const ::Plugin::RegistryResponseMessage_Response &p, response.payload()) {
			BOOST_FOREACH(const ::Plugin::RegistryResponseMessage_Response_Inventory &i, p.inventory()) {
				if (json) {
					json_spirit::Value v = i.name();
					data.push_back(v);
				} else {
					resp += i.name() + "\n";
				}
			}
		}
	} else {
		boost::filesystem::path dir = get_core()->expand_path("${scripts}");
		boost::filesystem::path rel = get_core()->expand_path("${base-path}");
		boost::filesystem::recursive_directory_iterator iter(dir), eod;
		BOOST_FOREACH(boost::filesystem::path const& i, std::make_pair(iter, eod)) {
			std::string s = i.string();
			if (boost::algorithm::starts_with(s, rel.string()))
				s = s.substr(rel.string().size());
			if (s.size() == 0)
				continue;
			if (s[0] == '\\' || s[0] == '/')
				s = s.substr(1);
			boost::filesystem::path clone = i.parent_path();
			if (boost::filesystem::is_regular_file(i) && !boost::algorithm::contains(clone.string(), "lib")) {
				if (json) {
					json_spirit::Value v = s;
					data.push_back(v);
				} else {
					resp += s + "\n";
				}

			}
		}
	}
	if (json)
		resp = json_spirit::write(data, json_spirit::raw_utf8);

	nscapi::protobuf::functions::set_response_good(*response, resp);
}
void CheckExternalScripts::add_script(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response) {
	namespace po = boost::program_options;
	namespace pf = nscapi::protobuf::functions;
	po::variables_map vm;
	po::options_description desc;
	std::string script, arguments, alias;
	bool wrapped = false, list = false;

	desc.add_options()
		("help", "Show help.")

		("script", po::value<std::string>(&script),
			"Script to add")

		("alias", po::value<std::string>(&alias),
			"Name of command to execute script (defaults to basename of script)")

		("arguments", po::value<std::string>(&arguments),
			"Arguments for script.")

		("list", po::bool_switch(&list),
			"List all scripts in the scripts folder.")

		("wrapped", po::bool_switch(&wrapped),
			"Add this to add a wrapped script such as ps1, vbs or similar..")

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
	if (!wrapped) {
		if (!boost::filesystem::is_regular(file)) {
			nscapi::protobuf::functions::set_response_bad(*response, "Script not found: " + file.string());
			return;
		}
	}
	if (alias.empty()) {
		alias = boost::filesystem::basename(file.filename());
	}

	nscapi::protobuf::functions::settings_query s(get_id());
	if (!wrapped)
		s.set("/settings/external scripts/scripts", alias, script + " " + arguments);
	else
		s.set("/settings/external scripts/wrapped scripts", alias, script + " " + arguments);
	s.set(MAIN_MODULES_SECTION, "CheckExternalScripts", "enabled");
	s.save();
	get_core()->settings_query(s.request(), s.response());
	if (!s.validate_response()) {
		nscapi::protobuf::functions::set_response_bad(*response, s.get_response_error());
		return;
	}
	std::string actual = "";
	if (wrapped)
		actual = "\nActual command is: " + generate_wrapped_command(script + " " + arguments);
	nscapi::protobuf::functions::set_response_good(*response, "Added " + alias + " as " + script + actual);
}

void CheckExternalScripts::configure(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response) {
	namespace po = boost::program_options;
	namespace pf = nscapi::protobuf::functions;
	po::variables_map vm;
	po::options_description desc;
	std::string arguments = "false";
	const std::string path = "/settings/external scripts/server";

	pf::settings_query q(get_id());
	q.get(path, "allow arguments", false);
	q.get(path, "allow nasty characters", false);

	get_core()->settings_query(q.request(), q.response());
	if (!q.validate_response()) {
		nscapi::protobuf::functions::set_response_bad(*response, q.get_response_error());
		return;
	}
	BOOST_FOREACH(const pf::settings_query::key_values &val, q.get_query_key_response()) {
		if (val.path == path && val.key && *val.key == "allow arguments" && val.get_bool())
			arguments = "true";
		else if (val.path == path && val.key && *val.key == "allow nasty characters" && val.get_bool())
			arguments = "safe";
	}
	desc.add_options()
		("help", "Show help.")

		("arguments", po::value<std::string>(&arguments)->default_value(arguments)->implicit_value("safe"),
			"Allow arguments. false=don't allow, safe=allow non escape chars, all=allow all arguments.")

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
	std::stringstream result;

	nscapi::protobuf::functions::settings_query s(get_id());
	s.set(MAIN_MODULES_SECTION, "CheckExternalScripts", "enabled");
	if (arguments == "all" || arguments == "unsafe") {
		result << "UNSAFE Arguments are allowed." << std::endl;
		s.set(path, "allow arguments", "true");
		s.set(path, "allow nasty characters", "true");
	} else if (arguments == "safe" || arguments == "true") {
		result << "SAFE Arguments are allowed." << std::endl;
		s.set(path, "allow arguments", "true");
		s.set(path, "allow nasty characters", "false");
	} else {
		result << "Arguments are NOT allowed." << std::endl;
		s.set(path, "allow arguments", "false");
		s.set(path, "allow nasty characters", "false");
	}
	s.save();
	get_core()->settings_query(s.request(), s.response());
	if (!s.validate_response())
		nscapi::protobuf::functions::set_response_bad(*response, s.get_response_error());
	else
		nscapi::protobuf::functions::set_response_good(*response, result.str());
}

void CheckExternalScripts::add_command(std::string key, std::string arg) {
	try {
		commands_.add(get_settings_proxy(), key, arg, key == "default");
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
		aliases_.add(get_settings_proxy(), key, arg, key == "default");
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to add: " + key, e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to add: " + key);
	}
}

std::string CheckExternalScripts::generate_wrapped_command(std::string command) {
	strEx::s::token tok = strEx::s::getToken(command, ' ');
	std::string::size_type pos = tok.first.find_last_of(".");
	std::string type = "none";
	if (pos != std::wstring::npos)
		type = tok.first.substr(pos + 1);
	std::string tpl = wrappings_[type];
	if (tpl.empty()) {
		NSC_LOG_ERROR("Failed to find wrapping for type: " + type);
	} else {
		strEx::replace(tpl, "%SCRIPT%", tok.first);
		strEx::replace(tpl, "%ARGS%", tok.second);
		return tpl;
	}
	return "";
}
void CheckExternalScripts::add_wrapping(std::string key, std::string command) {
	add_command(key, generate_wrapped_command(command));
}

void CheckExternalScripts::query_fallback(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response, const Plugin::QueryRequestMessage &) {
	//nscapi::functions::decoded_simple_command_data data = nscapi::functions::parse_simple_query_request(char_command, request);

	commands::command_object_instance command_def = commands_.find_object(request.command());

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
			strEx::replace(cmdline, "$ARG" + strEx::s::xtos(i++) + "$", str);
			strEx::replace(cmdline, "%ARG" + strEx::s::xtos(i++) + "%", str);
			strEx::append_list(all, str, " ");
			strEx::append_list(allesc, "\"" + str + "\"", " ");
		}
		strEx::replace(cmdline, "$ARGS$", all);
		strEx::replace(cmdline, "%ARGS%", all);
		strEx::replace(cmdline, "$ARGS\"$", allesc);
		strEx::replace(cmdline, "%ARGS\"%", allesc);
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
	NSC_DEBUG_MSG("Command line: " + cmdline);

	process::exec_arguments arg(root_, cmdline, timeout, cd.encoding, cd.session, cd.display);
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
	if (!nscapi::plugin_helper::isNagiosReturnCode(result)) {
		nscapi::protobuf::functions::set_response_bad(*response, "The command (" + cd.get_alias() + ") returned an invalid return code: " + strEx::s::xtos(result));
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
					if (arg.find("$ARG" + strEx::s::xtos(i++) + "$") != std::string::npos) {
						ss << "$ARG" << strEx::s::xtos(i++) << "$,false,," << arg << "\n";
						found = true;
					}
					if (arg.find("%ARG" + strEx::s::xtos(i++) + "%") != std::string::npos) {
						ss << "%ARG" << strEx::s::xtos(i++) << "%,false,," << arg << "\n";
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
			strEx::replace(arg, "$ARG" + strEx::s::xtos(i++) + "$", str);
			strEx::replace(arg, "%ARG" + strEx::s::xtos(i++) + "%", str);
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