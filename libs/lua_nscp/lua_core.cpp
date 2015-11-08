#include <boost/optional.hpp>

#include <nscapi/functions.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_plugin_wrapper.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>

#include <lua/lua_cpp.hpp>
#include <lua/lua_core.hpp>

void lua::lua_runtime::register_query(const std::string &command, const std::string &description) {
	throw lua::lua_exception("The method or operation is not implemented(reg_query).");
}

void lua::lua_runtime::register_subscription(const std::string &channel, const std::string &description) {
	throw lua::lua_exception("The method or operation is not implemented(reg_sub).");
}

void lua::lua_runtime::on_query(std::string command, script_information *information, lua::lua_traits::function_type function, bool simple, const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response, const Plugin::QueryRequestMessage &request_message) {
	lua_wrapper lua(prep_function(information, function));
	int args = 2;
	if (function.object_ref != 0)
		args = 3;
	if (simple) {
		std::list<std::string> argslist;
		for (int i = 0; i < request.arguments_size(); i++)
			argslist.push_back(request.arguments(i));
		lua.push_string(command);
		lua.push_array(argslist);
		if (lua.pcall(args, 3, 0) != 0)
			return nscapi::protobuf::functions::set_response_bad(*response, "Failed to handle command: " + command + ": " + lua.pop_string());
		NSCAPI::nagiosReturn ret = NSCAPI::query_return_codes::returnUNKNOWN;
		if (lua.size() < 3) {
			NSC_LOG_ERROR_STD("Invalid return: " + lua.dump_stack());
			nscapi::protobuf::functions::append_simple_query_response_payload(response, command, NSCAPI::query_return_codes::returnUNKNOWN, "Invalid return", "");
			return;
		}
		std::string msg, perf;
		perf = lua.pop_string();
		msg = lua.pop_string();
		ret = lua.pop_code();
		lua.gc(LUA_GCCOLLECT, 0);
		nscapi::protobuf::functions::append_simple_query_response_payload(response, command, ret, msg, perf);
	} else {
		lua.push_string(command);
		lua.push_raw_string(request.SerializeAsString());
		lua.push_raw_string(request_message.SerializeAsString());
		args++;
		if (lua.pcall(args, 1, 0) != 0)
			return nscapi::protobuf::functions::set_response_bad(*response, "Failed to handle command: " + command + ": " + lua.pop_string());
		if (lua.size() < 1) {
			NSC_LOG_ERROR_STD("Invalid return: " + lua.dump_stack());
			nscapi::protobuf::functions::append_simple_query_response_payload(response, command, NSCAPI::query_return_codes::returnUNKNOWN, "Invalid return data", "");
			return;
		}
		Plugin::QueryResponseMessage local_response;
		std::string data = lua.pop_raw_string();
		response->ParseFromString(data);
		lua.gc(LUA_GCCOLLECT, 0);
	}
}

void lua::lua_runtime::exec_main(script_information *information, const std::vector<std::string> &opts, Plugin::ExecuteResponseMessage::Response *response) {
	lua_wrapper lua(prep_function(information, "main"));
	lua.push_array(opts);
	if (lua.pcall(1, 2, 0) != 0)
		return nscapi::protobuf::functions::set_response_bad(*response, "Failed to handle command main: " + lua.pop_string());
	NSCAPI::nagiosReturn ret = NSCAPI::exec_return_codes::returnERROR;
	if (lua.size() < 2) {
		NSC_LOG_ERROR_STD("Invalid return: " + lua.dump_stack());
		nscapi::protobuf::functions::append_simple_exec_response_payload(response, "", NSCAPI::exec_return_codes::returnERROR, "Invalid return");
		return;
	}
	std::string msg;
	msg = lua.pop_string();
	ret = lua.pop_code();
	lua.gc(LUA_GCCOLLECT, 0);
	nscapi::protobuf::functions::append_simple_exec_response_payload(response, "", ret, msg);
}
void lua::lua_runtime::on_exec(std::string command, script_information *information, lua::lua_traits::function_type function, bool simple, const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response, const Plugin::ExecuteRequestMessage &request_message) {
	lua_wrapper lua(prep_function(information, function));
	int args = 2;
	if (function.object_ref != 0)
		args = 3;
	if (simple) {
		std::list<std::string> argslist;
		for (int i = 0; i < request.arguments_size(); i++)
			argslist.push_back(request.arguments(i));
		lua.push_string(command);
		lua.push_array(argslist);
		if (lua.pcall(args, 3, 0) != 0)
			return nscapi::protobuf::functions::set_response_bad(*response, "Failed to handle command: " + command + ": " + lua.pop_string());
		NSCAPI::nagiosReturn ret = NSCAPI::exec_return_codes::returnERROR;
		if (lua.size() < 3) {
			NSC_LOG_ERROR_STD("Invalid return: " + lua.dump_stack());
			nscapi::protobuf::functions::append_simple_exec_response_payload(response, command, NSCAPI::exec_return_codes::returnERROR, "Invalid return");
			return;
		}
		std::string msg, perf;
		msg = lua.pop_string();
		ret = lua.pop_code();
		lua.gc(LUA_GCCOLLECT, 0);
		nscapi::protobuf::functions::append_simple_exec_response_payload(response, command, ret, msg);
	} else {
		lua.push_string(command);
		lua.push_raw_string(request.SerializeAsString());
		lua.push_raw_string(request_message.SerializeAsString());
		args++;
		if (lua.pcall(args, 1, 0) != 0)
			return nscapi::protobuf::functions::set_response_bad(*response, "Failed to handle command: " + command + ": " + lua.pop_string());
		if (lua.size() < 1) {
			NSC_LOG_ERROR_STD("Invalid return: " + lua.dump_stack());
			nscapi::protobuf::functions::append_simple_exec_response_payload(response, command, NSCAPI::exec_return_codes::returnERROR, "Invalid return data");
			return;
		}
		Plugin::QueryResponseMessage local_response;
		std::string data = lua.pop_raw_string();
		response->ParseFromString(data);
		lua.gc(LUA_GCCOLLECT, 0);
	}
}

NSCAPI::nagiosReturn lua::lua_runtime::on_submit(std::string command, script_information *information, lua::lua_traits::function_type function, bool simple, const Plugin::QueryResponseMessage::Response &request, Plugin::SubmitResponseMessage::Response *response) {
	throw lua::lua_exception("The method or operation is not implemented(on_submit).");
}

void lua::lua_runtime::create_user_data(scripts::script_information<lua_traits> *info) {
	info->user_data.base_path_ = base_path;
}

void lua::lua_runtime::load(scripts::script_information<lua_traits> *info) {
	std::string base_path = info->user_data.base_path_;
	lua::lua_wrapper lua_instance(info->user_data.L);
	lua_instance.set_userdata(lua::lua_traits::user_data_tag, info);
	lua_instance.openlibs();
	lua::lua_script::luaopen(info->user_data.L);
	BOOST_FOREACH(lua::lua_runtime_plugin_type &plugin, plugins) {
		plugin->load(lua_instance);
	}
	lua_instance.append_path(base_path + "/scripts/lua/lib/?.lua;" + base_path + "scripts/lua/?;");
	if (lua_instance.loadfile(info->script) != 0)
		throw lua::lua_exception("Failed to load script: " + info->script + ": " + lua_instance.pop_string());
	if (lua_instance.pcall(0, 0, 0) != 0)
		throw lua::lua_exception("Failed to execute script: " + info->script + ": " + lua_instance.pop_string());
}
void lua::lua_runtime::unload(scripts::script_information<lua_traits> *info) {
	lua::lua_wrapper lua_instance(info->user_data.L);
	BOOST_FOREACH(lua::lua_runtime_plugin_type &plugin, plugins) {
		plugin->unload(lua_instance);
	}
	lua_instance.gc(LUA_GCCOLLECT, 0);
	lua_instance.remove_userdata(lua::lua_traits::user_data_tag);
}