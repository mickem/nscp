#pragma once

#include <map>

extern "C" {
#include <lua.h>
#include "lauxlib.h"
#include "lualib.h"
}
#include "luna.h"

#include <scripts/functions.hpp>

namespace script_wrapper {

	class Lua_State {
		lua_State *L;
	public:
		Lua_State() : L(lua_open()) { }

		~Lua_State() {
			lua_close(L);
		}

		// implicitly act as a lua_State pointer
		inline operator lua_State*() {
			return L;
		}
	};

	class LUAException {
		std::wstring error_;
	public:
		LUAException(std::wstring error) : error_(error) {}

		std::wstring getMessage() const {
			return error_;
		}

	};

	inline std::string w2s(std::wstring s) {
		return strEx::wstring_to_string(s);
	}
	inline std::wstring s2w(std::string s) {
		return strEx::string_to_wstring(s);
	}
	typedef std::pair<std::wstring,int> where_type;
	where_type where(lua_State *L, int level = 1) {
		lua_Debug ar;
		if (lua_getstack(L, level, &ar)) {  /* check function at level */
			lua_getinfo(L, "Sl", &ar);  /* get info about it */
			if (ar.currentline > 0) {  /* is there info? */
				return where_type(s2w(ar.short_src), ar.currentline);
			}
		}
		return where_type(_T("unknown"),0);
	}
	std::wstring extract_string(lua_State *L) {
		int top = lua_gettop(L);
		if (lua_isstring(L, top))
			return strEx::string_to_wstring(lua_tostring( L, lua_gettop( L ) ));
		return _T("<NOT_A_STRING>");
	}
	std::wstring pop_string(lua_State *L) {
		std::wstring ret;
		int top = lua_gettop(L);
		if (lua_isstring(L, top))
			ret = strEx::string_to_wstring(lua_tostring( L, top));
		else if (lua_isnil(L, top))
			ret = _T("<NIL>");
		else if (lua_istable(L, top))
			ret = _T("<TABLE>");
		else if (lua_isnumber(L, top))
			ret = _T("<NUMBER>");
		else if (lua_iscfunction(L, top))
			ret = _T("<C-FUNCTION>");
		else
			ret = _T("<UNKNOWN>");
		lua_pop(L, 1);
		return ret;
	}
	NSCAPI::nagiosReturn extract_code(lua_State *L) {
		std::string str;
		switch (lua_type( L, lua_gettop( L ) )) {
	case LUA_TNUMBER: 
		return static_cast<int>(lua_tonumber(L, lua_gettop(L)));
	case LUA_TTABLE:
		NSC_LOG_ERROR_STD(_T("Incorect return from script: should be error, ok, warning or unknown"));
		return NSCAPI::returnUNKNOWN;
	case LUA_TSTRING:
		str = lua_tostring(L, lua_gettop(L));
		if ((str == "critical")||(str == "crit")||(str == "error")) {
			return NSCAPI::returnCRIT;
		} else if ((str == "warning")||(str == "warn")) {
			return NSCAPI::returnWARN;
		} else if (str == "ok") {
			return NSCAPI::returnOK;
		} else if (str == "unknown") {
			return NSCAPI::returnUNKNOWN;
		} else {
			NSC_LOG_ERROR_STD(_T("Incorect return from script: should be ok, warning, critical or unknown not: ") + strEx::string_to_wstring(str) );
			return NSCAPI::returnUNKNOWN;
		}
	case LUA_TBOOLEAN:
		return lua_toboolean( L, lua_gettop( L ) )?NSCAPI::returnOK:NSCAPI::returnCRIT;
		}
		NSC_LOG_ERROR_STD(_T("Incorect return from script: should be error, ok, warning or unknown"));
		return NSCAPI::returnUNKNOWN;
	}
	void push_code(lua_State *L, NSCAPI::nagiosReturn  code) {
		if (code == NSCAPI::returnOK)
			lua_pushstring(L, strEx::wstring_to_string(_T("ok")).c_str());
		else if (code == NSCAPI::returnWARN)
			lua_pushstring(L, strEx::wstring_to_string(_T("warning")).c_str());
		else if (code == NSCAPI::returnCRIT)
			lua_pushstring(L, strEx::wstring_to_string(_T("critical")).c_str());
		else
		lua_pushstring(L, strEx::wstring_to_string(_T("unknown")).c_str());
	}
	void push_string(lua_State *L, std::wstring s) {
		lua_pushstring(L, strEx::wstring_to_string(s).c_str());
	}
	void push_array(lua_State *L, std::list<std::wstring> &arr) {
		lua_createtable(L, 0, arr.size());
		int i=0;
		for (std::list<std::wstring>::const_iterator cit=arr.begin(); cit != arr.end(); ++cit) {
			lua_pushnumber(L,i++);
			lua_pushstring(L,strEx::wstring_to_string(*cit).c_str());
			lua_settable(L,-3);
		}
	}

	class lua_script;
	class lua_handler {
	public:
		virtual void register_command(lua_script* script, std::wstring command, std::wstring function) = 0;

	};
	class lua_manager {
		typedef std::map<double,lua_handler*> handler_type;
		typedef std::map<double,lua_script*> script_type;
		static handler_type handlers;
		static script_type scripts;
		static double last_value;
		static char handler_key[];
		static char script_key[];
	public:
		static lua_handler* get_handler(lua_State *L) {
			handler_type::const_iterator cit = handlers.find(get_id(L, handler_key));
			if (cit == handlers.end())
				throw LUAException(_T("Could not find handler reference"));
			return (*cit).second;
		}
		static void set_handler(lua_State *L, lua_handler* handler) {
			double id = get_id(L, handler_key);
			handlers[id] = handler;
		}
		static lua_script* get_script(lua_State *L) {
			script_type::const_iterator cit = scripts.find(get_id(L, script_key));
			if (cit == scripts.end())
				throw LUAException(_T("Could not find script reference"));
			return (*cit).second;
		}
		static void set_script(lua_State *L, lua_script* script) {
			double id = get_id(L, script_key);
			scripts[id] = script;
		}
		static double get_id(lua_State *L, char *key) {
			/* retrieve a number */
			lua_pushstring(L, key);
			//lua_pushlightuserdata(L, (void*)&key);  /* push address */
			lua_gettable(L, LUA_REGISTRYINDEX);  /* retrieve value */
			double v = 0;
			v = lua_tonumber(L, -1);  /* convert to number */
			lua_pop(L,1);
			if (v <= 0) {
				v = ++last_value;
				lua_pushstring(L, key);
				//lua_pushlightuserdata(L, reinterpret_cast<void*>(&key));  /* push address */
				lua_pushnumber(L, v);  /* push value */
				/* registry[&Key] = myNumber */
				lua_settable(L, LUA_REGISTRYINDEX);
			}
			return v;

		}

	};
	class nsclient_wrapper {
	public:

		static int execute (lua_State *L) {
			try {
				int nargs = lua_gettop( L );
				if (nargs == 0) {
					return luaL_error(L, "nscp.execute requires at least 1 argument!");
				}
				unsigned int argLen = nargs-1;
				std::list<std::wstring> arguments;
				for (unsigned int i=argLen;i>0;i--) {
					arguments.push_front(extract_string(L));
					lua_pop(L, 1);
				}
				std::wstring command = extract_string(L);
				lua_pop(L, 1);
				std::wstring message;
				std::wstring perf;
				NSCAPI::nagiosReturn ret = GET_CORE()->simple_query(command, arguments, message, perf);
				push_code(L, ret);
				lua_pushstring(L, strEx::wstring_to_string(message).c_str());
				lua_pushstring(L, strEx::wstring_to_string(perf).c_str());
				return 3;
			} catch (...) {
				return luaL_error(L, "Unknown exception in: nscp.execute");
			}
		}

		static int register_command(lua_State *L) {
			try {
				lua_handler *handler = lua_manager::get_handler(L);
				lua_script *script = lua_manager::get_script(L);
				int nargs = lua_gettop( L );
				if (nargs != 2)
					return luaL_error(L, "Incorrect syntax: nscp.register(<key>, <function>);");
				handler->register_command(script, pop_string(L), pop_string(L));
				return 0;
			} catch (LUAException e) {
				return luaL_error(L, std::string("Error in nscp.register: " + w2s(e.getMessage())).c_str());
			} catch (...) {
				return luaL_error(L, "Unknown exception in: nscp.register");
			}
		}

		static int getSetting (lua_State *L) {
			int nargs = lua_gettop( L );
			if (nargs < 2 || nargs > 3)
				return luaL_error(L, "Incorrect syntax: nscp.getSetting(<section>, <key>[, <default value>]);");
			std::wstring v;
			if (nargs > 2)
				v = pop_string(L);
			std::wstring k = pop_string(L);
			std::wstring s = pop_string(L);
			push_string(L, GET_CORE()->getSettingsString(s, k, v));
			return 1;
		}
		static int getSection (lua_State *L) {
			int nargs = lua_gettop( L );
			if (nargs > 1)
				return luaL_error(L, "Incorrect syntax: nscp.getSection([<section>]);");
			std::wstring v;
			if (nargs > 0)
				v = pop_string(L);
			try {
				std::list<std::wstring> list = GET_CORE()->getSettingsSection(v);
				push_array(L, list);
			} catch (...) {
				return luaL_error(L, "Unknown exception getting section");
			}
			return 1;
		}
		static int info (lua_State *L) {
			return log_any(L, NSCAPI::log_level::info);
		}
		static int error (lua_State *L) {
			return log_any(L, NSCAPI::log_level::error);
		}
		static int log_any(lua_State *L, int mode) {
			where_type w = where(L);
			int nargs = lua_gettop( L );
			std::wstring str;
			for (int i=0;i<nargs;i++) {
				str += pop_string(L);
			}
			GET_CORE()->log(mode, utf8::cvt<std::string>(w.first), w.second, str);
			return 0;
		}

		static const luaL_Reg my_funcs[];

		static int luaopen(lua_State *L) {
			luaL_register(L, "nscp", my_funcs);
			return 1;
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

	lua_manager::handler_type lua_manager::handlers;
	lua_manager::script_type lua_manager::scripts;
	double lua_manager::last_value = 0;
	char lua_manager::handler_key[] = "registry.key.handler";
	char lua_manager::script_key[] = "registry.key.script";

	class lua_script {
		Lua_State L;
		std::string script_;
		std::string alias_;
	public:
		lua_script(const script_container &script) : script_(utf8::cvt<std::string>(script.script.string())), alias_(utf8::cvt<std::string>(script.alias)) {
			load();
		}
		void load() {
			luaL_openlibs(L);
			nsclient_wrapper::luaopen(L);
			if (luaL_loadfile(L, script_.c_str()) != 0) {
				throw LUAException(_T("Failed to load script: ") + get_wscript() + _T(": ") + s2w(lua_tostring(L, -1)));
			}

		}
		std::wstring get_wscript() const {
			return utf8::cvt<std::wstring>(script_);
		}
		std::string get_script() const {
			return script_;
		}
		void unload() {}
		void reload(lua_handler *handler) {
			unload();
			load();
			pre_load(handler);
		}
		void pre_load(lua_handler *handler) {
			lua_manager::set_handler(L, handler);
			lua_manager::set_script(L, this);
			if (lua_pcall(L, 0, 0, 0) != 0) {
				throw LUAException(_T("Failed to parse script: ") + get_wscript() + _T(": ") + s2w(lua_tostring(L, -1)));
			}
		}


		NSCAPI::nagiosReturn extract_return(Lua_State &L, int arg_count,  std::wstring &message, std::wstring &perf) {
			// code, message, performance data
			if (arg_count > 3) {
				NSC_LOG_ERROR_STD(_T("Too many arguments return from script (only using last 3)"));
				lua_pop(L, arg_count-3);
			}
			if (arg_count > 2) {
				perf = extract_string(L);
				lua_pop( L, 1 );
			}
			if (arg_count > 1) {
				message = extract_string(L);
				lua_pop( L, 1 );
			}
			if (arg_count > 0) {
				int ret = extract_code(L);
				lua_pop( L, 1 );
				return ret;
			}
			NSC_LOG_ERROR_STD(_T("No arguments returned from script."));
			return NSCAPI::returnUNKNOWN;
		}

		NSCAPI::nagiosReturn handleCommand(lua_handler *handler, std::wstring function, std::wstring cmd, std::list<std::wstring> arguments, std::wstring &msg, std::wstring &perf) {
			lua_manager::set_handler(L, handler);
			lua_manager::set_script(L, this);
			int nargs = lua_gettop( L );
			lua_getglobal(L, w2s(function).c_str());
			if (!lua_isfunction(L, -1)) {
				lua_pop(L, 1); // remove function from LUA stack
				throw LUAException(_T("Failed to run script: ") + get_wscript() + _T(": Function not found: handle"));
			}
			lua_pushstring(L, w2s(cmd).c_str()); 

			lua_createtable(L, 0, arguments.size());
			int i=0;
			BOOST_FOREACH(std::wstring arg, arguments) {
				lua_pushnumber(L,i++);
				lua_pushstring(L,strEx::wstring_to_string(arg).c_str());
				lua_settable(L,-3);
			}

			if (lua_pcall(L, 2, LUA_MULTRET, 0) != 0) {
				std::wstring err = strEx::string_to_wstring(lua_tostring(L, -1));
				NSC_LOG_ERROR_STD(_T("Failed to call main function in script: ") + get_wscript() + _T(": ") + err);
				lua_pop(L, 1); // remove error message
				return NSCAPI::returnUNKNOWN;
			}
			return extract_return(L, lua_gettop( L )-nargs, msg, perf);
		}
	};





}