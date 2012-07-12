#include "lua_wrappers.hpp"
#include <nscapi/functions.hpp>

#include <boost/optional.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_plugin_wrapper.hpp>


lua_wrappers::lua_instance_manager::script_map_type lua_wrappers::lua_instance_manager::scripts;

NSCAPI::nagiosReturn lua_wrappers::lua_wrapper::get_code(int pos) {
	std::string str;
	if (pos == -1)
		pos = lua_gettop(L);
	if (pos == 0)
		return NSCAPI::returnUNKNOWN;
	switch (lua_type(L, pos)) {
				case LUA_TNUMBER: 
					return static_cast<int>(lua_tonumber(L, pos));
				case LUA_TSTRING:
					return string_to_code(lua_tostring(L, pos));
				case LUA_TBOOLEAN:
					return lua_toboolean(L, pos)?NSCAPI::returnOK:NSCAPI::returnCRIT;
	}
	NSC_LOG_ERROR_STD(_T("Incorrect type: should be error, ok, warning or unknown: ") + strEx::itos(lua_type(L, pos)));
	return NSCAPI::returnUNKNOWN;
}

NSCAPI::nagiosReturn lua_wrappers::lua_wrapper::string_to_code(std::string str) {
	if ((str == "critical")||(str == "crit")||(str == "error")) {
		return NSCAPI::returnCRIT;
	} else if ((str == "warning")||(str == "warn")) {
		return NSCAPI::returnWARN;
	} else if (str == "ok") {
		return NSCAPI::returnOK;
	} else if (str == "unknown") {
		return NSCAPI::returnUNKNOWN;
	}
	NSC_LOG_ERROR_STD(_T("Invalid code: ") + utf8::to_unicode(str));
	return NSCAPI::returnUNKNOWN;
}

void lua_wrappers::lua_wrapper::log_stack() {
	int args = size();
	NSC_DEBUG_MSG_STD(_T("Invalid lua stack state, dumping stack"));
	for (int i=1;i<args+1;i++) {
		NSC_DEBUG_MSG_STD(get_type_as_string(i) +_T(": ") + get_string(i));
	}
}

NSCAPI::nagiosReturn lua_wrappers::lua_registry::on_query(const wchar_t* command, const std::string &request, std::string &response) {
	function_map::iterator it = functions.find(command);
	if (it == functions.end())
		throw LUAException(std::wstring(_T("Invalid function: ")) + command);
	function_container c = it->second;
	lua_wrapper lua(prep_function(c));
	if (c.simple) {
		nscapi::functions::decoded_simple_command_data data = nscapi::functions::parse_simple_query_request(command, request);
		lua.push_string(command);
		lua.push_array(data.args);
		std::wstring msg, perf;
		if (lua.pcall(2, LUA_MULTRET, 0) != 0) {
			NSC_LOG_ERROR_STD(_T("Failed to handle command: ") + command + _T(": ") + lua.pop_string());
			return NSCAPI::returnUNKNOWN;
		}
		NSCAPI::nagiosReturn ret;
		int arg_count = lua.size();
		if (arg_count > 2)
			perf = lua.pop_string();
		if (arg_count > 1)
			msg = lua.pop_string();
		if (arg_count > 0)
			ret = lua.pop_code();
		nscapi::functions::create_simple_query_response(command, ret, msg, perf, response);
		return ret;
	} else {
		lua.push_string(command);
		lua.push_raw_string(request);
		if (lua.pcall(2, LUA_MULTRET, 0) != 0) {
			NSC_LOG_ERROR_STD(_T("Failed to handle command: ") + command + _T(": ") + lua.pop_string());
			return NSCAPI::returnUNKNOWN;
		}
		int arg_count = lua.size();
		if (arg_count > 1)
			response = utf8::cvt<std::string>(lua.pop_string());
		if (arg_count > 0)
			return lua.pop_code();
	}
	NSC_LOG_ERROR_STD(_T("No arguments returned from script."));
	return NSCAPI::returnUNKNOWN;
}

NSCAPI::nagiosReturn lua_wrappers::lua_registry::on_exec(const std::wstring & command, std::list<std::wstring> & arguments, std::wstring & result) {
	function_map::iterator it = execs.find(command);
	if (it == execs.end())
		throw LUAException(_T("Invalid function: ") + command);
	function_container c = it->second;
	lua_wrapper lua(prep_function(c));
	lua.push_string(command);
	lua.push_array(arguments);
	if (lua.pcall(2, LUA_MULTRET, 0) != 0) {
		NSC_LOG_ERROR_STD(_T("Failed to handle command: ") + command + _T(": ") + lua.pop_string());
		return NSCAPI::returnUNKNOWN;
	}
	int arg_count = lua.size();
	if (arg_count > 1)
		result = lua.pop_string();
	if (arg_count > 0)
		return lua.pop_code();
	NSC_LOG_ERROR_STD(_T("No arguments returned from script."));
	return NSCAPI::returnUNKNOWN;
}

NSCAPI::nagiosReturn lua_wrappers::lua_registry::on_submission(const std::wstring channel, const std::wstring source, const std::wstring command, NSCAPI::nagiosReturn code, std::wstring msg, std::wstring perf) {
	function_map::iterator it = channels.find(channel);
	if (it == channels.end())
		throw LUAException(_T("Invalid function: ") + channel);
	function_container c = it->second;
	lua_wrapper lua(prep_function(c));
	lua.push_string(source);
	lua.push_string(command);
	lua.push_code(code);
	lua.push_string(msg);
	lua.push_string(perf);
	if (lua.pcall(5, LUA_MULTRET, 0) != 0) {
		NSC_LOG_ERROR_STD(_T("Failed to handle command: ") + command + _T(": ") + lua.pop_string());
		return NSCAPI::returnUNKNOWN;
	}
	if (lua.size() > 0)
		return lua.pop_code();
	NSC_LOG_ERROR_STD(_T("No arguments returned from script."));
	return NSCAPI::returnUNKNOWN;
}
