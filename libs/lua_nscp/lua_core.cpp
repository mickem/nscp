#include <boost/optional.hpp>

#include <nscapi/functions.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_plugin_wrapper.hpp>

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

NSCAPI::nagiosReturn lua::lua_runtime::on_query(std::string command, lua::script_information *information, lua::lua_traits::function_type c, bool simple, const std::string &request, std::string &response)
{
	lua_wrapper lua(prep_function(information, c));
	if (simple) {
		int args = 2;
		if (c.object_ref != 0)
			args = 3;
		std::list<std::string> argslist;
		nscapi::functions::parse_simple_query_request(argslist, request);
		lua.push_string(command);
		lua.push_array(argslist);
		std::string msg, perf;
		if (lua.pcall(args, LUA_MULTRET, 0) != 0) {
			NSC_LOG_ERROR_STD(_T("Failed to handle command: ") + utf8::cvt<std::wstring>(command) + _T(": ") + utf8::cvt<std::wstring>(lua.pop_string()));
			return NSCAPI::returnUNKNOWN;
		}
		NSCAPI::nagiosReturn ret = NSCAPI::returnUNKNOWN;
		int arg_count = lua.size();
		if (arg_count > 3) {
			NSC_LOG_ERROR_STD(_T("Invalid return: ") + utf8::cvt<std::wstring>(lua.dump_stack()));
		}
		if (arg_count > 2)
			perf = lua.pop_string();
		if (arg_count > 1)
			msg = lua.pop_string();
		if (arg_count > 0)
			ret = lua.pop_code();
		lua.gc(LUA_GCCOLLECT, 0);
		nscapi::functions::create_simple_query_response(command, ret, msg, perf, response);
		return ret;
	} else {
		int args = 2;
		if (c.object_ref != 0)
			args = 3;
		lua.push_string(command);
		lua.push_raw_string(request);
		if (lua.pcall(args, LUA_MULTRET, 0) != 0) {
			NSC_LOG_ERROR_STD(_T("Failed to handle command: ") + utf8::cvt<std::wstring>(command) + _T(": ") + utf8::cvt<std::wstring>(lua.pop_string()));
			return NSCAPI::returnUNKNOWN;
		}
		int arg_count = lua.size();
		if (arg_count > 2) {
			NSC_LOG_ERROR_STD(_T("Invalid return: ") + utf8::cvt<std::wstring>(lua.dump_stack()));
		}
		if (arg_count > 1)
			response = lua.pop_string();
		lua.gc(LUA_GCCOLLECT, 0);
		if (arg_count > 0)
			return lua.pop_code();
	}
	NSC_LOG_ERROR_STD(_T("No arguments returned from script."));
	return NSCAPI::returnUNKNOWN;
}

NSCAPI::nagiosReturn lua::lua_runtime::on_exec(std::string command, lua::script_information *information, lua::lua_traits::function_type function, bool simple, const std::string &request, std::string &response)
{
	throw lua::lua_exception("The method or operation is not implemented(on_exec).");
}

NSCAPI::nagiosReturn lua::lua_runtime::on_submit(std::string command, lua::script_information *information, lua::lua_traits::function_type function, bool simple, const std::string &request, std::string &response)
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
	lua_instance.append_path(base_path + "\\scripts\\lua\\lib\\?.lua;" + base_path + "scripts\\lua\\?;");
	if (lua_instance.loadfile(info->script) != 0)
		throw lua::lua_exception("Failed to load script: " + info->script + ": " + lua_instance.pop_string());
	if (lua_instance.pcall(0, 0, 0) != 0)
		throw lua::lua_exception("Failed to execute script: " + info->script + ": " + lua_instance.pop_string());
}
void lua::lua_runtime::unload(scripts::script_information<lua_traits> *info) {
	lua::lua_wrapper lua_instance(info->user_data.L);
	lua_instance.gc(LUA_GCCOLLECT, 0);
	lua_instance.remove_userdata(lua::lua_traits::user_data_tag);
}

