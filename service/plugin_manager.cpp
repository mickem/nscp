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

#include <str/format.hpp>
#include <settings/settings_core.hpp>
#include <config.h>
#include "../libs/settings_manager/settings_manager_impl.h"

#include <nscapi/nscapi_protobuf_functions.hpp>

#include <boost/unordered_map.hpp>


std::list<std::string> nsclient::core::plugin_manager::list_commands() {
	return commands_.list_all();
}

bool contains_plugin(nsclient::core::plugin_manager::plugin_alias_list_type &ret, std::string alias, std::string plugin) {
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
	, metricsFetchers(log_instance_)
	, metricsSubmitetrs(log_instance_)
	, plugin_cache_(log_instance_)
	, event_subscribers_(log_instance_)
{
}

nsclient::core::plugin_manager::~plugin_manager() {
}

nsclient::core::plugin_manager::plugin_alias_list_type nsclient::core::plugin_manager::find_all_plugins(settings::instance_ptr settings, boost::filesystem::path pluginPath) {
	plugin_alias_list_type ret;

	settings::settings_interface::string_list list = settings->get_keys(MAIN_MODULES_SECTION);
	BOOST_FOREACH(std::string plugin, list) {
		std::string alias;
		try {
			alias = settings->get_string(MAIN_MODULES_SECTION, plugin, "");
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
	for (boost::filesystem::directory_iterator itr(pluginPath); itr != end_itr; ++itr) {
		if (!is_directory(itr->status())) {
			boost::filesystem::path file = itr->path().filename();
			if (NSCPlugin::is_module(pluginPath / file)) {
				const std::string module = NSCPlugin::file_to_module(file);
				if (!contains_plugin(ret, "", module))
					ret.insert(plugin_alias_list_type::value_type("", module));
			}
		}
	}
	return ret;
}

nsclient::core::plugin_manager::plugin_alias_list_type nsclient::core::plugin_manager::find_all_active_plugins(settings::instance_ptr settings) {
	plugin_alias_list_type ret;

	settings::settings_interface::string_list list = settings->get_keys(MAIN_MODULES_SECTION);
	BOOST_FOREACH(std::string plugin, list) {
		std::string alias;
		try {
			alias = settings->get_string(MAIN_MODULES_SECTION, plugin, "");
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

bool nsclient::core::plugin_manager::boot_load_plugin(std::string plugin, bool boot) {
	try {
		if (plugin.length() > 4 && plugin.substr(plugin.length() - 4) == ".dll")
			plugin = plugin.substr(0, plugin.length() - 4);

		std::string plugin_file = NSCPlugin::get_plugin_file(plugin);
		boost::filesystem::path pluginPath = path_->expand_path("${module-path}");
		boost::filesystem::path file = pluginPath / plugin_file;
		nsclient::core::plugin_manager::plugin_type instance;
		if (boost::filesystem::is_regular(file)) {
			instance = addPlugin(file, "");
		} else {
			if (plugin_file == "CheckTaskSched1.dll" || plugin_file == "CheckTaskSched2.dll") {
				LOG_ERROR_CORE_STD("Your loading the CheckTaskSched1/2 which has been renamed into CheckTaskSched, please update your config");
				plugin_file = "CheckTaskSched.dll";
				boost::filesystem::path file = pluginPath / plugin_file;
				if (boost::filesystem::is_regular(file)) {
					instance = addPlugin(file, "");
				}
			} else {
				LOG_ERROR_CORE_STD("Failed to load: " + plugin + "File not found: " + file.string());
				return false;
			}
		}
		if (boot) {
			try {
				if (!instance->load_plugin(NSCAPI::normalStart)) {
					LOG_ERROR_CORE_STD("Plugin refused to load: " + instance->get_description());
				}
			} catch (NSPluginException e) {
				LOG_ERROR_CORE_STD("Could not load plugin: " + e.reason() + ": " + e.file());
			} catch (...) {
				LOG_ERROR_CORE_STD("Could not load plugin: " + instance->get_description());
			}
		}
		return true;
	} catch (const NSPluginException &e) {
		LOG_ERROR_CORE_STD("Module (" + e.file() + ") was not found: " + e.reason());
	} catch (const std::exception &e) {
		LOG_ERROR_CORE_STD("Module (" + plugin + ") was not found: " + utf8::utf8_from_native(e.what()));
	} catch (...) {
		LOG_ERROR_CORE_STD("Module (" + plugin + ") was not found...");
	}
	return false;
}

/**
 * Unload all plug-ins (in reversed order)
 */
void nsclient::core::plugin_manager::unloadPlugins() {
	commands_.remove_all();
	channels_.remove_all();
	BOOST_FOREACH(plugin_type p, plugin_list_.get_plugins()) {
		try {
			if (p) {
				LOG_DEBUG_CORE_STD("Unloading plugin: " + p->get_alias_or_name() + "...");
				p->unload_plugin();
			}
		} catch (const NSPluginException &e) {
			LOG_ERROR_CORE_STD("Exception raised when unloading plugin: " + e.reason() + " in module: " + e.file());
		} catch (...) {
			LOG_ERROR_CORE("Unknown exception raised when unloading plugin");
		}
	}
	plugin_list_.clear();
}

void nsclient::core::plugin_manager::loadPlugins(NSCAPI::moduleLoadMode mode) {
	std::set<long> broken;
	BOOST_FOREACH(plugin_type plugin, plugin_list_.get_plugins()) {
		LOG_DEBUG_CORE_STD("Loading plugin: " + plugin->get_description());
		try {
			if (!plugin->load_plugin(mode)) {
				LOG_ERROR_CORE_STD("Plugin refused to load: " + plugin->get_description());
				broken.insert(plugin->get_id());
			}
		} catch (const NSPluginException &e) {
			broken.insert(plugin->get_id());
			LOG_ERROR_CORE_STD("Could not load plugin: " + e.reason() + ": " + e.file());
		} catch (const std::exception &e) {
			broken.insert(plugin->get_id());
			LOG_ERROR_CORE_STD("Could not load plugin: " + plugin->get_alias() + ": " + e.what());
		} catch (...) {
			broken.insert(plugin->get_id());
			LOG_ERROR_CORE_STD("Could not load plugin: " + plugin->get_description());
		}
	}
	BOOST_FOREACH(const long &id, broken) {
		plugin_list_.remove(id);
	}
}
/**
 * Load and add a plugin to various internal structures
 * @param plugin The plug-in instance to load. The pointer is managed by the
 */
nsclient::core::plugin_manager::plugin_type nsclient::core::plugin_manager::addPlugin(boost::filesystem::path file, std::string alias) {
	LOG_DEBUG_CORE_STD(NSCPlugin::get_plugin_file(file.string()));
	if (alias.empty()) {
		LOG_DEBUG_CORE_STD("adding " + file.string());
	} else {
		LOG_DEBUG_CORE_STD("adding " + file.string() + " (" + alias + ")");
	}
	// Check if this is a duplicate plugin (if so return that instance)
	plugin_type dup = plugin_list_.find_duplicate(file, alias);
	if (dup) {
		return dup;
	}

	plugin_type plugin(new NSCPlugin(plugin_list_.get_next_id(), file.normalize(), alias));
	plugin->load_dll();
	plugin_list_.append_plugin(plugin);
	if (plugin->hasCommandHandler()) {
		commands_.add_plugin(plugin);
	}
	if (plugin->hasNotificationHandler()) {
		channels_.add_plugin(plugin);
	}
	if (plugin->hasMetricsFetcher()) {
		metricsFetchers.add_plugin(plugin);
	}
	if (plugin->hasMetricsSubmitter()) {
		metricsSubmitetrs.add_plugin(plugin);
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
}

bool nsclient::core::plugin_manager::reload_plugin(const std::string module) {
	plugin_type plugin = plugin_list_.find_by_alias(module);
	if (plugin) {
		LOG_DEBUG_CORE_STD(std::string("Reloading: ") + plugin->get_alias_or_name());
		plugin->load_plugin(NSCAPI::reloadStart);
		return true;
	}
	return false;
}


std::string nsclient::core::plugin_manager::describeCommand(std::string command) {
	return commands_.describe(command).description;
}
std::list<std::string> nsclient::core::plugin_manager::getAllCommandNames() {
	return commands_.list_all();
}
void nsclient::core::plugin_manager::registerCommand(unsigned int id, std::string cmd, std::string desc) {
	return commands_.register_command(id, cmd, desc);
}

nsclient::core::plugin_manager::plugin_type nsclient::core::plugin_manager::find_plugin(const unsigned int plugin_id) {
	return plugin_list_.find_by_id(plugin_id);
}

struct command_chunk {
	nsclient::commands::plugin_type plugin;
	Plugin::QueryRequestMessage request;
};


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
			} catch (const NSPluginException &e) {
				errors.push_back("Could not execute command: " + e.reason() + " in " + e.file());
				return 1;
			}
		}
		try {
			boost::filesystem::path pluginPath = path_->expand_path("${module-path}");
			boost::filesystem::path file = NSCPlugin::get_filename(pluginPath, module);
			if (!boost::filesystem::is_regular(file)) {
				file = NSCPlugin::get_filename("./modules", module);
				LOG_DEBUG_CORE_STD("Found local plugin");
			}
			if (boost::filesystem::is_regular(file)) {
				plugin_type plugin = addPlugin(file, "");
				if (plugin) {
					LOG_DEBUG_CORE_STD("Loading plugin: " + plugin->get_alias_or_name() + "...");
					plugin->load_plugin(NSCAPI::dontStart);
					return fun(plugin);
				} else {
					errors.push_back("Failed to load: " + file.string());
					return 1;
				}
			} else {
				errors.push_back("File not found: " + file.string());
				return 1;
			}
		} catch (const NSPluginException &e) {
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
			} catch (NSPluginException e) {
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
		try {
			BOOST_FOREACH(nsclient::plugin_type p, event_subscribers_.get(r.event())) {
				try {
					p->on_event(request);
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
	}
	return NSCAPI::api_return_codes::isSuccess;
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
	metricsFetchers.remove_plugin(plugin_id);
	metricsSubmitetrs.remove_plugin(plugin_id);
	plugin->unload_plugin();
	plugin->unload_dll();
}
void nsclient::core::plugin_manager::load_plugin(const boost::filesystem::path &file, std::string alias) {
	try {
		plugin_type instance = addPlugin(file, alias);
		if (!instance) {
			LOG_DEBUG_CORE_STD("Failed to load " + file.string());
			return;
		}
		instance->load_plugin(NSCAPI::normalStart);
	} catch (const NSPluginException& e) {
		if (e.file().find("FileLogger") != std::string::npos) {
			LOG_DEBUG_CORE_STD("Failed to load " + file.string() + ": " + e.reason());
		} else {
			LOG_ERROR_CORE_STD("Failed to load " + file.string() + ": " + e.reason());
		}
	}
}
unsigned int nsclient::core::plugin_manager::add_plugin(unsigned int plugin_id) {
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

void nsclient::core::plugin_manager::process_metrics() {
	metrics_fetcher f;

	metricsFetchers.do_all(boost::bind(&metrics_fetcher::fetch, &f, _1));
	//ownMetricsFetcher(f.get_root());
	f.render();
	metricsSubmitetrs.do_all(boost::bind(&metrics_fetcher::digest, &f, _1));
}
