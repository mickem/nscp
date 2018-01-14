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
#include <boost/optional.hpp>
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
namespace fs = boost::filesystem;

#define SCRIPT_PATH "/settings/python/scripts"
#define MODULE_NAME "PythonScript"
#define REL_SCRIPT_PATH "scripts\\python\\"


extscr_cli::extscr_cli(boost::shared_ptr<script_provider_interface> provider, std::string alias_)
	: provider_(provider)
	, alias_(alias_)
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

bool extscr_cli::validate_sandbox(fs::path pscript, Plugin::ExecuteResponseMessage::Response *response) {
	fs::path path = provider_->get_root();
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
		Plugin::RegistryRequestMessage rrm;
		Plugin::RegistryResponseMessage response;
		Plugin::RegistryRequestMessage::Request *payload = rrm.add_payload();
		payload->mutable_inventory()->set_fetch_all(true);
		payload->mutable_inventory()->add_type(Plugin::Registry_ItemType_QUERY);
		std::string pb_response;
		provider_->get_core()->registry_query(rrm.SerializeAsString(), pb_response);
		response.ParseFromString(pb_response);
		BOOST_FOREACH(const ::Plugin::RegistryResponseMessage_Response &p, response.payload()) {
			BOOST_FOREACH(const ::Plugin::RegistryResponseMessage_Response_Inventory &i, p.inventory()) {
				if (json) {
#ifdef HAVE_JSON_SPIRIT
					data.push_back(i.name());
#endif
				} else {
					resp += i.name() + "\n";
				}
			}
		}
	} else {
		fs::path dir = provider_->get_core()->expand_path("${scripts}/python");
		fs::path rel = provider_->get_core()->expand_path("${base-path}/python");
		fs::recursive_directory_iterator iter(dir), eod;
		BOOST_FOREACH(fs::path const& i, std::make_pair(iter, eod)) {
			std::string s = i.string();
			if (boost::algorithm::starts_with(s, rel.string()))
				s = s.substr(rel.string().size());
			if (s.size() == 0)
				continue;
			if (s[0] == '\\' || s[0] == '/')
				s = s.substr(1);
			fs::path clone = i.parent_path();
			if (fs::is_regular_file(i) && !boost::algorithm::contains(clone.string(), "lib")) {
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


// 	commands::command_object_instance command_def = provider_->find_command(script);
// 	if (command_def) {
// 		nscapi::protobuf::functions::set_response_good(*response, command_def->command);
// 	} else {
// 		fs::path pscript = script;
// 		bool found = fs::is_regular_file(pscript);
// 		if (!found) {
// 			pscript = provider_->get_core()->expand_path("${base-path}/" + script);
// 			found = fs::is_regular_file(pscript);
// 		}
// #ifdef WIN32
// 		if (!found) {
// 			pscript = boost::algorithm::replace_all_copy(script, "/", "\\");
// 			found = fs::is_regular_file(pscript);
// 		}
// #endif
// 		if (found) {
// 			if (!validate_sandbox(pscript, response)) {
// 				return;
// 			}
// 				
// 			std::ifstream t(pscript.string().c_str());
// 			std::string str((std::istreambuf_iterator<char>(t)),
// 				std::istreambuf_iterator<char>());
// 
// 			nscapi::protobuf::functions::set_response_good(*response, str);
// 		} else {
// 			nscapi::protobuf::functions::set_response_bad(*response, "Script not found: " + script);
// 		}
// 	}
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


// 	commands::command_object_instance command_def = provider_->find_command(script);
// 	if (command_def) {
// 		provider_->remove_command(script);
// 
// 		nscapi::protobuf::functions::settings_query s(provider_->get_id());
// 		s.save();
// 		provider_->get_core()->settings_query(s.request(), s.response());
// 		if (!s.validate_response()) {
// 			nscapi::protobuf::functions::set_response_bad(*response, s.get_response_error());
// 			return;
// 		}
// 		nscapi::protobuf::functions::set_response_good(*response, "Script definition has been removed don't forget to delete any artifact for: " + command_def->command);
// 	} else {
// 		fs::path pscript = script;
// 		bool found = fs::is_regular_file(pscript);
// 		if (!found) {
// 			pscript = provider_->get_core()->expand_path("${base-path}/" + script);
// 			found = fs::is_regular_file(pscript);
// 		}
// #ifdef WIN32
// 		if (!found) {
// 			pscript = boost::algorithm::replace_all_copy(script, "/", "\\");
// 			found = fs::is_regular_file(pscript);
// 		}
// #endif
// 		if (found) {
// 			if (!validate_sandbox(pscript, response)) {
// 				return;
// 			}
// 			fs::remove(pscript);
// 			nscapi::protobuf::functions::set_response_good(*response, "Script file was removed");
// 		} else {
// 			nscapi::protobuf::functions::set_response_bad(*response, "Script not found: " + script);
// 		}
// 	}
}




void extscr_cli::add_script(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response) {
	namespace po = boost::program_options;
	namespace pf = nscapi::protobuf::functions;
	po::variables_map vm;
	po::options_description desc;
	std::string script, alias, import_script;
	bool list = false, replace = false, no_config = false;

	desc.add_options()
		("help", "Show help.")

		("script", po::value<std::string>(&script),
			"Script to add")

		("alias", po::value<std::string>(&alias),
			"The alias of the script (defaults to basename of script)")

		("list", po::bool_switch(&list),
			"List all scripts in the scripts folder.")

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
	fs::path file = provider_->get_core()->expand_path(script);
	fs::path script_root= provider_->get_root();

	if (!import_script.empty()) {
		file = script_root / file_helpers::meta::get_filename(file);
		script = REL_SCRIPT_PATH + file_helpers::meta::get_filename(file);
		if (fs::exists(file)) {
			if (replace) {
				fs::remove(file);
			} else {
				nscapi::protobuf::functions::set_response_bad(*response, "Script already exists specify --overwrite to replace the script");
				return;
			}
		}
		try {
			fs::copy_file(import_script, file);
		} catch (const std::exception &e) {
			nscapi::protobuf::functions::set_response_bad(*response, "Failed to import script: " + utf8::utf8_from_native(e.what()));
			return;
		}
	}


	bool found = fs::is_regular(file);
	if (!found) {
		boost::optional<fs::path> path = provider_->find_file(file.string());
		if (path) {
			file = *path;
			found = fs::is_regular(file);
		}
	}
	if (!found) {
		nscapi::protobuf::functions::set_response_bad(*response, "Script not found: " + file.string());
		return;
	}
	if (alias.empty()) {
		alias = fs::basename(file.filename());
	}

	if (!no_config) {
		nscapi::protobuf::functions::settings_query s(provider_->get_id());
		s.set(SCRIPT_PATH, alias, script);
		s.set(MAIN_MODULES_SECTION, MODULE_NAME, "enabled");
		s.save();
		provider_->get_core()->settings_query(s.request(), s.response());
		if (!s.validate_response()) {
			nscapi::protobuf::functions::set_response_bad(*response, s.get_response_error());
			return;
		}
	}
	std::string actual = "";
	provider_->add_command(alias, script, alias_);
	nscapi::core_helper core(provider_->get_core(), provider_->get_id());
	core.register_command(alias, "Alias for: " + script);
	nscapi::protobuf::functions::set_response_good(*response, "Added " + alias + " as " + script + actual);
}

void extscr_cli::configure(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response) {
	po::variables_map vm;
	po::options_description desc;
	typedef std::map<std::string, std::string> script_map_type;
	typedef std::vector<std::string> script_lst_type;
	script_map_type scripts;
	script_lst_type to_add;
	script_lst_type to_remove;
	bool module = false;

	pf::settings_query q(provider_->get_id());
	q.list(SCRIPT_PATH);
	q.get(MAIN_MODULES_SECTION, MODULE_NAME, "");

	provider_->get_core()->settings_query(q.request(), q.response());
	if (!q.validate_response()) {
		nscapi::protobuf::functions::set_response_bad(*response, q.get_response_error());
		return;
	}
	BOOST_FOREACH(const pf::settings_query::key_values &val, q.get_query_key_response()) {
		if (val.matches(MAIN_MODULES_SECTION, MODULE_NAME) && val.get_bool())
			module = true;
		else if (val.matches(SCRIPT_PATH))
			scripts[val.get_string()] = val.key();
	}
	desc.add_options()
		("help", "Show help.")
		("add", po::value<script_lst_type>(&to_add), "Scripts to add to the list of loaded scripts.")
		("remove", po::value<script_lst_type>(&to_remove), "Scripts to remove from list of loaded scripts.")

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

	nscapi::protobuf::functions::settings_query sq(provider_->get_id());
	if (!module) {
		sq.set(MAIN_MODULES_SECTION, MODULE_NAME, "enabled");
	}
	BOOST_FOREACH(const std::string &s, to_add) {
		if (!provider_->find_file(s)) {
			result << "Failed to find: " << s << std::endl;
		} else {
			if (scripts.find(s) == scripts.end()) {
				sq.set(SCRIPT_PATH, s, s);
				scripts[s] = s;
			} else {
				result << "Failed to add duplicate script: " << s << std::endl;
			}
		}
	}
	BOOST_FOREACH(const std::string &s, to_remove) {
		const script_map_type::const_iterator v = scripts.find(s);
		if (v != scripts.end()) {
			sq.erase(SCRIPT_PATH, v->second);
			scripts.erase(s);
		} else {
			result << "Failed to remove nonexisting script: " << s << std::endl;
		}
	}
	BOOST_FOREACH(const script_map_type::value_type &e, scripts) {
		result << e.second << std::endl;
	}

	sq.save();
	provider_->get_core()->settings_query(sq.request(), sq.response());
	if (!sq.validate_response())
		nscapi::protobuf::functions::set_response_bad(*response, sq.get_response_error());
	else
		nscapi::protobuf::functions::set_response_good(*response, result.str());
}


