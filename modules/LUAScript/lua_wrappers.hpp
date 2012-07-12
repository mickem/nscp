#pragma once

extern "C" {
#include <lua.h>
#include "lauxlib.h"
#include "lualib.h"
}
#include "luna.h"

#include <string>
#include <list>

#include <NSCAPI.h>
#include <nscapi/nscapi_core_wrapper.hpp>

#include <strEx.h>

namespace lua_wrappers {
	class Lua_State {
		lua_State *L;
	public:
		Lua_State() : L(lua_open()) { }

		~Lua_State() {
			lua_close(L);
		}

		// implicitly act as a lua_State pointer
		inline operator lua_State*() const {
			return L;
		}
		inline lua_State* get_state() const {
			int i = lua_gettop(L);
			return L;
		}



	};

	class lua_wrapper {

		lua_State *L;
	public:

		lua_wrapper(lua_State *L) : L(L) {}

		//////////////////////////////////////////////////////////////////////////
		/// get_xxx
		std::wstring get_string(int pos = -1) {
			if (pos == -1)
				pos = lua_gettop(L);
			if (pos == 0)
				return _T("<EMPTY>");
			if (is_string(pos))
				return utf8::cvt<std::wstring>(lua_tostring(L, pos));
			if (is_number(pos))
				return strEx::itos(lua_tonumber(L, pos));
			return _T("<NOT_A_STRING>");
		}
		bool get_string(std::wstring &str, int pos = -1) {
			if (pos == -1)
				pos = lua_gettop(L);
			if (pos == 0)
				return false;
			if (is_string(pos))
				str = utf8::cvt<std::wstring>(lua_tostring(L, pos));
			else if (is_number(pos))
				str = strEx::itos(lua_tonumber(L, pos));
			else
				return false;
			return true;
		}
		int get_int(int pos = -1) {
			if (pos == -1)
				pos = lua_gettop(L);
			if (pos == 0)
				return 0;
			if (is_string(pos))
				return strEx::stoi(utf8::cvt<std::wstring>(lua_tostring(L, pos)));
			if (is_number(pos))
				return lua_tonumber(L, pos);
			return 0;
		}
		bool get_boolean(int pos = -1) {
			if (pos == -1)
				pos = lua_gettop(L);
			if (pos == 0)
				return false;
			if (is_boolean(pos))
				return lua_toboolean(L, pos);
			if (is_number(pos))
				return lua_tonumber(L, pos)==1;
			return false;
		}
		NSCAPI::nagiosReturn get_code(int pos = -1);
		std::list<std::wstring> get_array(const int pos = -1) {
			std::list<std::wstring> ret;
			const int len = lua_objlen(L, pos);
			for ( int i = 1; i <= len; ++i ) {
				lua_pushinteger(L, i);
				lua_gettable(L, -2);
				ret.push_back(get_string(-1));
				pop();
			}
			return ret;
		}

		//////////////////////////////////////////////////////////////////////////
		/// pop_xxx
		boolean pop_boolean() {
			int pos = lua_gettop(L);
			if (pos == 0)
				return false;
			NSCAPI::nagiosReturn ret = get_boolean(pos);
			lua_pop(L, 1);
			return ret;
		}
		std::wstring pop_string() {
			std::wstring ret;
			int top = lua_gettop(L);
			if (top == 0)
				return _T("<EMPTY>");
			ret = get_string(top);
			pop();
			return ret;
		}
		bool pop_string(std::wstring &str) {
			int top = lua_gettop(L);
			if (top == 0)
				return false;
			if (!get_string(str, top))
				return false;
			pop();
			return true;
		}
		bool pop_function(int &funref) {
			int top = lua_gettop(L);
			if (top == 0)
				return false;
			if (!is_function(top))
				return false;
			funref = luaL_ref(L, LUA_REGISTRYINDEX);
			if (funref == 0)
				return false;
			return true;
		}
		int pop_int() {
			int ret;
			int top = lua_gettop(L);
			if (top == 0)
				return 0;
			ret = get_int(top);
			pop();
			return ret;
		}
		NSCAPI::nagiosReturn pop_code() {
			int pos = lua_gettop(L);
			if (pos == 0)
				return NSCAPI::returnUNKNOWN;
			NSCAPI::nagiosReturn ret = get_code(pos);
			pop();
			return ret;
		}
		std::list<std::wstring> pop_array() {
			std::list<std::wstring> ret;
			int pos = lua_gettop(L);
			if (pos == 0)
				return ret;
			ret = get_array(pos);
			pop();
			return ret;
		}

		void getglobal(const std::wstring &name) {
			lua_getglobal(L, utf8::cvt<std::string>(name).c_str());
		}


		//////////////////////////////////////////////////////////////////////////
		// Converters
		NSCAPI::nagiosReturn string_to_code(std::string str);


		////////////////////////////////////////////////////////////////////////////
		// Misc
		inline void pop(int count = 1) {
			lua_pop(L, count);
		}
		inline int type(int pos = -1) {
			if (pos == -1)
				pos = lua_gettop(L);
			if (pos == 0)
				return LUA_TNIL;
			int type = lua_type(L, pos);
			return type;
		}
		std::wstring get_type_as_string(int pos = -1) {
			if (pos == -1)
				pos = lua_gettop(L);
			if (pos == 0)
				return _T("<EMPTY>");
			switch (lua_type(L, pos)) {
				case LUA_TNUMBER: 
					return _T("<NUMBER>");
				case LUA_TSTRING:
					return _T("<STRING>");
				case LUA_TBOOLEAN:
					return _T("<TABLE>");
				case LUA_TLIGHTUSERDATA:
					return _T("<LIGHTUSERDATA>");
				case LUA_TTABLE:
					return _T("<TABLE>");
			}
			return _T("<UNKNOWN>");
		}

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
		void push_code(NSCAPI::nagiosReturn code) {
			if (code == NSCAPI::returnOK)
				lua_pushstring(L, strEx::wstring_to_string(_T("ok")).c_str());
			else if (code == NSCAPI::returnWARN)
				lua_pushstring(L, strEx::wstring_to_string(_T("warning")).c_str());
			else if (code == NSCAPI::returnCRIT)
				lua_pushstring(L, strEx::wstring_to_string(_T("critical")).c_str());
			else
				lua_pushstring(L, strEx::wstring_to_string(_T("unknown")).c_str());
		}
		void push_string(std::wstring s) {
			lua_pushstring(L, strEx::wstring_to_string(s).c_str());
		}
		void push_boolean(bool b) {
			lua_pushboolean(L, b?TRUE:FALSE);
		}
		void push_int(int b) {
			lua_pushinteger(L, b);
		}
		void push_string(std::string s) {
			lua_pushstring(L, s.c_str());
		}
		void push_raw_string(std::string s) {
			lua_pushlstring(L, s.c_str(), s.size());
		}
		void push_array(std::list<std::wstring> &arr) {
			lua_createtable(L, 0, arr.size());
			int i=0;
			BOOST_FOREACH(const std::wstring &s, arr) {
				lua_pushnumber(L,i++);
				lua_pushstring(L,strEx::wstring_to_string(s).c_str());
				lua_settable(L,-3);
			}
		}
		inline int size() {
			return lua_gettop(L);
		}
		inline bool empty() {
			return size() == 0;
		}
		void log_stack();

		int error(std::string s) {
			return luaL_error(L, s.c_str());
		}

		typedef std::pair<std::wstring,int> stack_trace;
		stack_trace get_stack_trace(int level = 1) {
			lua_Debug ar;
			if (lua_getstack(L, level, &ar)) {  /* check function at level */
				lua_getinfo(L, "Sl", &ar);  /* get info about it */
				if (ar.currentline > 0) {  /* is there info? */
					return stack_trace(utf8::cvt<std::wstring>(ar.short_src), ar.currentline);
				}
			}
			return stack_trace(_T("unknown"),0);
		}

		std::wstring dump_stack() {
			std::wstring ret;
			while (!empty()) {
				if (!ret.empty())
					ret += _T(", ");
				ret += pop_string();
			}
			return ret;
		}

		inline void openlibs() {
			luaL_openlibs(L);
		}

		inline int loadfile(std::string script) {
			return luaL_loadfile(L, script.c_str());
		}

		int pcall(int nargs, int nresults, int errfunc) {
			return lua_pcall(L, nargs, nresults, errfunc);
		}


		std::string inline op_string(int pos, std::string def = "") {
			return luaL_optstring(L, pos, def.c_str());
		}
		std::wstring inline op_wstring(int pos, std::string def = "") {
			return utf8::cvt<std::wstring>(op_string(pos, def));
		}
		std::wstring inline op_wstring(int pos, std::wstring def) {
			return op_wstring(pos, utf8::cvt<std::string>(def));
		}
		std::string inline string(int pos) {
			return luaL_checkstring(L, pos);
		}
		std::wstring inline wstring(int pos) {
			return utf8::cvt<std::wstring>(string(pos));
		}

		std::list<std::wstring> inline checkarray(int pos) {
			luaL_checktype(L, pos, LUA_TTABLE);
			return get_array(pos);
		}

		boolean inline checkbool(int pos) {
			return lua_toboolean(L, pos);
		}
		int inline op_int(int pos, int def = 0) {
			return luaL_optinteger(L, pos, def);
		}
		int inline checkint(int pos) {
			return luaL_checkint(L, pos);
		}
	};

	class LUAException : std::exception {
		std::string error_;
	public:
		LUAException(std::wstring error) : error_(utf8::cvt<std::string>(error)) {}
		LUAException(std::string error) : error_(error) {}

		~LUAException() throw() {}
		const char* what() const throw() {
			return error_.c_str();
		}
		std::wstring getMessage() const {
			return utf8::cvt<std::wstring>(error_);
		}
	};

	inline std::string w2s(std::wstring s) {
		return utf8::cvt<std::string>(s);
	}
	inline std::wstring s2w(std::string s) {
		return utf8::cvt<std::wstring>(s);
	}

	class lua_registry;
	class lua_script_instance {
		Lua_State L;
		nscapi::core_wrapper* core;
		int plugin_id;
		boost::shared_ptr<lua_registry> registry;
		std::string alias;
		std::string script;

	public:

		lua_script_instance(nscapi::core_wrapper* core, int plugin_id, boost::shared_ptr<lua_registry> registry, std::string alias, std::string script) 
			: core(core)
			, plugin_id(plugin_id)
			, registry(registry)
			, alias(alias)
			, script(script)
		{}

		int get_plugin_id() const {
			return plugin_id;
		}
		lua_State *get_lua_state() const {
			return L.get_state();
		}
		nscapi::core_wrapper* get_core() const {
			return core;
		}
		boost::shared_ptr<lua_registry> get_registry() const {
			return registry;
		}
		std::string get_script() const {
			return script;
		}
		std::string get_alias() const {
			return alias;
		}
	};

	class lua_registry {
		struct function_container {
			boost::shared_ptr<lua_script_instance> instance;
			int func_ref;
			bool simple;
		};

		typedef std::map<std::wstring,function_container> function_map;
		function_map functions;
		function_map channels;
		function_map execs;

		inline lua_State * prep_function(const function_container &c) {
			lua_State *L = c.instance->get_lua_state();
			lua_rawgeti(L, LUA_REGISTRYINDEX, c.func_ref);
			return L;
		}
	public:

		NSCAPI::nagiosReturn on_query(const wchar_t* command, const std::string &request, std::string &response);
		NSCAPI::nagiosReturn on_exec(const std::wstring & command, std::list<std::wstring> & arguments, std::wstring & result);
		NSCAPI::nagiosReturn on_submission(const std::wstring channel, const std::wstring source, const std::wstring command, NSCAPI::nagiosReturn code, std::wstring msg, std::wstring perf);

		void register_query(const std::wstring &command, boost::shared_ptr<lua_script_instance> instance, int func_ref, bool simple = true) {
			function_container c;
			c.func_ref = func_ref;
			c.instance = instance;
			c.simple = simple;
			functions[command] = c;
		}
		void register_subscription(const std::wstring &channel, boost::shared_ptr<lua_script_instance> instance, int func_ref) {
			function_container c;
			c.func_ref = func_ref;
			c.instance = instance;
			channels[channel] = c;
		}

		void register_exec(const std::wstring &command, boost::shared_ptr<lua_script_instance> instance, int func_ref) {
			function_container c;
			c.func_ref = func_ref;
			c.instance = instance;
			execs[command] = c;
		}

		void clear() {
			functions.clear();
			execs.clear();
			channels.clear();
			// DO we need to release reference here?
		}

		bool has_command(const std::wstring & command) {
			return functions.find(command) != functions.end();
		}
		bool has_exec(const std::wstring & command) {
			return execs.find(command) != execs.end();
		}
		bool has_submit(const std::wstring &command) {
			return channels.find(command) != channels.end();
		}
	};

	class lua_instance_manager {
	public:
		typedef boost::shared_ptr<lua_script_instance> script_instance_type;
		typedef std::vector<script_instance_type> script_map_type;
	private:
		static script_map_type scripts;
	public:
		static void set_script(lua_State *L, script_instance_type script) {
			int index = 0;
			{
				// TODO: mutex lock!
				index = scripts.size();
				scripts.push_back(script);
			}
			set_registry(L, get_script_key(), index);
		}
		static script_instance_type get_script(lua_State *L) {
			int index = get_registry(L, get_script_key());
			{
				// TODO: mutex lock!
				if (index >= scripts.size())
					throw lua_wrappers::LUAException(_T("Could not find script reference"));
				return scripts[index];
			}
		}
		static std::string get_script_key() {
			return "nscp.keys.script";
		}
		//////////////////////////////////////////////////////////////////////////
		// Registry access
		static int get_registry(lua_State *L, std::string key) {
			lua_pushstring(L, key.c_str());
			lua_gettable(L, LUA_REGISTRYINDEX);
			return lua_tonumber(L, -1);
		}
		static void set_registry(lua_State *L, std::string key, int val) {
			lua_pushstring(L, key.c_str());
			lua_pushnumber(L, val);
			lua_settable(L, LUA_REGISTRYINDEX);
		}
	};




}

