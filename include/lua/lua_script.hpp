#pragma once

#include <map>

extern "C" {
#include <lua.h>
}
#include <luna.h>

#ifdef HAVE_LUA_PB
#include <plugin.pb-lua.h>
#endif

#include <lua/lua_cpp.hpp>
#include <scripts/script_interface.hpp>

//#include <scripts/functions.hpp>
//#include <nscapi/nscapi_core_helper.hpp>



namespace lua {

	struct lua_traits {

		static const std::string user_data_tag;

		struct user_data_type {
			std::string base_path_;
			Lua_State L;
		};

		struct function {
			//lua_State *L;
			int object_ref;
			int function_ref;
		};
		typedef function function_type;

	};


	typedef lua::lua_wrapper lua_wrapper;

	typedef scripts::script_information<lua_traits> script_information;
	typedef scripts::core_provider core_provider;
	typedef scripts::settings_provider settings_provider;
	typedef scripts::regitration_provider<lua_traits> regitration_provider;


	class core_wrapper {
	private:
		script_information *info;
	public:
		core_wrapper(lua_State *L);

		static const char className[];
		static const Luna<core_wrapper>::RegType methods[];

		int simple_query(lua_State *L);
		int query(lua_State *L);
		int simple_exec(lua_State *L);
		int exec(lua_State *L);
		int simple_submit(lua_State *L);
		int submit(lua_State *L);
		int reload(lua_State *L);
		int log(lua_State *L);
	private:
		boost::shared_ptr<core_provider> get();
	};

	struct registry_wrapper {
	private:
		script_information *info;
	public:
		registry_wrapper(lua_State *L);

		static const char className[];
		static const Luna<registry_wrapper>::RegType methods[];

		int register_function(lua_State *L);
		int register_simple_function(lua_State *L);
		int register_cmdline(lua_State *L);
		int register_simple_cmdline(lua_State *L);
		int subscription(lua_State *L);
		int simple_subscription(lua_State *L);
//	private:
//		boost::shared_ptr<regitration_provider> get();
	};

	class settings_wrapper {
	private:
		script_information *info;
	public:
		settings_wrapper(lua_State *L);

		static const char className[];
		static const Luna<settings_wrapper>::RegType methods[];

		int get_section(lua_State *L);
		int get_string(lua_State *L);
		int set_string(lua_State *L);
		int get_bool(lua_State *L);
		int set_bool(lua_State *L);
		int get_int(lua_State *L);
		int set_int(lua_State *L);
		int save(lua_State *L);
		int register_path(lua_State *L);
		int register_key(lua_State *L);
	private:
		boost::shared_ptr<settings_provider> get();
	};

	struct lua_script {
		static void luaopen(lua_State *L);
	};
/*
	class lua_script : public script_instance, public boost::enable_shared_from_this<lua_script>  {
		std::string base_path_;
		lua_script(nscapi::core_wrapper* core, const int plugin_id, boost::shared_ptr<lua::lua_registry> registry, const std::string alias, const std::string base_path, const std::string script) 
			: script_instance(core, plugin_id, registry, alias, script), base_path_(base_path) {
		}
	public:
		virtual ~lua_script() {
		}

		static boost::shared_ptr<lua_script> create_instance(nscapi::core_wrapper* core, const int plugin_id, boost::shared_ptr<lua::lua_registry> registry, const std::wstring alias, const std::wstring base_path, const std::wstring script) {
			boost::shared_ptr<lua_script> instance(new lua_script(core, plugin_id, registry, utf8::cvt<std::string>(alias), utf8::cvt<std::string>(base_path), utf8::cvt<std::string>(script)));
			if (instance) {
				instance->load();
			}
			return instance;
		}

		void load() {
			lua::lua_instance_manager::set_script(get_lua_state(), shared_from_this());
			lua::lua_wrapper lua(get_lua_state());
			lua.openlibs();
			nsclient_wrapper::luaopen(get_lua_state());
			lua.append_path(base_path_ + "\\scripts\\lua\\lib\\?.lua;" + base_path_ + "scripts\\lua\\?;");
			if (lua.loadfile(get_script()) != 0)
				throw lua::LUAException(_T("Failed to load script: ") + get_wscript() + _T(": ") + lua.pop_string());
			if (lua.pcall(0, 0, 0) != 0)
				throw lua::LUAException(_T("Failed to execute script: ") + get_wscript() + _T(": ") + lua.pop_string());
		}
		std::wstring get_wscript() const {
			return utf8::cvt<std::wstring>(get_script());
		}
		void unload() {
			lua::lua_wrapper lua(get_lua_state());
			lua.gc(LUA_GCCOLLECT, 0);
			lua::lua_instance_manager::remove_script(shared_from_this());
		}
		void reload() {
			unload();
			load();
		}
	};
	*/
}