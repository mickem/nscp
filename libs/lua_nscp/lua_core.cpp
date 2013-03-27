#include <boost/optional.hpp>

#include <nscapi/functions.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_plugin_wrapper.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>

#include <lua/lua_cpp.hpp>
#include <lua/lua_core.hpp>

void lua::lua_runtime::register_query(const std::string &command, const std::string &description)
{
	throw lua::lua_exception("The method or operation is not implemented(reg_query).");
}

void lua::lua_runtime::register_subscription(const std::string &channel, const std::string &description)
{
	throw lua::lua_exception("The method or operation is not implemented(reg_sub).");
}

void lua::lua_runtime::on_query(std::string command, script_information *information, lua::lua_traits::function_type function, bool simple, const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response, const Plugin::QueryRequestMessage &request_message)
{
	lua_wrapper lua(prep_function(information, function));
	if (simple) {
		int args = 2;
		if (function.object_ref != 0)
			args = 3;
		std::list<std::string> argslist;
		for (int i=0;i<request.arguments_size();i++)
			argslist.push_back(request.arguments(i));
		lua.push_string(command);
		lua.push_array(argslist);
		std::string msg, perf;
		if (lua.pcall(args, LUA_MULTRET, 0) != 0)
			return nscapi::protobuf::functions::set_response_bad(*response, "Failed to handle command: " + command + ": " + lua.pop_string());
		NSCAPI::nagiosReturn ret = NSCAPI::returnUNKNOWN;
		int arg_count = lua.size();
		if (arg_count > 3) {
			NSC_LOG_ERROR_STD("Invalid return: " + lua.dump_stack());
		}
		if (arg_count > 2)
			perf = lua.pop_string();
		if (arg_count > 1)
			msg = lua.pop_string();
		if (arg_count > 0)
			ret = lua.pop_code();
		lua.gc(LUA_GCCOLLECT, 0);
		nscapi::protobuf::functions::append_simple_query_response_payload(response, command, ret, msg, perf);
	} else {
		int args = 2;
		if (function.object_ref != 0)
			args = 3;
		lua.push_string(command);
		lua.push_raw_string(request_message.SerializeAsString());
		if (lua.pcall(args, LUA_MULTRET, 0) != 0)
			return nscapi::protobuf::functions::set_response_bad(*response, "Failed to handle command: " + command + ": " + lua.pop_string());
		int arg_count = lua.size();
		if (arg_count > 2)
			return nscapi::protobuf::functions::set_response_bad(*response, "Invalid return: " + lua.dump_stack());
		if (arg_count > 1) {
			Plugin::QueryResponseMessage local_response;
			local_response.ParseFromString(lua.pop_string());
			if (local_response.payload_size() != 1)
				return nscapi::protobuf::functions::set_response_bad(*response, "Invalid response: " + command);
			response->CopyFrom(local_response.payload(0));
		}
		lua.gc(LUA_GCCOLLECT, 0);
		if (arg_count > 0)
			lua.pop_code();
	}
	return nscapi::protobuf::functions::set_response_bad(*response, "No arguments returned from script.");
}

NSCAPI::nagiosReturn lua::lua_runtime::on_exec(std::string command, script_information *information, lua::lua_traits::function_type function, bool simple, const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response)
{
	throw lua::lua_exception("The method or operation is not implemented(on_exec).");
}

NSCAPI::nagiosReturn lua::lua_runtime::on_submit(std::string command, script_information *information, lua::lua_traits::function_type function, bool simple, const Plugin::QueryResponseMessage::Response &request, Plugin::SubmitResponseMessage::Response *response)
{
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
	lua_instance.append_path(base_path + "\\scripts\\lua\\lib\\?.lua;" + base_path + "scripts\\lua\\?;");
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

