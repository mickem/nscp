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

#include <boost/noncopyable.hpp>

extern "C" {
#include <lua.h>
}

#include <string>
#include <list>
#include <vector>
#include <NSCAPI.h>

namespace lua {
	class Lua_State : boost::noncopyable {
		lua_State *L;
	public:
		Lua_State();
		~Lua_State();

		// implicitly act as a lua_State pointer
		inline operator lua_State*() const {
			return L;
		}
		lua_State* get_state() const {
			return L;
		}
	};

	class lua_wrapper {
		lua_State *L;
	public:

		lua_wrapper(lua_State *L) : L(L) {}
		inline operator lua_State*() const {
			return L;
		}
		lua_State* get_state() const {
			return L;
		}

		int append_path(const std::string &path);

		//////////////////////////////////////////////////////////////////////////
		/// get_xxx
		std::string get_string(int pos = -1);
		std::string get_raw_string(int pos = -1);
		bool get_string(std::string &str, int pos = -1);
		bool get_raw_string(std::string &str, int pos = -1);
		int get_int(int pos = -1);
		bool get_boolean(int pos = -1);
		NSCAPI::nagiosReturn get_code(int pos = -1);
		std::list<std::string> get_array(const int pos = -1);
		void* get_raw_userdata(std::string id);
		template<class T>
		T get_userdata(std::string id) {
			return reinterpret_cast<T>(get_raw_userdata(id));
		}
		void set_raw_userdata(std::string id, void* data);
		template<class T>
		void set_userdata(std::string id, T data) {
			set_raw_userdata(id, reinterpret_cast<void*>(data));
		}
		void remove_userdata(std::string id);

		//////////////////////////////////////////////////////////////////////////
		/// pop_xxx
		bool pop_boolean();
		std::string pop_string();
		std::string pop_raw_string();
		bool pop_string(std::string &str);
		bool pop_raw_string(std::string &str);
		bool pop_function_ref(int &funref);
		bool pop_instance_ref(int &funref);
		int pop_int();
		NSCAPI::nagiosReturn pop_code();
		std::list<std::string> pop_array();

		//////////////////////////////////////////////////////////////////////////
		// Converters
		NSCAPI::nagiosReturn string_to_code(std::string str);

		////////////////////////////////////////////////////////////////////////////
		// Misc
		void getglobal(const std::string &name) {
			lua_getglobal(L, name.c_str());
		}
		inline void pop(int count = 1) {
			lua_pop(L, count);
		}
		int type(int pos = -1);
		std::string get_type_as_string(int pos = -1);

		inline bool is_string(int pos = -1) {
			return type(pos) == LUA_TSTRING;
		}
		inline bool is_function(int pos = -1) {
			return type(pos) == LUA_TFUNCTION;
		}
		inline bool is_number(int pos = -1) {
			return type(pos) == LUA_TNUMBER;
		}
		inline bool is_nil(int pos = -1) {
			return type(pos) == LUA_TNIL;
		}
		inline bool is_boolean(int pos = -1) {
			return type(pos) == LUA_TBOOLEAN;
		}
		inline bool is_table(int pos = -1) {
			return type(pos) == LUA_TTABLE;
		}

		//////////////////////////////////////////////////////////////////////////
		// push_xxx
		void push_code(NSCAPI::nagiosReturn code);
		void push_string(std::string s);
		void push_boolean(bool b);
		void push_int(int b);
		void push_raw_string(std::string s);
		void push_array(const std::list<std::string> &arr);
		void push_array(const std::vector<std::string> &arr);
		int size();
		bool empty();
		void log_stack();
		int error(std::string s);
		typedef std::pair<std::string, int> stack_trace;
		stack_trace get_stack_trace(int level = 1);
		std::string dump_stack();

		void openlibs();
		int loadfile(std::string script);
		int pcall(int nargs, int nresults, int errfunc);
		std::string op_string(int pos, std::string def = "");

		std::string check_string(int pos);
		std::list<std::string> check_array(int pos);
		bool check_bool(int pos);
		int  op_int(int pos, int def = 0);
		int checkint(int pos);
		int gc(int what, int data);

		template<class T>
		T* newuserdata() {
			return reinterpret_cast<T*>(lua_newuserdata(L, sizeof(T)));
		}
	};

	class lua_exception : public std::exception {
		std::string error_;
	public:
		lua_exception(std::string error) : error_(error) {}
		~lua_exception() throw() {}
		const char* what() const throw() {
			return error_.c_str();
		}
	};

	std::string w2s(std::wstring s);
	std::wstring s2w(std::string s);
}