#pragma once

#include <scripts/functions.hpp>
#include <scripts/script_interface.hpp>
#include <scripts/script_nscp.hpp>

#include <lua/lua_script.hpp>
#include <lua/lua_core.hpp>

#include <check_mk/data.hpp>

namespace check_mk {
	class check_mk_lua_wrapper {
	private:
		lua::script_information *info;
	public:
		check_mk_lua_wrapper(lua_State *L, bool fromLua);

		static const char className[];
		static const Luna<check_mk_lua_wrapper>::PropertyType Properties[];
		static const Luna<check_mk_lua_wrapper>::FunctionType Functions[];
		bool isExisting;
		bool isPrecious() { return false; }

		int client_callback(lua_State *L);
		int server_callback(lua_State *L);
	};

	class check_mk_packet_wrapper {
	public:
		check_mk_packet_wrapper(lua_State *L, bool fromLua) {}

		static const char className[];
		static const Luna<check_mk_packet_wrapper>::PropertyType Properties[];
		static const Luna<check_mk_packet_wrapper>::FunctionType Functions[];
		bool isExisting;
		bool isPrecious() { return false; }

		int size_section(lua_State *L);
		int get_section(lua_State *L);
		int add_section(lua_State *L);


		check_mk::packet packet;
	};


	class check_mk_section_wrapper {
	public:
		check_mk_section_wrapper(lua_State *L, bool fromLua) {}

		static const char className[];
		static const Luna<check_mk_section_wrapper>::PropertyType Properties[];
		static const Luna<check_mk_section_wrapper>::FunctionType Functions[];
		bool isExisting;
		bool isPrecious() { return false; }

		int get_title(lua_State *L);
		int set_title(lua_State *L);
		int size_line(lua_State *L);
		int get_line(lua_State *L);
		int add_line(lua_State *L);

		check_mk::packet::section section;
	};
	class check_mk_line_wrapper {
	public:
		check_mk_line_wrapper(lua_State *L, bool fromLua) {}

		static const char className[];
		static const Luna<check_mk_line_wrapper>::PropertyType Properties[];
		static const Luna<check_mk_line_wrapper>::FunctionType Functions[];
		bool isExisting;
		bool isPrecious() { return false; }

		int get_line(lua_State *L);
		int set_line(lua_State *L);
		int size_item(lua_State *L);
		int get_item(lua_State *L);
		int add_item(lua_State *L);

		check_mk::packet::section::line line;
	};

	struct check_mk_plugin : public lua::lua_runtime_plugin {
		void load(lua::lua_wrapper &instance);
		void unload(lua::lua_wrapper &instance);
	};

}