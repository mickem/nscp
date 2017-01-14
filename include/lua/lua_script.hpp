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

#pragma once

#include <map>

#include <boost/filesystem.hpp>
#include <boost/optional.hpp>

extern "C" {
#include <lua.h>
}
#include <luna.h>

#ifdef HAVE_LUA_PB
#include <nscapi/nscapi_protobuf.hpp>
#include <plugin.pb-lua.h>
#endif

#include <lua/lua_cpp.hpp>
#include <scripts/script_interface.hpp>

namespace lua {
	struct lua_traits {
		static const std::string user_data_tag;

		struct user_data_type {
			std::string base_path_;
			Lua_State L;
		};

		struct function {
			function() : object_ref(0), function_ref(0) {}
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
		core_wrapper(lua_State *L, bool fromLua);

		static const char className[];
		static const Luna<core_wrapper>::FunctionType Functions[];
		static const Luna<core_wrapper>::PropertyType Properties[];
		bool isExisting;
		bool isPrecious() { return false; }

		int create_pb_query(lua_State *L);
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
		registry_wrapper(lua_State *L, bool fromLua);

		static const char className[];
		static const Luna<registry_wrapper>::FunctionType Functions[];
		static const Luna<registry_wrapper>::PropertyType Properties[];
		bool isExisting;
		bool isPrecious() { return false; }

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
		settings_wrapper(lua_State *L, bool fromLua);

		static const char className[];
		static const Luna<settings_wrapper>::FunctionType Functions[];
		static const Luna<settings_wrapper>::PropertyType Properties[];
		bool isExisting;
		bool isPrecious() { return false; }

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
		static boost::optional<boost::filesystem::path> find_script(boost::filesystem::path root, std::string file);
	};
}