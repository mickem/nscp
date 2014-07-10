#include <types.hpp>
#include <unicode_char.hpp>

#include <string>
#include <functional>

#include <NSCAPI.h>
#include <nscapi/nscapi_plugin_wrapper.hpp>
#include <nscapi/nscapi_core_wrapper.hpp>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>




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
#include <stack>
#include <iostream>

#include <boost/assign.hpp>

#include "DotnetPlugins.h"

#include <strEx.h>
#include <utf8.hpp>

#include "Vcclr.h"
#include "json.h"
#include "block_allocator.h"

#include <tchar.h>

NSC_WRAPPERS_MAIN_DEF(DotnetPlugins, "dotnet");
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF();
NSC_WRAPPERS_CLI_DEF();
NSC_WRAPPERS_HANDLE_NOTIFICATION_DEF();


const std::string settings_path = "/modules/dotnet";
const std::string module_path = "${exe-path}/modules/dotnet";
const std::string factory_key = "factory class";
const std::string factory_default = "NSCP.Plugin.PluginFactory";

bool DotnetPlugins::settings_query(const std::string &request_json, std::string &response_json) {
	std::string request_message, response_message;
	if (!get_core()->json_to_protobuf(request_json, request_message)) {
		NSC_LOG_ERROR("Failed to convert request: " + request_json);
		return false;
	}
	if (!get_core()->settings_query(request_message, response_message)) {
		NSC_LOG_ERROR("Failed to query");
		return false;
	}
	if (!get_core()->protobuf_to_json("SettingsResponseMessage", response_message, response_json)) {
		NSC_LOG_ERROR("Failed to convert response");
		return false;
	}
	return true;
}

bool DotnetPlugins::registry_query(const std::string &request_json, std::string &response_json) {
	std::string request_message, response_message;
	if (!get_core()->json_to_protobuf(request_json, request_message)) {
		NSC_LOG_ERROR("Failed to convert request: " + request_json);
		return false;
	}
	if (!get_core()->registry_query(request_message, response_message)) {
		NSC_LOG_ERROR("Failed to query");
		return false;
	}
	if (!get_core()->protobuf_to_json("RegistryResponseMessage", response_message, response_json)) {
		NSC_LOG_ERROR("Failed to convert response");
		return false;
	}
	return true;
}

json_value* find_node(json_value *node, std::list<std::string> &trail) {
	if (trail.empty())
		return node;
	std::string target = trail.front();
	trail.pop_front();
	for (json_value *it = node->first_child; it; it = it->next_sibling) {
		if (it->name && target == it->name) {
			return find_node(it, trail);
		}
	}
	trail.push_front(target);
	for (json_value *it = node->first_child; it; it = it->next_sibling) {
		if (!it->name) {
			json_value * tmp = find_node(it, trail);
			if (tmp != NULL)
				return tmp;
		}
	}
	NSC_DEBUG_MSG("Failed to find node: " + target);
	return NULL;
}
std::string json_string(json_value *node) {
	if (node == NULL || node->type != JSON_STRING)
		return "";
	return node->string_value;
}
int json_int(json_value *node) {
	if (node == NULL || node->type != JSON_INT)
		return -1;
	return node->int_value;
}

std::list<std::string> json_list(json_value *node) {
	std::list<std::string> ret;
	if (node == NULL || node->type != JSON_ARRAY)
		return ret;
	for (json_value *it = node->first_child; it; it = it->next_sibling) {
		ret.push_back(json_string(it));
	}
	return ret;
}

struct json_object : boost::noncopyable{
	json_value* root;
	block_allocator allocator;
	char* buffer;
	std::string last_error;

	json_object() : root(NULL), allocator(1 << 10), buffer(NULL) {}
	~json_object() {
		if (buffer)
			delete [] buffer;
	}

	bool parse(const std::string &json) {
		char *errorPos = 0, *errorDesc = 0;
		int errorLine = 0;
		buffer = new char[json.size()+10];
		memcpy(buffer, json.c_str(), json.size()+1);
		root = json_parse(buffer, &errorPos, &errorDesc, &errorLine, &allocator);

		if (errorPos == 0)
			return true;
		last_error = std::string("Parsing failed: ") + errorDesc;
		return false;
	}

	std::string to_string(json_value* node = NULL, int level = 0) {
		if (node == NULL)
			node = root;
		std::stringstream ss;
		std::string pad(level, ' ');
		ss << pad;
		if (node->name)
		{
			ss << node->name << " = ";
		}

		switch(node->type)
		{
		case JSON_NULL:
			ss << "null\n";
			break;
		case JSON_STRING:
			ss << "\"" << node->string_value << "\"\n";
			break;
		case JSON_INT:
			ss << node->int_value << "\n";
			break;
		case JSON_FLOAT:
			ss << node->float_value << "\n";
			break;
		case JSON_BOOL:
			ss << (node->int_value ? "true\n" : "false\n");
			break;
		case JSON_OBJECT:
		case JSON_ARRAY:
			ss << (node->type==JSON_ARRAY?"[\n":"{\n");
			for (json_value *it = node->first_child; it; it = it->next_sibling) {
				ss << to_string(it, level+4);
			}
			ss << pad << (node->type==JSON_ARRAY?"]\n":"}\n");
			break;
		default:
			ss << "???\n";
		}
		return ss.str();
	}

};

void json_error(const json_object &json, const std::string &data) {
	std::cout << json.last_error << std::endl;
	std::cout << data << std::endl;
}



using namespace boost::assign;

std::list<std::string> DotnetPlugins::settings_get_list(const std::string path) {
	std::list<std::string> ret;
	std::string list_req = "{ \"type\": \"SettingsRequestMessage\", \"header\": { \"version\": \"1\"}, \"payload\": { \"plugin_id\": " + strEx::s::xtos(get_id()) + ", \"query\": { \"node\": { \"path\": \"" + path + "\"}, \"recursive\": false, \"type\": \"LIST\" } } }";
	std::string list_res;
	if (!settings_query(list_req, list_res)) {
		NSC_LOG_ERROR("Failed to query: " + path);
		return ret;
	}
	json_object json;
	if (!json.parse(list_res)) {
		json_error(json, list_res);
		return ret;
	}

	std::list<std::string> trail = list_of("payload")("query")("value")("list_data");
	return json_list(find_node(json.root, trail));
}

void DotnetPlugins::settings_reg_path(const std::string path, const std::string title, const std::string desc) {
	std::string req = "{ \"type\": \"SettingsRequestMessage\", \"header\": { \"version\": \"1\"}, \"payload\": { \"plugin_id\": " + strEx::s::xtos(get_id()) + ", \"registration\": { \"node\": { \"path\": \"" + path + "\"}, \"info\": { \"title\": \"" + title + "\", \"description\": \"" + desc + "\"} } } }";
	std::string res;
	if (!settings_query(req, res)) {
		NSC_LOG_ERROR("Failed to describe path: " + path);
		return;
	}
	json_object json;
	if (!json.parse(res)) {
		json_error(json, res);
		return;
	}
	std::list<std::string> trail = list_of("payload")("result")("status");
	json_value* resnode = find_node(json.root, trail);
	if (json_int(resnode) != 0) {
		NSC_LOG_ERROR("Failed to describe path: " + path + " (" + json_string(resnode) + ")");
	}
}

void DotnetPlugins::settings_reg_key(const std::string path, const std::string key, const std::string title, const std::string desc) {
	std::string req = "{ \"type\": \"SettingsRequestMessage\", \"header\": { \"version\": \"1\"}, \"payload\": { \"plugin_id\": " + strEx::s::xtos(get_id()) + ", \"registration\": { \"node\": { \"path\": \"" + path + "\", \"key\": \"" + key + "\"}, \"info\": { \"title\": \"" + title + "\", \"description\": \"" + desc + "\"} } } }";
	std::string res;
	if (!settings_query(req, res))
		std::cout << "Failed to describe key: " << path << "." << key << "\n";
	json_object json;
	if (!json.parse(res)) {
		json_error(json, res);
		return;
	}
	std::list<std::string> trail = list_of("payload")("result")("status");
	if (json_int(find_node(json.root, trail)) != 0) {
		std::cout << "Failed to describe key: " << path << "." << key << "\n";
	}
}

std::string DotnetPlugins::settings_get_string(const std::string path, const std::string key, const std::string value) {
	std::string req = "{ \"type\": \"SettingsRequestMessage\", \"header\": { \"version\": \"1\"}, \"payload\": { \"plugin_id\": " + strEx::s::xtos(get_id()) + ", \"query\": { \"node\": { \"path\": \"" + path + "\", \"key\": \"" + key + "\"}, \"type\": \"STRING\", \"default_value\": {\"type\": \"STRING\", \"string_data\": \"" + value + "\"} } } }";
	std::string res;
	if (!settings_query(req, res))
		std::cout << "Failed to describe key: " << path << "." << key << "\n";
	json_object json;
	if (!json.parse(res)) {
		json_error(json, res);
		return "";
	}
	std::list<std::string> trail = list_of("payload")("result")("status");
	if (json_int(find_node(json.root, trail)) != 0) {
		std::cout << "Failed to get key: " << path << "." << key << "\n";
		return "";
	}
	std::list<std::string> value_trail = list_of("payload")("query")("value")("string_data");
	return json_string(find_node(json.root, value_trail));
}
int DotnetPlugins::settings_get_int(const std::string path, const std::string key, const int value) {
	std::string req = "{ \"type\": \"SettingsRequestMessage\", \"header\": { \"version\": \"1\"}, \"payload\": { \"plugin_id\": " + strEx::s::xtos(get_id()) + ", \"query\": { \"node\": { \"path\": \"" + path + "\", \"key\": \"" + key + "\"}, \"type\": \"INT\", \"default_value\": {\"type\": \"INT\", \"int_data\": " + strEx::s::xtos(value) + "} } } }";
	std::string res;
	if (!settings_query(req, res))
		std::cout << "Failed to describe key: " << path << "." << key << "\n";
	json_object json;
	if (!json.parse(res)) {
		json_error(json, res);
		return -1;
	}
	std::list<std::string> trail = list_of("payload")("result")("status");
	if (json_int(find_node(json.root, trail)) != 0) {
		std::cout << "Failed to get key: " << path << "." << key << "\n";
		return -1;
	}
	std::list<std::string> value_trail = list_of("payload")("query")("value")("string_data");
	return json_int(find_node(json.root, value_trail));
}


int DotnetPlugins::registry_reg_module(const std::string module) {
	std::string req = "{ \"type\": \"RegistryRequestMessage\", \"header\": { \"version\": \"1\"}, \"payload\": {\"registration\": {  \"plugin_id\": " + strEx::s::xtos(get_id()) + ", \"type\": \"PLUGIN\", \"name\": \"" + module + "\" } } }";
	std::string res;
	if (!registry_query(req, res)) {
		NSC_LOG_ERROR("Failed to register module: " + module);
		return -1;
	}
	json_object json;
	if (!json.parse(res)) {
		json_error(json, res);
		return -1;
	}
	std::list<std::string> trail = list_of("payload")("registration")("item_id");
	return json_int(find_node(json.root, trail));
}

void DotnetPlugins::registry_reg_command(const std::string command, const std::string description, const int plugin_id) {
	std::string req = "{ \"type\": \"RegistryRequestMessage\", \"header\": { \"version\": \"1\"}, \"payload\": {\"registration\": {  \"plugin_id\": " + strEx::s::xtos(plugin_id) + ", \"type\": \"QUERY\", \"name\": \"" + command + "\", \"info\": { \"description\": \"" + description + "\"} } } }";
	std::string res;
	if (!registry_query(req, res)) {
		NSC_LOG_ERROR("Failed to register: " + command);
		return;
	}
	json_object json;
	if (!json.parse(res)) {
		json_error(json, res);
		return;
	}
	std::list<std::string> trail = list_of("payload")("result")("status");
	if (json_int(find_node(json.root, trail)) != 0) {
		NSC_LOG_ERROR("Failed to register: " + command);
	}
}



bool DotnetPlugins::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {
	root_path = utf8::cvt<std::wstring>(get_core()->expand_path(utf8::cvt<std::string>(module_path)));

	settings_reg_path(settings_path, "DOT NET MODULES", "Modules written in dotnet/CLR");

	BOOST_FOREACH(const std::string &s, settings_get_list("/modules/dotnet")) {
		settings_reg_key(settings_path, s, "DOT NET Module", "dotnet plugin");
		std::string v = settings_get_string(settings_path, s, "");
		if (mode == NSCAPI::normalStart) {
			load(s, v);
		}
	}
	return true;
}
bool DotnetPlugins::unloadModule() {
	BOOST_FOREACH(plugin_type p, plugins) {
		p->unload();
		plugin_instance.erase(p->get_id());
	}
	plugins.clear();
	return true;
}

void DotnetPlugins::load(std::string key, std::string val) {
	try {
		std::wstring alias = utf8::cvt<std::wstring>(key);
		std::wstring plugin = utf8::cvt<std::wstring>(val);
		if (val == "enabled") {
			plugin = alias;
			alias = std::wstring();
		}
		if (val.empty())
			plugin = alias;
		std::wstring factory = utf8::cvt<std::wstring>(settings_get_string(settings_path + "/" + key, factory_key, factory_default));
		int id = registry_reg_module(key);
		plugin_instance.add_alias(get_id(), id);
		NSC_LOG_ERROR("Adding: " + strEx::s::xtos((id)));
		plugin_type instance = plugin_instance::create(this, factory, plugin, root_path + _T("\\") + plugin, id);
		plugins.push_back(instance);
		instance->load(alias, 1);
	} catch(System::Exception ^e) {
		NSC_LOG_ERROR_STD("Failed to load module: " + to_nstring(e->ToString()));
	} catch(const std::exception &e) {
		NSC_LOG_ERROR_STD("Failed to load module: " + utf8::utf8_from_native(e.what()));
	} catch(...) {
		NSC_LOG_ERROR_STD("CLR failed to load!");
	}
}
bool DotnetPlugins::settings_register_key(std::wstring path, std::wstring key, NSCAPI::settings_type type, std::wstring title, std::wstring description, std::wstring defaultValue, bool advanced) {
	// 	get_core()->settings_register_key(id_, path, key, type, title, description, defaultValue, advanced);
	return true;
}
bool DotnetPlugins::settings_register_path(std::wstring path, std::wstring title, std::wstring description, bool advanced) {
	// 	get_core()->settings_register_path(id_, path, title, description, advanced);
	return true;
}

bool DotnetPlugins::register_command(std::wstring command, plugin_instance::plugin_type plugin, std::wstring description) {

	// 	commands[command] = plugin;
	// 	get_core()->registerCommand(id_, command, description);
	return true;
}
bool DotnetPlugins::register_channel(std::wstring channel, plugin_instance::plugin_type plugin) {
	// 	channels[channel] = plugin;
	// 	get_core()->registerSubmissionListener(id_, channel);
	return true;
}
nscapi::core_wrapper* DotnetPlugins::get_core() {
	return nscapi::plugin_singleton->get_core();
}

bool DotnetPlugins::hasCommandHandler() {
	return true;
}
bool DotnetPlugins::hasMessageHandler() {
	return false;
}
bool DotnetPlugins::hasNotificationHandler() {
	return false;
}

NSCAPI::nagiosReturn DotnetPlugins::handleRAWCommand(const std::string &request, std::string &response) {
	// 	std::wstring command(char_command);
	// 	try {
	// 		commands_type::const_iterator cit = commands.find(command);
	// 		if (cit == commands.end())
	// 			return NSCAPI::returnIgnored;
	// 		return cit->second->onCommand(command, request, response);
	// 	} catch(System::Exception ^e) {
	// 		NSC_LOG_ERROR_STD("Failed to execute command " + utf8::cvt<std::string>(command) + ": " + to_nsstring(e->ToString()));
	// 	} catch (const std::exception &e) {
	// 		NSC_LOG_ERROR_STD("Failed to execute command " + utf8::cvt<std::string>(command), e);
	// 	}
	return NSCAPI::returnIgnored;
}

NSCAPI::nagiosReturn DotnetPlugins::handleRAWNotification(const std::string &channel, std::string &request, std::string &response) {
	// 	try {
	// 		commands_type::const_iterator cit = channels.find(channel);
	// 		if (cit == channels.end())
	// 			return NSCAPI::returnIgnored;
	// 		return cit->second->onSubmit(channel, request, response);
	// 	} catch(System::Exception ^e) {
	// 		NSC_LOG_ERROR_STD("Failed to execute command " + utf8::cvt<std::string>(channel) + ": " + to_nsstring(e->ToString()));
	// 	} catch (const std::exception &e) {
	// 		NSC_LOG_ERROR_STD("Failed to execute command " + utf8::cvt<std::string>(channel), e);
	// 	}
	return NSCAPI::returnIgnored;
}

NSCAPI::nagiosReturn DotnetPlugins::commandRAWLineExec(const std::string &request, std::string &response) {
	return NSCAPI::returnIgnored;
}

#pragma managed(push, off)
BOOL APIENTRY DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved) {
	return TRUE;
}
#pragma managed(pop)
nscapi::helper_singleton* nscapi::plugin_singleton = new nscapi::helper_singleton();


