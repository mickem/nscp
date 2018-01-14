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

#include "extscr_cli.h"

#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/nscapi_settings_proxy.hpp>

#include <file_helpers.hpp>
#include <config.h>

#ifdef HAVE_JSON_SPIRIT
#include <json_spirit.h>
#endif

#include <boost/regex.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <string>
#include <fstream>
#include <streambuf>

namespace sh = nscapi::settings_helper;
namespace po = boost::program_options;
namespace pf = nscapi::protobuf::functions;
namespace npo = nscapi::program_options;



extscr_cli::extscr_cli(boost::shared_ptr<script_provider_interface> provider)
	: provider_(provider)
{
}


bool extscr_cli::run(std::string cmd, const Plugin::ExecuteRequestMessage_Request &request, Plugin::ExecuteResponseMessage_Response *response) {
	if (cmd == "add")
		add_script(request, response);
	else if (cmd == "install")
		configure(request, response);
	else if (cmd == "list")
		list(request, response);
	else if (cmd == "show")
		show(request, response);
	else if (cmd == "delete")
		delete_script(request, response);
	else
		return false;
	return true;
}

// nscapi::core_wrapper* extscr_cli::get_core() const {
// 	return core_;
// }
// boost::shared_ptr<nscapi::settings_proxy> extscr_cli::get_settings_proxy() {
// 	return boost::shared_ptr<nscapi::settings_proxy>(new nscapi::settings_proxy(get_id(), get_core()));
// }

bool extscr_cli::validate_sandbox(boost::filesystem::path pscript, Plugin::ExecuteResponseMessage::Response *response) {
	boost::filesystem::path path = provider_->get_root();
	if (!file_helpers::checks::path_contains_file(path, pscript)) {
		nscapi::protobuf::functions::set_response_bad(*response, "Not allowed outside: " + path.string());
		return false;
	}
	return true;
}

void extscr_cli::list(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response) {
	po::variables_map vm;
	po::options_description desc;
	bool json = false, query = false, lib = false;

	desc.add_options()
		("help", "Show help.")
#ifdef HAVE_JSON_SPIRIT
		("json", po::bool_switch(&json),
			"Return the list in json format.")
#endif
		("query", po::bool_switch(&query),
			"List queries instead of scripts (for aliases).")
		("include-lib", po::bool_switch(&lib),
			"Do not ignore any lib folders.")

		;

	try {
		npo::basic_command_line_parser cmd(request);
		cmd.options(desc);

		po::parsed_options parsed = cmd.run();
		po::store(parsed, vm);
		po::notify(vm);
	} catch (const std::exception &e) {
		return npo::invalid_syntax(desc, request.command(), "Invalid command line: " + utf8::utf8_from_native(e.what()), *response);
	}

	if (vm.count("help")) {
		nscapi::protobuf::functions::set_response_good(*response, npo::help(desc));
		return;
	}
	std::string resp;
#ifdef HAVE_JSON_SPIRIT
	json_spirit::Array data;
#endif
	if (query) {
		BOOST_FOREACH(const std::string &cmd, provider_->get_commands()) {
			if (json) {
#ifdef HAVE_JSON_SPIRIT
				data.push_back(cmd);
#endif
			} else {
				resp += cmd + "\n";
			}
		}
	} else {
		boost::filesystem::path dir = provider_->get_core()->expand_path("${scripts}");
		boost::filesystem::path rel = provider_->get_core()->expand_path("${base-path}");
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
#ifdef HAVE_JSON_SPIRIT
					data.push_back(s);
#endif
				} else {
					resp += s + "\n";
				}

			}
		}
	}
#ifdef HAVE_JSON_SPIRIT
	if (json) {
		resp = json_spirit::write(data, json_spirit::raw_utf8);
	}
#endif

	nscapi::protobuf::functions::set_response_good(*response, resp);
}

void extscr_cli::show(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response) {
	namespace po = boost::program_options;
	namespace pf = nscapi::protobuf::functions;
	po::variables_map vm;
	po::options_description desc;
	std::string script;

	desc.add_options()
		("help", "Show help.")

		("script", po::value<std::string>(&script),
		"Script to show.")
		;

	try {
		npo::basic_command_line_parser cmd(request);
		cmd.options(desc);

		po::parsed_options parsed = cmd.run();
		po::store(parsed, vm);
		po::notify(vm);
	} catch (const std::exception &e) {
		return npo::invalid_syntax(desc, request.command(), "Invalid command line: " + utf8::utf8_from_native(e.what()), *response);
	}

	if (vm.count("help")) {
		nscapi::protobuf::functions::set_response_good(*response, npo::help(desc));
		return;
	}


	commands::command_object_instance command_def = provider_->find_command(script);
	if (command_def) {
		nscapi::protobuf::functions::set_response_good(*response, command_def->command);
	} else {
		boost::filesystem::path pscript = script;
		bool found = boost::filesystem::is_regular_file(pscript);
		if (!found) {
			pscript = provider_->get_core()->expand_path("${base-path}/" + script);
			found = boost::filesystem::is_regular_file(pscript);
		}
#ifdef WIN32
		if (!found) {
			pscript = boost::algorithm::replace_all_copy(script, "/", "\\");
			found = boost::filesystem::is_regular_file(pscript);
		}
#endif
		if (found) {
			if (!validate_sandbox(pscript, response)) {
				return;
			}
				
			std::ifstream t(pscript.string().c_str());
			std::string str((std::istreambuf_iterator<char>(t)),
				std::istreambuf_iterator<char>());

			nscapi::protobuf::functions::set_response_good(*response, str);
		} else {
			nscapi::protobuf::functions::set_response_bad(*response, "Script not found: " + script);
		}
	}
}

void extscr_cli::delete_script(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response) {
	namespace po = boost::program_options;
	namespace pf = nscapi::protobuf::functions;
	po::variables_map vm;
	po::options_description desc;
	std::string script;

	desc.add_options()
		("help", "Show help.")

		("script", po::value<std::string>(&script),
		"Script to delete.")
		;

	try {
		npo::basic_command_line_parser cmd(request);
		cmd.options(desc);

		po::parsed_options parsed = cmd.run();
		po::store(parsed, vm);
		po::notify(vm);
	} catch (const std::exception &e) {
		return npo::invalid_syntax(desc, request.command(), "Invalid command line: " + utf8::utf8_from_native(e.what()), *response);
	}

	if (vm.count("help")) {
		nscapi::protobuf::functions::set_response_good(*response, npo::help(desc));
		return;
	}


	commands::command_object_instance command_def = provider_->find_command(script);
	if (command_def) {
		provider_->remove_command(script);

		nscapi::protobuf::functions::settings_query s(provider_->get_id());
		s.save();
		provider_->get_core()->settings_query(s.request(), s.response());
		if (!s.validate_response()) {
			nscapi::protobuf::functions::set_response_bad(*response, s.get_response_error());
			return;
		}
		nscapi::protobuf::functions::set_response_good(*response, "Script definition has been removed don't forget to delete any artifact for: " + command_def->command);
	} else {
		boost::filesystem::path pscript = script;
		bool found = boost::filesystem::is_regular_file(pscript);
		if (!found) {
			pscript = provider_->get_core()->expand_path("${base-path}/" + script);
			found = boost::filesystem::is_regular_file(pscript);
		}
#ifdef WIN32
		if (!found) {
			pscript = boost::algorithm::replace_all_copy(script, "/", "\\");
			found = boost::filesystem::is_regular_file(pscript);
		}
#endif
		if (found) {
			if (!validate_sandbox(pscript, response)) {
				return;
			}
			boost::filesystem::remove(pscript);
			nscapi::protobuf::functions::set_response_good(*response, "Script file was removed");
		} else {
			nscapi::protobuf::functions::set_response_bad(*response, "Script not found: " + script);
		}
	}
}




void extscr_cli::add_script(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response) {
	namespace po = boost::program_options;
	namespace pf = nscapi::protobuf::functions;
	po::variables_map vm;
	po::options_description desc;
	std::string script, arguments, alias, import_script;
	bool wrapped = false, list = false, replace = false, no_config = false;

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

		("import", po::value<std::string>(&import_script),
		"Import (copy to script folder) a script.")

		("replace", po::bool_switch(&replace),
		"Used when importing to specify that the script will be overwritten.")

		("no-config", po::bool_switch(&no_config),
		"Do not write the updated configuration (i.e. changes are only transient).")

		;

	try {
		npo::basic_command_line_parser cmd(request);
		cmd.options(desc);

		po::parsed_options parsed = cmd.run();
		po::store(parsed, vm);
		po::notify(vm);
	} catch (const std::exception &e) {
		return npo::invalid_syntax(desc, request.command(), "Invalid command line: " + utf8::utf8_from_native(e.what()), *response);
	}

	if (vm.count("help")) {
		nscapi::protobuf::functions::set_response_good(*response, npo::help(desc));
		return;
	}
	boost::filesystem::path file = provider_->get_core()->expand_path(script);
	boost::filesystem::path script_root= provider_->get_root();

	if (!import_script.empty()) {
		file = script_root / file_helpers::meta::get_filename(file);
		script = "scripts\\" + file_helpers::meta::get_filename(file);
		if (boost::filesystem::exists(file)) {
			if (replace) {
				boost::filesystem::remove(file);
			} else {
				nscapi::protobuf::functions::set_response_bad(*response, "Script already exists specify --overwrite to replace the script");
				return;
			}
		}
		try {
			boost::filesystem::copy_file(import_script, file);
		} catch (const std::exception &e) {
			nscapi::protobuf::functions::set_response_bad(*response, "Failed to import script: " + utf8::utf8_from_native(e.what()));
			return;
		}
	}


	if (!wrapped) {
		bool found = boost::filesystem::is_regular(file);
		if (!found) {
			file = file = provider_->get_core()->expand_path("${shared-path}/" + file.string());
			found = boost::filesystem::is_regular(file);
		}
		if (!found) {
			nscapi::protobuf::functions::set_response_bad(*response, "Script not found: " + file.string());
			return;
		}
	}
	if (alias.empty()) {
		alias = boost::filesystem::basename(file.filename());
	}

	if (!no_config) {
		nscapi::protobuf::functions::settings_query s(provider_->get_id());
		if (!wrapped)
			s.set("/settings/external scripts/scripts", alias, script + " " + arguments);
		else
			s.set("/settings/external scripts/wrapped scripts", alias, script + " " + arguments);
		s.set(MAIN_MODULES_SECTION, "CheckExternalScripts", "enabled");
		s.save();
		provider_->get_core()->settings_query(s.request(), s.response());
		if (!s.validate_response()) {
			nscapi::protobuf::functions::set_response_bad(*response, s.get_response_error());
			return;
		}
	}
	std::string actual = "";
	if (wrapped)
		actual = "\nActual command is: " + provider_->generate_wrapped_command(script + " " + arguments);
	else {
		provider_->add_command(alias, script);
		nscapi::core_helper core(provider_->get_core(), provider_->get_id());
		core.register_command(alias, "Alias for: " + script);
	}
	nscapi::protobuf::functions::set_response_good(*response, "Added " + alias + " as " + script + actual);
}

void extscr_cli::configure(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response) {
	po::variables_map vm;
	po::options_description desc;
	std::string arguments = "false";
	const std::string path = "/settings/external scripts/server";

	pf::settings_query q(provider_->get_id());
	q.get(path, "allow arguments", false);
	q.get(path, "allow nasty characters", false);

	provider_->get_core()->settings_query(q.request(), q.response());
	if (!q.validate_response()) {
		nscapi::protobuf::functions::set_response_bad(*response, q.get_response_error());
		return;
	}
	BOOST_FOREACH(const pf::settings_query::key_values &val, q.get_query_key_response()) {
		if (val.matches(path, "allow arguments") && val.get_bool())
			arguments = "true";
		else if (val.matches(path, "allow nasty characters") && val.get_bool())
			arguments = "safe";
	}
	desc.add_options()
		("help", "Show help.")

		("arguments", po::value<std::string>(&arguments)->default_value(arguments)->implicit_value("safe"),
		"Allow arguments. false=don't allow, safe=allow non escape chars, all=allow all arguments.")

		;

	try {
		npo::basic_command_line_parser cmd(request);
		cmd.options(desc);

		po::parsed_options parsed = cmd.run();
		po::store(parsed, vm);
		po::notify(vm);
	} catch (const std::exception &e) {
		return npo::invalid_syntax(desc, request.command(), "Invalid command line: " + utf8::utf8_from_native(e.what()), *response);
	}

	if (vm.count("help")) {
		nscapi::protobuf::functions::set_response_good(*response, npo::help(desc));
		return;
	}
	std::stringstream result;

	nscapi::protobuf::functions::settings_query s(provider_->get_id());
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
	provider_->get_core()->settings_query(s.request(), s.response());
	if (!s.validate_response())
		nscapi::protobuf::functions::set_response_bad(*response, s.get_response_error());
	else
		nscapi::protobuf::functions::set_response_good(*response, result.str());
}


