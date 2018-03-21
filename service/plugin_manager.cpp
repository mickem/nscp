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

#include "plugin_manager.hpp"

#include "dll_plugin.h"
#ifdef HAVE_JSON_SPIRIT
#include "zip_plugin.h"
#endif
#include <str/format.hpp>
#include <file_helpers.hpp>
#include <settings/settings_core.hpp>
#include <config.h>
#include "../libs/settings_manager/settings_manager_impl.h"

#include <nscapi/nscapi_protobuf_functions.hpp>

#include <boost/unordered_map.hpp>

struct command_chunk {
	nsclient::commands::plugin_type plugin;
	Plugin::QueryRequestMessage request;
};

bool nsclient::core::plugin_manager::contains_plugin(nsclient::core::plugin_manager::plugin_alias_list_type &ret, std::string alias, std::string plugin) {
	std::pair<std::string, std::string> v;
	BOOST_FOREACH(v, ret.equal_range(alias)) {
		if (v.second == plugin)
			return true;
	}
	return false;
}

nsclient::core::plugin_manager::plugin_manager(nsclient::core::path_instance path_, nsclient::logging::logger_instance log_instance)
	: path_(path_)
	, log_instance_(log_instance)
	, plugin_list_(log_instance_)
	, commands_(log_instance_)
	, channels_(log_instance_)
	, metrics_fetchers_(log_instance_)
	, metrics_submitetrs_(log_instance_)
	, plugin_cache_(log_instance_)
	, event_subscribers_(log_instance_)
{
}

nsclient::core::plugin_manager::~plugin_manager() {
}

// Find all plugins on the filesystem
nsclient::core::plugin_manager::plugin_alias_list_type nsclient::core::plugin_manager::find_all_plugins() {
	plugin_alias_list_type ret;

	settings::settings_interface::string_list list = settings_manager::get_settings()->get_keys(MAIN_MODULES_SECTION);
	BOOST_FOREACH(std::string plugin, list) {
		std::string alias;
		try {
			alias = settings_manager::get_settings()->get_string(MAIN_MODULES_SECTION, plugin, "");
		} catch (settings::settings_exception e) {
			LOG_ERROR_CORE_STD("Exception looking for module: " + e.reason());
		}
		if (plugin == "enabled" || plugin == "1" || plugin == "true") {
			plugin = alias;
			alias = "";
		} else if (alias == "enabled" || alias == "1" || alias == "true") {
			alias = "";
		} else if (plugin == "disabled" || plugin == "0" || plugin == "false") {
			plugin = alias;
			alias = "";
		} else if (alias == "disabled" || alias == "0" || alias == "false") {
			alias = "";
		}
		if (!alias.empty()) {
			std::string tmp = plugin;
			plugin = alias;
			alias = tmp;
		}
		if (alias.empty()) {
			LOG_DEBUG_CORE_STD("Found: " + plugin);
		} else {
			LOG_DEBUG_CORE_STD("Found: " + plugin + " as " + alias);
		}
		if (plugin.length() > 4 && plugin.substr(plugin.length() - 4) == ".dll")
			plugin = plugin.substr(0, plugin.length() - 4);
		ret.insert(plugin_alias_list_type::value_type(alias, plugin));
	}
	boost::filesystem::directory_iterator end_itr; // default construction yields past-the-end
	for (boost::filesystem::directory_iterator itr(plugin_path_); itr != end_itr; ++itr) {
		if (!is_directory(itr->status())) {
			boost::filesystem::path file = itr->path().filename();
			if (is_module(plugin_path_ / file)) {
				const std::string module = file_to_module(file);
				if (!contains_plugin(ret, "", module))
					ret.insert(plugin_alias_list_type::value_type("", module));
			}
		}
	}
	return ret;
}

// Find all plugins which are marked as active under the [/modules] section.
nsclient::core::plugin_manager::plugin_alias_list_type nsclient::core::plugin_manager::find_all_active_plugins() {
	plugin_alias_list_type ret;

	settings::settings_interface::string_list list = settings_manager::get_settings()->get_keys(MAIN_MODULES_SECTION);
	BOOST_FOREACH(std::string plugin, list) {
		std::string alias;
		try {
			alias = settings_manager::get_settings()->get_string(MAIN_MODULES_SECTION, plugin, "");
		} catch (settings::settings_exception e) {
			LOG_DEBUG_CORE_STD("Exception looking for module: " + e.reason());
		}
		if (plugin == "enabled" || plugin == "1" || plugin == "true") {
			plugin = alias;
			alias = "";
		} else if (alias == "enabled" || alias == "1" || alias == "true") {
			alias = "";
		} else if ((plugin == "disabled") || (alias == "disabled")) {
			continue;
		} else if ((plugin == "0") || (alias == "0")) {
			continue;
		} else if ((plugin == "false") || (alias == "false")) {
			continue;
		} else if (plugin == "disabled" || plugin == "0" || plugin == "false") {
			plugin = alias;
			alias = "";
		} else if (alias == "disabled" || alias == "0" || alias == "false") {
			alias = "";
		}
		if (!alias.empty()) {
			std::string tmp = plugin;
			plugin = alias;
			alias = tmp;
		}
		if (alias.empty()) {
			LOG_DEBUG_CORE_STD("Found: " + plugin);
		} else {
			LOG_DEBUG_CORE_STD("Found: " + plugin + " as " + alias);
		}
		if (plugin.length() > 4 && plugin.substr(plugin.length() - 4) == ".dll")
			plugin = plugin.substr(0, plugin.length() - 4);
		ret.insert(plugin_alias_list_type::value_type(alias, plugin));
	}
	return ret;
}

// Load all configured (nsclient.ini) plugins.
void nsclient::core::plugin_manager::load_active_plugins() {
	if (plugin_path_.empty()) {
		throw core_exception("Please configure plugin_manager first");
	}
	BOOST_FOREACH(const plugin_alias_list_type::value_type &v, find_all_active_plugins()) {
		std::string module = v.first;
		std::string alias = v.first;
		try {
			add_plugin(v.second, v.first);
		} catch (const plugin_exception& e) {
			if (e.file().find("FileLogger") != std::string::npos) {
				LOG_DEBUG_CORE_STD("Failed to load " + module + ": " + e.reason());
			} else {
				LOG_ERROR_CORE_STD("Failed to load " + module + ": " + e.reason());
			}
		} catch (const std::exception &e) {
			LOG_ERROR_CORE_STD("exception loading plugin: " + module + utf8::utf8_from_native(e.what()));
		} catch (...) {
			LOG_ERROR_CORE_STD("Unknown exception loading plugin: " + module);
		}
	}
}
// Load all available plugins (from the filesystem)
void nsclient::core::plugin_manager::load_all_plugins() {
	BOOST_FOREACH(plugin_alias_list_type::value_type v, find_all_plugins()) {
		if (v.second == "NSCPDOTNET.dll" || v.second == "NSCPDOTNET" || v.second == "NSCP.Core")
			continue;
		try {
			add_plugin(v.second, v.first);
		} catch (const plugin_exception &e) {
			if (e.file().find("FileLogger") != std::string::npos) {
				LOG_DEBUG_CORE_STD("Failed to register plugin: " + e.reason());
			} else {
				LOG_ERROR_CORE("Failed to register plugin " + v.second + ": " + e.reason());
			}
		} catch (...) {
			LOG_CRITICAL_CORE_STD("Failed to register plugin key: " + v.second);
		}
	}
}

bool nsclient::core::plugin_manager::load_single_plugin(std::string plugin, std::string alias, bool start) {
	try {
		plugin_type instance = add_plugin(plugin, alias);
		if (!instance) {
			LOG_ERROR_CORE("Failed to load: " + plugin);
			return false;
		}
		if (start) {
			instance->load_plugin(NSCAPI::normalStart);
		}
		return true;
	} catch (const plugin_exception &e) {
		LOG_ERROR_CORE_STD("Module (" + e.file() + ") was not found: " + e.reason());
	} catch (const std::exception &e) {
		LOG_ERROR_CORE_STD("Module (" + plugin + ") was not found: " + utf8::utf8_from_native(e.what()));
	} catch (...) {
		LOG_ERROR_CORE_STD("Module (" + plugin + ") was not found...");
	}
	return false;
}

void nsclient::core::plugin_manager::start_plugins(NSCAPI::moduleLoadMode mode) {
	std::set<long> broken;
	BOOST_FOREACH(plugin_type plugin, plugin_list_.get_plugins()) {
		LOG_DEBUG_CORE_STD("Loading plugin: " + plugin->getModule())
		try {
			if (!plugin->load_plugin(mode)) {
				LOG_ERROR_CORE_STD("Plugin refused to load: " + plugin->getModule());
				broken.insert(plugin->get_id());
			}
		} catch (const plugin_exception &e) {
			broken.insert(plugin->get_id());
			LOG_ERROR_CORE_STD("Could not load plugin: " + e.reason() + ": " + e.file());
		} catch (const std::exception &e) {
			broken.insert(plugin->get_id());
			LOG_ERROR_CORE_STD("Could not load plugin: " + plugin->get_alias() + ": " + e.what());
		} catch (...) {
			broken.insert(plugin->get_id());
			LOG_ERROR_CORE_STD("Could not load plugin: " + plugin->getModule());
		}
	}
	BOOST_FOREACH(const long &id, broken) {
		plugin_list_.remove(id);
	}
}
/**
 * Unload all plug-ins
 */
void nsclient::core::plugin_manager::stop_plugins() {
	commands_.remove_all();
	channels_.remove_all();
	std::list<plugin_type> tmp = plugin_list_.get_plugins();
	BOOST_FOREACH(plugin_type p, tmp) {
		try {
			if (p) {
				LOG_DEBUG_CORE_STD("Unloading plugin: " + p->get_alias_or_name() + "...");
				p->unload_plugin();
			}
		} catch (const plugin_exception &e) {
			LOG_ERROR_CORE_STD("Exception raised when unloading plugin: " + e.reason() + " in module: " + e.file());
		} catch (...) {
			LOG_ERROR_CORE("Unknown exception raised when unloading plugin");
		}
	}
	plugin_list_.clear();
}

boost::optional<boost::filesystem::path> nsclient::core::plugin_manager::find_file(std::string file_name) {
	std::string name = file_name;
	std::list<std::string> names;
	names.push_back(file_name);
	if (name.length() > 4 && (name.substr(name.length() - 4) == ".dll" || name.substr(name.length() - 4) == ".zip")) {
		name = name.substr(0, name.length() - 4);
	}
	names.push_back(get_plugin_file(name));
	names.push_back(name + ".zip");

	BOOST_FOREACH(const std::string &name, names) {
		boost::filesystem::path tmp = plugin_path_ / name;
		if (boost::filesystem::is_regular_file(tmp))
			return tmp;
	}

	BOOST_FOREACH(const std::string &name, names) {
		boost::optional<boost::filesystem::path> module = file_helpers::finder::locate_file_icase(plugin_path_, name);
		if (module) {
			return module;
		}
		module = file_helpers::finder::locate_file_icase(boost::filesystem::path("./modules"), name);
		if (module) {
			return module;
		}
	}
	LOG_ERROR_CORE("Failed to find plugin: " + file_name + " in " + plugin_path_.string());
	return boost::optional<boost::filesystem::path>();
}

nsclient::core::plugin_manager::plugin_type nsclient::core::plugin_manager::only_load_module(std::string module, std::string alias, bool &loaded) {
	loaded = false;
	boost::optional<boost::filesystem::path> real_file = find_file(module);
	if (!real_file) {
		return nsclient::core::plugin_manager::plugin_type();
	}
	LOG_DEBUG_CORE_STD("Loading module " + real_file->string() + " (" + alias + ")");
	plugin_type dup = plugin_list_.find_duplicate(*real_file, alias);
	if (dup) {
		return dup;
	}
	loaded = true;
#ifdef HAVE_JSON_SPIRIT
	if (boost::algorithm::ends_with(real_file->string(), ".zip")) {
		return plugin_type(new nsclient::core::zip_plugin(plugin_list_.get_next_id(), real_file->normalize(), alias, path_, shared_from_this(), log_instance_));
	}
#endif
	return plugin_type(new nsclient::core::dll_plugin(plugin_list_.get_next_id(), real_file->normalize(), alias));
}

/**
 * Load and add a plugin to various internal structures
 * @param plugin The plug-in instance to load. The pointer is managed by the
 */
nsclient::core::plugin_manager::plugin_type nsclient::core::plugin_manager::add_plugin(std::string file_name, std::string alias) {
	try {
		bool loaded = false;
		plugin_type plugin = only_load_module(file_name, alias, loaded);
		if (!loaded) {
			return plugin;
		}
		plugin_list_.append_plugin(plugin);
		if (plugin->hasCommandHandler()) {
			commands_.add_plugin(plugin);
		}
		if (plugin->hasNotificationHandler()) {
			channels_.add_plugin(plugin);
		}
		if (plugin->hasMetricsFetcher()) {
			metrics_fetchers_.add_plugin(plugin);
		}
		if (plugin->hasMetricsSubmitter()) {
			metrics_submitetrs_.add_plugin(plugin);
		}
		if (plugin->hasMessageHandler()) {
			log_instance_->add_subscriber(plugin);
		}
		if (plugin->has_on_event()) {
			event_subscribers_.add_plugin(plugin);
		}
		settings_manager::get_core()->register_key(0xffff, MAIN_MODULES_SECTION, plugin->getModule(), settings::settings_core::key_string, plugin->getName(), plugin->getDescription(), "0", false, false);
		plugin_cache_.add_plugin(plugin);
		return plugin;
	} catch (const std::exception &e) {
		LOG_ERROR_CORE("Failed to load plugin " + file_name + ": " + utf8::utf8_from_native(e.what()));
		return nsclient::core::plugin_manager::plugin_type();
	} catch (...) {
		LOG_ERROR_CORE("Failed to load plugin " + file_name);
		return nsclient::core::plugin_manager::plugin_type();
	}

}

bool nsclient::core::plugin_manager::reload_plugin(const std::string module) {
	plugin_type plugin = plugin_list_.find_by_alias(module);
	if (plugin) {
		LOG_DEBUG_CORE_STD(std::string("Reloading: ") + plugin->get_alias_or_name());
		plugin->load_plugin(NSCAPI::reloadStart);
		return true;
	} 
	LOG_ERROR_CORE("Failed to reload plugin " + module);
	return false;
}



void nsclient::core::plugin_manager::remove_plugin(const std::string name) {
	plugin_type plugin = plugin_list_.find_by_module(name);
	if (!plugin) {
		LOG_ERROR_CORE("Module " + name + " was not found.");
		return;
	}
	unsigned int plugin_id = plugin->get_id();
	plugin_list_.remove(plugin_id);
	commands_.remove_plugin(plugin_id);
	metrics_fetchers_.remove_plugin(plugin_id);
	metrics_submitetrs_.remove_plugin(plugin_id);
	plugin->unload_plugin();
	plugin_cache_.remove_plugin(plugin_id);
}

unsigned int nsclient::core::plugin_manager::clone_plugin(unsigned int plugin_id) {
	plugin_type match = plugin_list_.find_by_id(plugin_id);
	if (match) {
		int new_id = plugin_list_.get_next_id();
		commands_.add_plugin(new_id, match);
		return new_id;
	} else {
		LOG_ERROR_CORE("Plugin not found.");
		return -1;
	}
}

std::string nsclient::core::plugin_manager::get_plugin_module_name(unsigned int plugin_id) {
	plugin_type plugin = plugin_list_.find_by_id(plugin_id);
	if (!plugin)
		return "";
	return plugin->get_alias_or_name();
}


nsclient::core::plugin_manager::plugin_type nsclient::core::plugin_manager::find_plugin(const unsigned int plugin_id) {
	return plugin_list_.find_by_id(plugin_id);
}

::Plugin::QueryResponseMessage nsclient::core::plugin_manager::execute_query(const ::Plugin::QueryRequestMessage &req) {
	::Plugin::QueryResponseMessage resp;
	std::string buffer;
	if (execute_query(req.SerializeAsString(), buffer) == NSCAPI::cmd_return_codes::isSuccess) {
		resp.ParseFromString(buffer);
	}
	return resp;
}
/**
 * Inject a command into the plug-in stack.
 *
 * @param command Command to inject
 * @param argLen Length of argument buffer
 * @param **argument Argument buffer
 * @param *returnMessageBuffer Message buffer
 * @param returnMessageBufferLen Length of returnMessageBuffer
 * @param *returnPerfBuffer Performance data buffer
 * @param returnPerfBufferLen Length of returnPerfBuffer
 * @return The command status
 */
NSCAPI::nagiosReturn nsclient::core::plugin_manager::execute_query(const std::string &request, std::string &response) {
	try {
		Plugin::QueryRequestMessage request_message;
		Plugin::QueryResponseMessage response_message;
		request_message.ParseFromString(request);

		typedef boost::unordered_map<int, command_chunk> command_chunk_type;
		command_chunk_type command_chunks;

		std::string missing_commands;

		if (request_message.header().has_command()) {
			std::string command = request_message.header().command();
			nsclient::commands::plugin_type plugin = commands_.get(command);
			if (plugin) {
				unsigned int id = plugin->get_id();
				command_chunks[id].plugin = plugin;
				command_chunks[id].request.CopyFrom(request_message);
			} else {
				str::format::append_list(missing_commands, command);
			}
		} else {
			for (int i = 0; i < request_message.payload_size(); i++) {
				::Plugin::QueryRequestMessage::Request *payload = request_message.mutable_payload(i);
				payload->set_command(commands_.make_key(payload->command()));
				nsclient::commands::plugin_type plugin = commands_.get(payload->command());
				if (plugin) {
					unsigned int id = plugin->get_id();
					if (command_chunks.find(id) == command_chunks.end()) {
						command_chunks[id].plugin = plugin;
						command_chunks[id].request.mutable_header()->CopyFrom(request_message.header());
					}
					command_chunks[id].request.add_payload()->CopyFrom(*payload);
				} else {
					str::format::append_list(missing_commands, payload->command());
				}
			}
		}

		if (command_chunks.size() == 0) {
			LOG_ERROR_CORE("Unknown command(s): " + missing_commands + " available commands: " + commands_.to_string());
			Plugin::QueryResponseMessage::Response *payload = response_message.add_payload();
			payload->set_command(missing_commands);
			nscapi::protobuf::functions::set_response_bad(*payload, "Unknown command(s): " + missing_commands);
			response = response_message.SerializeAsString();
			return NSCAPI::cmd_return_codes::isSuccess;
		}

		BOOST_FOREACH(command_chunk_type::value_type &v, command_chunks) {
			std::string local_response;
			int ret = v.second.plugin->handleCommand(v.second.request.SerializeAsString(), local_response);
			if (ret != NSCAPI::cmd_return_codes::isSuccess) {
				LOG_ERROR_CORE("Failed to execute command");
			} else {
				Plugin::QueryResponseMessage local_response_message;
				local_response_message.ParseFromString(local_response);
				if (!response_message.has_header()) {
					response_message.mutable_header()->CopyFrom(local_response_message.header());
				}
				for (int i = 0; i < local_response_message.payload_size(); i++) {
					response_message.add_payload()->CopyFrom(local_response_message.payload(i));
				}
			}
		}
		response = response_message.SerializeAsString();
	} catch (const std::exception &e) {
		LOG_ERROR_CORE("Failed to process command: " + utf8::utf8_from_native(e.what()));
		return NSCAPI::cmd_return_codes::hasFailed;
	} catch (...) {
		LOG_ERROR_CORE("Failed to process command: ");
		return NSCAPI::cmd_return_codes::hasFailed;
	}
	return NSCAPI::cmd_return_codes::isSuccess;
}

int nsclient::core::plugin_manager::load_and_run(std::string module, run_function fun, std::list<std::string> &errors) {
	if (!module.empty()) {
		plugin_type match = plugin_list_.find_by_module(module);
		if (match) {
			LOG_DEBUG_CORE_STD("Found module: " + match->get_alias_or_name() + "...");
			try {
				return fun(match);
			} catch (const plugin_exception &e) {
				errors.push_back("Could not execute command: " + e.reason() + " in " + e.file());
				return 1;
			}
		}
		try {
			plugin_type plugin = add_plugin(module, "");
			if (plugin) {
				LOG_DEBUG_CORE_STD("Loading plugin: " + plugin->get_alias_or_name() + "...");
				plugin->load_plugin(NSCAPI::dontStart);
				return fun(plugin);
			} else {
				errors.push_back("Failed to load: " + module);
				return 1;
			}
		} catch (const plugin_exception &e) {
			errors.push_back("Module (" + e.file() + ") was not found: " + utf8::utf8_from_native(e.what()));
		} catch (const std::exception &e) {
			errors.push_back(std::string("Module (") + module + ") was not found: " + utf8::utf8_from_native(e.what()));
			return 1;
		} catch (...) {
			errors.push_back("Module (" + module + ") was not found...");
			return 1;
		}
	} else {
		errors.push_back("No module was specified...");
	}
	return 1;
}

int exec_helper(nsclient::core::plugin_manager::plugin_type plugin, std::string command, std::vector<std::string> arguments, std::string request, std::list<std::string> *responses) {
	std::string response;
	if (!plugin || !plugin->has_command_line_exec())
		return -1;
	int ret = plugin->commandLineExec(true, request, response);
	if (ret != NSCAPI::cmd_return_codes::returnIgnored && !response.empty())
		responses->push_back(response);
	return ret;
}

int nsclient::core::plugin_manager::simple_exec(std::string command, std::vector<std::string> arguments, std::list<std::string> &resp) {
	std::string request;
	std::list<std::string> responses;
	std::list<std::string> errors;
	std::string module;
	std::string::size_type pos = command.find('.');
	if (pos != std::string::npos) {
		module = command.substr(0, pos);
		command = command.substr(pos + 1);
	}
	nscapi::protobuf::functions::create_simple_exec_request(module, command, arguments, request);
	int ret = load_and_run(module, boost::bind(&exec_helper, _1, command, arguments, request, &responses), errors);

	BOOST_FOREACH(std::string &r, responses) {
		try {
			ret = nscapi::protobuf::functions::parse_simple_exec_response(r, resp);
		} catch (std::exception &e) {
			resp.push_back("Failed to extract return message: " + utf8::utf8_from_native(e.what()));
			LOG_ERROR_CORE_STD("Failed to extract return message: " + utf8::utf8_from_native(e.what()));
			return NSCAPI::cmd_return_codes::hasFailed;
		}
	}
	BOOST_FOREACH(const std::string &e, errors) {
		LOG_ERROR_CORE_STD(e);
		resp.push_back(e);
	}
	return ret;
}
int query_helper(nsclient::core::plugin_manager::plugin_type plugin, std::string command, std::vector<std::string> arguments, std::string request, std::list<std::string> *responses) {
	return NSCAPI::cmd_return_codes::returnIgnored;
	// 	std::string response;
	// 	if (!plugin->hasCommandHandler())
	// 		return NSCAPI::returnIgnored;
	// 	int ret = plugin->handleCommand(command.c_str(), request, response);
	// 	if (ret != NSCAPI::returnIgnored && !response.empty())
	// 		responses->push_back(response);
	// 	return ret;
}

int nsclient::core::plugin_manager::simple_query(std::string module, std::string command, std::vector<std::string> arguments, std::list<std::string> &resp) {
	std::string request;
	std::list<std::string> responses;
	std::list<std::string> errors;
	nscapi::protobuf::functions::create_simple_query_request(command, arguments, request);
	int ret = load_and_run(module, boost::bind(&query_helper, _1, command, arguments, request, &responses), errors);

	nsclient::commands::plugin_type plugin = commands_.get(command);
	if (!plugin) {
		LOG_ERROR_CORE("No handler for command: " + command + " available commands: " + commands_.to_string());
		resp.push_back("No handler for command: " + command);
		return NSCAPI::query_return_codes::returnUNKNOWN;
	}
	std::string response;
	ret = plugin->handleCommand(request, response);
	try {
		std::string msg, perf;
		ret = nscapi::protobuf::functions::parse_simple_query_response(response, msg, perf, -1);
		resp.push_back(msg + "|" + perf);
	} catch (std::exception &e) {
		resp.push_back("Failed to extract return message: " + utf8::utf8_from_native(e.what()));
		LOG_ERROR_CORE_STD("Failed to extract return message: " + utf8::utf8_from_native(e.what()));
		return NSCAPI::query_return_codes::returnUNKNOWN;
	}
	BOOST_FOREACH(const std::string &e, errors) {
		LOG_ERROR_CORE_STD(e);
		resp.push_back(e);
	}
	return ret;
}

NSCAPI::nagiosReturn nsclient::core::plugin_manager::exec_command(const char* raw_target, std::string request, std::string &response) {
	std::string target = raw_target;
	LOG_DEBUG_CORE_STD("Executing command is target for: " + target);
	bool match_any = false;
	bool match_all = false;
	if (target == "any")
		match_any = true;
	else if (target == "all" || target == "*")
		match_all = true;
	std::list<std::string> responses;
	bool found = false;
	BOOST_FOREACH(plugin_type p, plugin_list_.get_plugins()) {
		if (p && p->has_command_line_exec()) {
			IS_LOG_TRACE_CORE() {
				LOG_TRACE_CORE("Trying : " + p->get_alias_or_name());
			}
			try {
				if (match_all || match_any || p->get_alias() == target || p->get_alias_or_name().find(target) != std::string::npos) {
					std::string respbuffer;
					LOG_DEBUG_CORE_STD("Executing command in: " + p->getName());
					NSCAPI::nagiosReturn r = p->commandLineExec(!(match_all || match_any), request, respbuffer);
					if (r != NSCAPI::cmd_return_codes::returnIgnored && !respbuffer.empty()) {
						LOG_DEBUG_CORE_STD("Module handled execution request: " + p->getName());
						found = true;
						if (match_any) {
							response = respbuffer;
							return NSCAPI::exec_return_codes::returnOK;
						}
						responses.push_back(respbuffer);
					}
				}
			} catch (plugin_exception e) {
				LOG_ERROR_CORE_STD("Could not execute command: " + e.reason() + " in " + e.file());
			}
		}
	}

	Plugin::ExecuteResponseMessage response_message;

	BOOST_FOREACH(std::string r, responses) {
		Plugin::ExecuteResponseMessage tmp;
		tmp.ParseFromString(r);
		for (int i = 0; i < tmp.payload_size(); i++) {
			Plugin::ExecuteResponseMessage::Response *r = response_message.add_payload();
			r->CopyFrom(tmp.payload(i));
		}
	}
	response_message.SerializeToString(&response);
	if (found)
		return NSCAPI::cmd_return_codes::isSuccess;
	return NSCAPI::cmd_return_codes::returnIgnored;
}

void nsclient::core::plugin_manager::register_submission_listener(unsigned int plugin_id, const char* channel) {
	channels_.register_listener(plugin_id, channel);
}

NSCAPI::errorReturn nsclient::core::plugin_manager::send_notification(const char* channel, std::string &request, std::string &response) {
	std::string schannel = channel;
	bool found = false;
	BOOST_FOREACH(std::string cur_chan, str::utils::split_lst(schannel, std::string(","))) {
		if (cur_chan == "noop") {
			found = true;
			nscapi::protobuf::functions::create_simple_submit_response_ok(cur_chan, "TODO", "seems ok", response);
			continue;
		}
		if (cur_chan == "log") {
			Plugin::SubmitRequestMessage msg;
			msg.ParseFromString(request);
			for (int i = 0; i < msg.payload_size(); i++) {
				LOG_INFO_CORE("Notification " + str::xtos(msg.payload(i).result()) + ": " + nscapi::protobuf::functions::query_data_to_nagios_string(msg.payload(i), nscapi::protobuf::functions::no_truncation));
			}
			found = true;
			nscapi::protobuf::functions::create_simple_submit_response_ok(cur_chan, "TODO", "seems ok", response);
			continue;
		}
		try {
			BOOST_FOREACH(nsclient::plugin_type p, channels_.get(cur_chan)) {
				try {
					p->handleNotification(cur_chan.c_str(), request, response);
				} catch (...) {
					LOG_ERROR_CORE("Plugin throw exception: " + p->get_alias_or_name());
				}
				found = true;
			}
		} catch (nsclient::plugins_list_exception &e) {
			LOG_ERROR_CORE("No handler for channel: " + schannel + ": " + utf8::utf8_from_native(e.what()));
			return NSCAPI::api_return_codes::hasFailed;
		} catch (const std::exception &e) {
			LOG_ERROR_CORE("No handler for channel: " + schannel + ": " + utf8::utf8_from_native(e.what()));
			return NSCAPI::api_return_codes::hasFailed;
		} catch (...) {
			LOG_ERROR_CORE("No handler for channel: " + schannel);
			return NSCAPI::api_return_codes::hasFailed;
		}
	}
	if (!found) {
		LOG_ERROR_CORE("No handler for channel: " + schannel + " channels: " + channels_.to_string());
		return NSCAPI::api_return_codes::hasFailed;
	}
	return NSCAPI::api_return_codes::isSuccess;
}


NSCAPI::errorReturn nsclient::core::plugin_manager::emit_event(const std::string &request) {
	Plugin::EventMessage em; em.ParseFromString(request);
	BOOST_FOREACH(const Plugin::EventMessage::Request &r, em.payload()) {
		bool has_matched = false;
		try {
			BOOST_FOREACH(nsclient::plugin_type p, event_subscribers_.get(r.event())) {
				try {
					p->on_event(request);
					has_matched = true;
				} catch (const std::exception &e) {
					LOG_ERROR_CORE("Failed to emit event to " + p->get_alias_or_name() + ": " + utf8::utf8_from_native(e.what()));
				} catch (...) {
					LOG_ERROR_CORE("Failed to emit event to " + p->get_alias_or_name() + ": UNKNOWN EXCEPTION");
				}
			}
		} catch (nsclient::plugins_list_exception &e) {
			LOG_ERROR_CORE("No handler for event: " + utf8::utf8_from_native(e.what()));
			return NSCAPI::api_return_codes::hasFailed;
		} catch (const std::exception &e) {
			LOG_ERROR_CORE("No handler for event: " + utf8::utf8_from_native(e.what()));
			return NSCAPI::api_return_codes::hasFailed;
		} catch (...) {
			LOG_ERROR_CORE("No handler for event");
			return NSCAPI::api_return_codes::hasFailed;
		}
		if (!has_matched) {
			LOG_DEBUG_CORE("No handler for event: " + r.event());
		}
	}
	return NSCAPI::api_return_codes::isSuccess;
}

struct metrics_fetcher {
	Plugin::MetricsMessage result;
	std::string buffer;
	metrics_fetcher() {
		result.add_payload();
	}

	Plugin::MetricsMessage::Response* get_root() {
		return result.mutable_payload(0);
	}
	void add_bundle(const Plugin::Common::MetricsBundle &b) {
		get_root()->add_bundles()->CopyFrom(b);
	}
	void fetch(nsclient::plugin_type p) {
		std::string buffer;
		p->fetchMetrics(buffer);
		Plugin::MetricsMessage payload;
		payload.ParseFromString(buffer);
		BOOST_FOREACH(const Plugin::MetricsMessage::Response &r, payload.payload()) {
			BOOST_FOREACH(const Plugin::Common::MetricsBundle &b, r.bundles()) {
				add_bundle(b);
			}
		}
	}
	void render() {
		get_root()->mutable_result()->set_code(Plugin::Common_Result_StatusCodeType_STATUS_OK);
		buffer = result.SerializeAsString();
	}
	void digest(nsclient::plugin_type p) {
		p->submitMetrics(buffer);
	}
};

void nsclient::core::plugin_manager::process_metrics(Plugin::Common::MetricsBundle bundle) {
	metrics_fetcher f;
	metrics_fetchers_.do_all(boost::bind(&metrics_fetcher::fetch, &f, _1));
	f.get_root()->add_bundles()->CopyFrom(bundle);
	f.render();
	metrics_submitetrs_.do_all(boost::bind(&metrics_fetcher::digest, &f, _1));
}

boost::filesystem::path nsclient::core::plugin_manager::get_filename(boost::filesystem::path folder, std::string module) {
	return dll::dll_impl::fix_module_name(folder / module);
}

void nsclient::core::plugin_manager::set_path(boost::filesystem::path path) {
	plugin_path_ = file_helpers::meta::make_preferred(path);
}
