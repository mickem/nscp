#pragma once

extern "C" {
#include <lua.h>
#include "lauxlib.h"
#include "lualib.h"
}
#include "luna.h"

#include <string>
#include <list>

#include <boost/shared_ptr.hpp>

#include <NSCAPI.h>
#include <nscapi/nscapi_core_wrapper.hpp>

#include <strEx.h>
#include <scripts/script_interface.hpp>
#include <lua/lua_script.hpp>

namespace lua {

	typedef scripts::script_information<lua_traits> script_information;

	struct lua_runtime_plugin {
		virtual void load(lua::lua_wrapper &instance) = 0;
		virtual void unload(lua::lua_wrapper &instance) = 0;
	};
	typedef boost::shared_ptr<lua_runtime_plugin> lua_runtime_plugin_type;

	struct lua_runtime : public scripts::script_runtime_interface<lua::lua_traits> {
		std::string base_path;
		std::list<lua_runtime_plugin_type> plugins;

		lua_runtime(std::string base_path) : base_path(base_path) {}

		virtual void register_query(const std::string &command, const std::string &description);
		virtual void register_subscription(const std::string &channel, const std::string &description);

		virtual NSCAPI::nagiosReturn on_query(std::string command, script_information *information, lua::lua_traits::function_type function, bool simple, const std::string &request, std::string &response);
		virtual NSCAPI::nagiosReturn on_exec(std::string command, script_information *information, lua::lua_traits::function_type function, bool simple, const std::string &request, std::string &response);
		virtual NSCAPI::nagiosReturn on_submit(std::string command, script_information *information, lua::lua_traits::function_type function, bool simple, const std::string &request, std::string &response);

		virtual void load(scripts::script_information<lua_traits> *info);
		virtual void unload(scripts::script_information<lua_traits> *info);

		void register_plugin(lua_runtime_plugin_type plugin) {
			plugins.push_back(plugin);
		}

		static lua_State * prep_function(const lua::script_information *information, const lua::lua_traits::function_type &c) {
			lua_State *L = information->user_data.L;
			lua_rawgeti(L, LUA_REGISTRYINDEX, c.function_ref);
			if (c.object_ref != 0)
				lua_rawgeti(L, LUA_REGISTRYINDEX, c.object_ref);
			return L;
		}
		void create_user_data(script_information* info);
	};
}

