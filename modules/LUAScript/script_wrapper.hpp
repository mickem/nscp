#pragma once

#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>
#include <map>

#include "lua_wrappers.hpp"

#include <scripts/functions.hpp>

namespace script_wrapper {

	typedef lua_wrappers::lua_wrapper lua_wrapper;
	typedef lua_wrappers::lua_script_instance script_instance;
	typedef lua_wrappers::lua_instance_manager instance_manager;


	class base_script_object : boost::noncopyable {
	private:
		instance_manager::script_instance_type instance;

	public:
		base_script_object(lua_State *L) {
			instance = instance_manager::get_script(L);
		}
		instance_manager::script_instance_type get_instance() {
			if (!instance)
				throw lua_wrappers::LUAException("Invalid instance");
			return instance;
		}
	};

	class core_wrapper : public base_script_object {
	public:
		core_wrapper(lua_State *L) : base_script_object(L) {
			NSC_DEBUG_MSG(_T("get: "));
		}

		static const char className[];
		static const Luna<core_wrapper>::RegType methods[];

		int simple_query(lua_State *L) {
			lua_wrappers::lua_wrapper lua(L);
			try {
				int nargs = lua.size();
				if (nargs == 0)
					return lua.error("nscp.execute requires at least 1 argument!");
				const unsigned int argLen = nargs-1;
				std::list<std::wstring> arguments;
				for (unsigned int i=argLen; i>0; i--) {
					if (lua.is_table()) {
						std::list<std::wstring> table = lua.pop_array();
						arguments.insert(arguments.begin(), table.begin(), table.end());
					} else {
						arguments.push_front(lua.pop_string());
					}
				}
				std::wstring command = lua.pop_string();
				std::wstring message;
				std::wstring perf;
				NSCAPI::nagiosReturn ret = get_instance()->get_core()->simple_query(command, arguments, message, perf);
				lua.push_code(ret);
				lua.push_string(message);
				lua.push_string(perf);
				return lua.size();
			} catch (...) {
				return lua.error("Unknown exception in: simple_query");
			}
		}
		int query(lua_State *L) {
			lua_wrappers::lua_wrapper lua(L);
			NSC_LOG_ERROR_STD(_T("Unsupported API called: query"));
			return lua.error("Unsupported API called: query");
		}
		int simple_exec(lua_State *L) {
			lua_wrappers::lua_wrapper lua(L);
			try {
				int nargs = lua.size();
				std::wstring target = lua.wstring(1);
				std::wstring command = lua.wstring(2);
				std::list<std::wstring> arguments = lua.checkarray(3);
				std::list<std::wstring> result;
				NSCAPI::nagiosReturn ret = get_instance()->get_core()->exec_simple_command(target, command, arguments, result);
				lua.push_code(ret);
				lua.push_array(result);
				return 2;
			} catch (...) {
				return lua.error("Unknown exception in: simple_query");
			}
		}
		int exec(lua_State *L) {
			lua_wrappers::lua_wrapper lua(L);
			NSC_LOG_ERROR_STD(_T("Unsupported API called: exec"));
			return lua.error("Unsupported API called: exec");
		}
		int simple_submit(lua_State *L) {
			lua_wrappers::lua_wrapper lua(L);
			try {
				int nargs = lua.size();
				std::wstring channel = lua.wstring(1);
				std::wstring command = lua.wstring(2);
				NSCAPI::nagiosReturn code = lua.get_code(3);
				std::wstring message = lua.wstring(4);
				std::wstring perf = lua.wstring(5);
				std::wstring result;
				NSCAPI::nagiosReturn ret = get_instance()->get_core()->submit_simple_message(channel, command, code, message, perf, result);
				lua.push_code(ret);
				lua.push_string(result);
				return 2;
			} catch (...) {
				return lua.error("Unknown exception in: simple_query");
			}
		}
		int submit(lua_State *L) {
			lua_wrappers::lua_wrapper lua(L);
			NSC_LOG_ERROR_STD(_T("Unsupported API called: submit"));
			return lua.error("Unsupported API called: submit");
		}
		int reload(lua_State *L) {
			lua_wrappers::lua_wrapper lua(L);
			if (lua.size() > 1)
				return lua.error("Incorrect syntax: reload([<module>]);");
			std::wstring module = _T("module");
			if (lua.size() == 1)
				module = lua.pop_string();
			get_instance()->get_core()->reload(module);
			return 0;
		}
	};

	const char core_wrapper::className[] = "Core";
	const Luna<core_wrapper>::RegType core_wrapper::methods[] = {
		{ "simple_query", &core_wrapper::simple_query },
		{ "query", &core_wrapper::query },
		{ "simple_exec", &core_wrapper::simple_exec },
		{ "exec", &core_wrapper::exec },
		{ "simple_submit", &core_wrapper::simple_submit },
		{ "submit", &core_wrapper::submit },
		{ "reload", &core_wrapper::reload },
		{ 0 }
	};

	class registry_wrapper : public base_script_object {
	private:

	public:

		registry_wrapper(lua_State *L) : base_script_object(L) {}

		static const char className[];
		static const Luna<registry_wrapper>::RegType methods[];

		int register_function(lua_State *L) {
			lua_wrappers::lua_wrapper lua(L);
			NSC_LOG_ERROR_STD(_T("Unsupported API called: exec"));
			return lua.error("Unsupported API called: exec");
		}
		int register_simple_function(lua_State *L) {
			lua_wrapper lua(L);
			std::wstring description;
			if (lua.size() > 2)
				description = lua.pop_string();
			std::wstring name;
			if (lua.is_string()) {
				name = lua.pop_string();
				lua_getglobal(L, utf8::cvt<std::string>(name).c_str());
			}
			if (!lua.is_function())
				return lua.error("Invalid argument not a function: " + utf8::cvt<std::string>(name));

			int func_ref = luaL_ref(L, LUA_REGISTRYINDEX);

			if (func_ref == 0)
				return lua.error("Invalid function: " + utf8::cvt<std::string>(name));
			std::wstring script = lua.pop_string();
			if (description.empty()) 
				description = _T("Lua script: ") + script;
			get_instance()->get_core()->registerCommand(get_instance()->get_plugin_id(), script, description);
			get_instance()->get_registry()->register_query(script, get_instance(), func_ref);
			return 0;
		}
		int register_cmdline(lua_State *L) {
			lua_wrappers::lua_wrapper lua(L);
			NSC_LOG_ERROR_STD(_T("Unsupported API called: exec"));
			return lua.error("Unsupported API called: exec");
		}
		int register_simple_cmdline(lua_State *L) {
			lua_wrapper lua(L);
			std::wstring name;
			if (lua.is_string()) {
				name = lua.pop_string();
				lua_getglobal(L, utf8::cvt<std::string>(name).c_str());
			}
			if (!lua.is_function())
				return lua.error("Invalid argument not a function: " + utf8::cvt<std::string>(name));

			int func_ref = luaL_ref(L, LUA_REGISTRYINDEX);

			if (func_ref == 0)
				return lua.error("Invalid function: " + utf8::cvt<std::string>(name));
			std::wstring script = lua.pop_string();
			get_instance()->get_registry()->register_exec(script, get_instance(), func_ref);
			return 0;
		}
		int subscription(lua_State *L) {
			lua_wrappers::lua_wrapper lua(L);
			NSC_LOG_ERROR_STD(_T("Unsupported API called: exec"));
			return lua.error("Unsupported API called: exec");
		}
		int simple_subscription(lua_State *L) {
			lua_wrapper lua(L);
			std::wstring name;
			if (lua.is_string()) {
				name = lua.pop_string();
				lua_getglobal(L, utf8::cvt<std::string>(name).c_str());
			}
			if (!lua.is_function())
				return lua.error("Invalid argument not a function: " + utf8::cvt<std::string>(name));

			int func_ref = luaL_ref(L, LUA_REGISTRYINDEX);

			if (func_ref == 0)
				return lua.error("Invalid function: " + utf8::cvt<std::string>(name));
			std::wstring channel = lua.pop_string();
			get_instance()->get_core()->registerSubmissionListener(get_instance()->get_plugin_id(), channel);
			get_instance()->get_registry()->register_subscription(channel, get_instance(), func_ref);
			return 0;
		}
	};

	const char registry_wrapper::className[] = "Registry";
	const Luna<registry_wrapper>::RegType registry_wrapper::methods[] = {
		{ "function", &registry_wrapper::register_function },
		{ "simple_function", &registry_wrapper::register_simple_function },
		{ "cmdline", &registry_wrapper::register_cmdline },
		{ "simple_cmdline", &registry_wrapper::register_simple_cmdline },
		{ "subscription", &registry_wrapper::subscription },
		{ "simple_subscription", &registry_wrapper::simple_subscription },
		{ 0 }
	};



	class settings_wrapper : public base_script_object {
	public:

		settings_wrapper(lua_State *L) : base_script_object(L) {
			NSC_DEBUG_MSG(_T("create"));
		}

		static const char className[];
		static const Luna<settings_wrapper>::RegType methods[];

		int get_section(lua_State *L) {
			lua_wrapper lua(L);
			std::wstring v = lua.op_wstring(1);
			try {
				lua.push_array(get_instance()->get_core()->getSettingsSection(v));
			} catch (...) {
				return lua.error("Unknown exception getting section");
			}
			return 1;
		}
		int get_string(lua_State *L) {
			lua_wrapper lua(L);
			std::wstring s = lua.wstring(1);
			std::wstring k = lua.wstring(2);
			std::wstring v = lua.op_wstring(3);
			lua.push_string(get_instance()->get_core()->getSettingsString(s, k, v));
			return 1;
		}
		int set_string(lua_State *L) {
			lua_wrapper lua(L);
			std::wstring s = lua.wstring(1);
			std::wstring k = lua.wstring(2);
			std::wstring v = lua.wstring(3);
			get_instance()->get_core()->SetSettingsString(s, k, v);
			return 0;
		}
		int get_bool(lua_State *L) {
			lua_wrapper lua(L);
			std::wstring s = lua.wstring(1);
			std::wstring k = lua.wstring(2);
			bool v = lua.checkbool(3);
			lua.push_boolean(get_instance()->get_core()->getSettingsInt(s, k, v?1:0)==1);
			return 1;
		}
		int set_bool(lua_State *L) {
			lua_wrapper lua(L);
			std::wstring s = lua.wstring(1);
			std::wstring k = lua.wstring(2);
			bool v = lua.checkbool(3);
			get_instance()->get_core()->SetSettingsInt(s, k, v?1:0);
			return 0;
		}
		int get_int(lua_State *L) {
			lua_wrapper lua(L);
			std::wstring s = lua.wstring(1);
			std::wstring k = lua.wstring(2);
			int v = lua.checkint(3);
			lua.push_int(get_instance()->get_core()->getSettingsInt(s, k, v));
			return 1;
		}
		int set_int(lua_State *L) {
			lua_wrapper lua(L);
			std::wstring s = lua.wstring(1);
			std::wstring k = lua.wstring(2);
			int v = lua.checkint(3);
			get_instance()->get_core()->SetSettingsInt(s, k, v);
			return 0;
		}
		int save(lua_State *L) {
			get_instance()->get_core()->settings_save();
			return 0;
		}
		int register_path(lua_State *L) {
			lua_wrapper lua(L);
			std::wstring path = lua.wstring(1);
			std::wstring title = lua.wstring(1);
			std::wstring description = lua.wstring(1);
			get_instance()->get_core()->settings_register_path(path, title, description, false);
			return 0;
		}
		NSCAPI::settings_type script_wrapper::settings_wrapper::get_type(std::string stype) {
			if (stype == "string" || stype == "str" || stype == "s")
				return NSCAPI::key_string;
			if (stype == "integer" || stype == "int" || stype == "i")
				return NSCAPI::key_integer;
			if (stype == "bool" || stype == "b")
				return NSCAPI::key_bool;
			NSC_LOG_ERROR_STD(_T("Invalid settings type"));
			return NSCAPI::key_string;
		}

		int register_key(lua_State *L) {
			lua_wrapper lua(L);
			std::wstring path = lua.wstring(1);
			std::wstring key = lua.wstring(1);
			std::string stype = lua.string(1);
			NSCAPI::settings_type type = get_type(stype);
			std::wstring title = lua.wstring(1);
			std::wstring description = lua.wstring(1);
			std::wstring defaultValue = lua.wstring(1);
			get_instance()->get_core()->settings_register_key(path, key, type, title, description, defaultValue, false);
			return 0;
		}
		
	};

	const char settings_wrapper::className[] = "Settings";
	const Luna<settings_wrapper>::RegType settings_wrapper::methods[] = {
		{ "get_section", &settings_wrapper::get_section },
		{ "get_string", &settings_wrapper::get_string },
		{ "set_string", &settings_wrapper::set_string },
		{ "get_bool", &settings_wrapper::get_bool },
		{ "set_bool", &settings_wrapper::set_bool },
		{ "get_int", &settings_wrapper::get_int },
		{ "set_int", &settings_wrapper::set_int },
		{ "save", &settings_wrapper::save },
		{ "register_path", &settings_wrapper::register_path },
		{ "register_key", &settings_wrapper::register_key },
		{ 0 }
	};

	class nsclient_wrapper {
	public:

		static int execute (lua_State *L) {
			core_wrapper core(L);
			return core.simple_query(L);
		}

		static int register_command(lua_State *L) {
			registry_wrapper registry(L);
			return registry.register_simple_function(L);
		}

		static int getSetting (lua_State *L) {
			settings_wrapper sw(L);
			return sw.get_string(L);
		}
		static int getSection (lua_State *L) {
			settings_wrapper sw(L);
			return sw.get_section(L);
		}
		static int info (lua_State *L) {
			return log_any(L, NSCAPI::log_level::info);
		}
		static int error (lua_State *L) {
			return log_any(L, NSCAPI::log_level::error);
		}
		static int log_any(lua_State *L, int mode) {
			lua_wrapper lua(L);
			lua_wrapper::stack_trace trace = lua.get_stack_trace();
			int nargs = lua.size();
			std::wstring str;
			for (int i=0;i<nargs;i++) {
				str += lua.pop_string();
			}
			GET_CORE()->log(mode, utf8::cvt<std::string>(trace.first), trace.second, str);
			return 0;
		}

		static const luaL_Reg my_funcs[];

		static void luaopen(lua_State *L) {
			luaL_register(L, "nscp", my_funcs);
			lua_pop(L, 1);
			Luna<core_wrapper>::Register(L);
			Luna<registry_wrapper>::Register(L);
			Luna<settings_wrapper>::Register(L);
		}

	};
	const luaL_Reg nsclient_wrapper::my_funcs[] = {
		{"execute", execute},
		{"info", info},
		{"print", info},
		{"error", error},
		{"register", register_command},
		{"getSetting", getSetting},
		{"getSection", getSection},
		{NULL, NULL}
	};

	class lua_script : public script_instance, public boost::enable_shared_from_this<lua_script>  {
		lua_script(nscapi::core_wrapper* core, const int plugin_id, boost::shared_ptr<lua_wrappers::lua_registry> registry, const std::string alias, const std::string script) 
			: script_instance(core, plugin_id, registry, alias, script) {}
	public:

		static boost::shared_ptr<lua_script> create_instance(nscapi::core_wrapper* core, const int plugin_id, boost::shared_ptr<lua_wrappers::lua_registry> registry, const std::wstring alias, const std::wstring script) {
			boost::shared_ptr<lua_script> instance(new lua_script(core, plugin_id, registry, utf8::cvt<std::string>(alias), utf8::cvt<std::string>(script)));
			if (instance) {
				instance->init();
				instance->load();
			}
			return instance;
		}
		void init() {
			lua_wrappers::lua_instance_manager::set_script(get_lua_state(), shared_from_this());
		}

		void load() {
			lua_wrappers::lua_wrapper lua(get_lua_state());
			lua.openlibs();
			nsclient_wrapper::luaopen(get_lua_state());
			if (lua.loadfile(get_script()) != 0)
				throw lua_wrappers::LUAException(_T("Failed to load script: ") + get_wscript() + _T(": ") + lua.pop_string());
			if (lua.pcall(0, 0, 0) != 0)
				throw lua_wrappers::LUAException(_T("Failed to execute script: ") + get_wscript() + _T(": ") + lua.pop_string());
		}
		std::wstring get_wscript() const {
			return utf8::cvt<std::wstring>(get_script());
		}
		void unload() {}
		void reload() {
			unload();
			load();
		}
	};
}