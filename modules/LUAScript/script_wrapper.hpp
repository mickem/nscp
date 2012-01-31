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
			NSC_DEBUG_MSG(_T("s_q"));
			return 0;
		}
		int exec(lua_State *L) {
			lua_wrappers::lua_wrapper lua(L);
			NSC_LOG_ERROR_STD(_T("Unsupported API called: exec"));
			return lua.error("Unsupported API called: exec");
		}
		int simple_submit(lua_State *L) {
			NSC_DEBUG_MSG(_T("s_q"));
			return 0;
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
		//registry_wrapper(const registry_wrapper &other) {}
		//registry_wrapper& operator=(const registry_wrapper &other) {}

	public:

		registry_wrapper(lua_State *L) : base_script_object(L) {
			NSC_DEBUG_MSG(_T("create (from LUA)"));
		}
		/*
		registry_wrapper(nscapi::core_wrapper* core, unsigned int plugin_id) : core(core), plugin_id(plugin_id) {
			NSC_DEBUG_MSG(_T("create (from c++)"));
		}
		static boost::shared_ptr<registry_wrapper> create(unsigned int plugin_id) {
			return boost::shared_ptr<registry_wrapper>(new registry_wrapper(nscapi::plugin_singleton->get_core(), plugin_id));
		}
		*/

		static const char className[];
		static const Luna<registry_wrapper>::RegType methods[];

		int register_function(lua_State *L) {
			NSC_DEBUG_MSG(_T("register_function"));
			return 0;
		}
		int register_simple_function(lua_State *L) {
			NSC_DEBUG_MSG(_T("register_simple_function"));
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
			NSC_DEBUG_MSG(_T("register_cmdline"));
			return 0;
		}
		int register_simple_cmdline(lua_State *L) {
			NSC_DEBUG_MSG(_T("register_simple_cmdline"));
			return 0;
		}
		int subscription(lua_State *L) {
			NSC_DEBUG_MSG(_T("subscription"));
			return 0;
		}
		int simple_subscription(lua_State *L) {
			NSC_DEBUG_MSG(_T("simple_subscription"));
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
			int nargs = lua.size();
			if (nargs > 1)
				return lua.error("Incorrect syntax: get_section([<section>]);");
			std::wstring v;
			if (nargs > 0)
				v = lua.pop_string();
			try {
				lua.push_array(get_instance()->get_core()->getSettingsSection(v));
			} catch (...) {
				return lua.error("Unknown exception getting section");
			}
			return 1;
		}
		int get_string(lua_State *L) {
			lua_wrapper lua(L);
			int nargs = lua.size();
//			if (nargs < 2 || nargs > 3) {
//				return lua.error("Incorrect syntax: get_string(<section>, <key>[, <default value>]);" + utf8::cvt<std::string>(lua.dump_stack()));
//			}
			std::wstring v;
			if (nargs > 2)
				v = lua.pop_string();
			std::wstring k = lua.pop_string();
			std::wstring s = lua.pop_string();
			if (nargs > 1)
				lua.pop_string();
			lua.push_string(get_instance()->get_core()->getSettingsString(s, k, v));
			return 1;
		}
		int set_string(lua_State *L) {
			NSC_DEBUG_MSG(_T("set_string"));
			return 0;
		}
		int get_bool(lua_State *L) {
			lua_wrapper lua(L);
			int nargs = lua.size();
			if (nargs < 2 || nargs > 3)
				return lua.error("Incorrect syntax: get_string(<section>, <key>[, <default value>]);");
			bool v;
			if (nargs > 2)
				v = lua.pop_boolean();
			std::wstring k = lua.pop_string();
			std::wstring s = lua.pop_string();
			lua.push_boolean(get_instance()->get_core()->getSettingsInt(s, k, v?1:0)==1);
			return 1;
		}
		int set_bool(lua_State *L) {
			NSC_DEBUG_MSG(_T("set_bool"));
			return 0;
		}
		int get_int(lua_State *L) {
			NSC_DEBUG_MSG(_T("get_int"));
			return 0;
		}
		int set_int(lua_State *L) {
			NSC_DEBUG_MSG(_T("set_int"));
			return 0;
		}
		int save(lua_State *L) {
			NSC_DEBUG_MSG(_T("save"));
			return 0;
		}
		int register_path(lua_State *L) {
			NSC_DEBUG_MSG(_T("register_path"));
			return 0;
		}
		int register_key(lua_State *L) {
			NSC_DEBUG_MSG(_T("register_key"));
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
			try {
				lua_wrapper lua(L);
				int nargs = lua.size();
				if (nargs == 0)
					return lua.error("nscp.execute requires at least 1 argument!");
				unsigned int argLen = nargs-1;
				std::list<std::wstring> arguments;
				for (unsigned int i=argLen; i>0; i--)
					arguments.push_front(lua.pop_string());
				std::wstring command = lua.pop_string();
				std::wstring message;
				std::wstring perf;
				NSCAPI::nagiosReturn ret = GET_CORE()->simple_query(command, arguments, message, perf);
				lua.push_code(ret);
				lua.push_string(message);
				lua.push_string(perf);
				return 3;
			} catch (...) {
				return luaL_error(L, "Unknown exception in: nscp.execute");
			}
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
			: script_instance(core, plugin_id, registry, alias, script) {
			load();
		}
	public:

		static boost::shared_ptr<lua_script> create_instance(nscapi::core_wrapper* core, const int plugin_id, boost::shared_ptr<lua_wrappers::lua_registry> registry, const std::wstring alias, const std::wstring script) {
			boost::shared_ptr<lua_script> instance(new lua_script(core, plugin_id, registry, utf8::cvt<std::string>(alias), utf8::cvt<std::string>(script)));
			if (instance)
				instance->init();
			return instance;
		}
		void init() {
			lua_wrappers::lua_instance_manager::set_script(get_lua_state(), shared_from_this());
		}

		void load() {
			int i = lua_gettop(get_lua_state());
			luaL_openlibs(get_lua_state());
			i = lua_gettop(get_lua_state());
			nsclient_wrapper::luaopen(get_lua_state());
			i = lua_gettop(get_lua_state());
			if (luaL_loadfile(get_lua_state(), get_script().c_str()) != 0) {
				throw lua_wrappers::LUAException(_T("Failed to load script: ") + get_wscript() + _T(": ") + utf8::cvt<std::wstring>(lua_tostring(get_lua_state(), -1)));
			}
		}
		std::wstring get_wscript() const {
			return utf8::cvt<std::wstring>(get_script());
		}
		void unload() {}
		void reload() {
			unload();
			load();
			pre_load();
		}
		void pre_load() {
			if (lua_pcall(get_lua_state(), 0, 0, 0) != 0) {
				throw lua_wrappers::LUAException(_T("Failed to parse script: ") + get_wscript() + _T(": ") + utf8::cvt<std::wstring>(lua_tostring(get_lua_state(), -1)));
			}
		}


/*
		NSCAPI::nagiosReturn handleCommand(lua_handler *handler, std::wstring function, std::wstring cmd, std::list<std::wstring> arguments, std::wstring &msg, std::wstring &perf) {
			lua_manager::set_handler(L, handler);
			lua_manager::set_script(L, this);
			lua_wrapper lua(L);
			int nargs = lua.size();
			lua_getglobal(L, utf8::cvt<std::string>(function).c_str());
			if (!lua_isfunction(L, -1)) {
				lua_pop(L, 1); // remove function from LUA stack
				throw lua_wrappers::LUAException(_T("Failed to run script: ") + get_wscript() + _T(": Function not found: handle"));
			}
			lua.push_string(cmd); 
			lua.push_array(arguments);

			if (lua_pcall(L, 2, LUA_MULTRET, 0) != 0) {
				std::wstring err = lua.pop_string();
				NSC_LOG_ERROR_STD(_T("Failed to call main function in script: ") + get_wscript() + _T(": ") + err);
				return NSCAPI::returnUNKNOWN;
			}
			return extract_return(L, lua.size(), msg, perf);
		}
		*/
	};





}