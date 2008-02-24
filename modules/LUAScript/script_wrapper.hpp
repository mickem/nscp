#pragma once

#include <map>

extern "C" {
#include "LUA/lua.h"
#include "LUA/lauxlib.h"
#include "LUA/lualib.h"
}
#include "luna.h"


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




	class Account {
		lua_Number m_balance;
	public:
		static const char className[];
		static Luna<Account>::RegType methods[];

		Account(lua_State *L)      { m_balance = luaL_checknumber(L, 1); }
		int inject(lua_State *L) {
			m_balance += luaL_checknumber(L, 1); return 0; 
		}
		int withdraw(lua_State *L) { m_balance -= luaL_checknumber(L, 1); return 0; }
		int balance (lua_State *L) { lua_pushnumber(L, m_balance); return 1; }
		~Account() { printf("deleted Account (%p)\n", this); }
	};

	const char Account::className[] = "Account";

	#define method(class, name) {#name, &class::name}

	Luna<Account>::RegType Account::methods[] = {
		method(Account, inject),
		method(Account, withdraw),
		method(Account, balance),
		{0,0}
	};
	std::wstring extract_string(lua_State *L) {
		return strEx::string_to_wstring(lua_tostring( L, lua_gettop( L ) ));
	}
	std::wstring pop_string(lua_State *L) {
		std::wstring ret = strEx::string_to_wstring(lua_tostring( L, lua_gettop( L ) ));
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

	static int inject(lua_State *L) {
		int nargs = lua_gettop( L );
		unsigned int argLen = nargs-1;
		arrayBuffer::arrayBuffer arguments = arrayBuffer::createArrayBuffer(argLen);
		for (unsigned int i=argLen;i>0;i--) {
			std::wstring arg = extract_string(L);
			arrayBuffer::set(arguments, argLen, i-1, arg);
			lua_pop(L, 1);
		}
		std::wstring command = extract_string(L);
		lua_pop(L, 1);

		std::wstring msg;
		std::wstring perf;
		NSCAPI::nagiosReturn ret = NSCModuleHelper::InjectCommand(command.c_str(), argLen, arguments, msg, perf);
		push_code(L, ret);
		lua_pushstring(L, strEx::wstring_to_string(msg).c_str());
		lua_pushstring(L, strEx::wstring_to_string(perf).c_str());
		return 3;
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
	lua_manager::handler_type lua_manager::handlers;
	lua_manager::script_type lua_manager::scripts;
	double lua_manager::last_value = 0;
	char lua_manager::handler_key[] = "registry.key.handler";
	char lua_manager::script_key[] = "registry.key.sctrip";

	static int register_command(lua_State *L) {
		try {
			lua_handler *handler = lua_manager::get_handler(L);
			lua_script *script = lua_manager::get_script(L);
			int nargs = lua_gettop( L );
			if (nargs < 2) {
				return luaL_error(L, "Missing argument for register_command! usage: register_command(<key>, <function>);");
			}
			if (nargs > 2) {
				return luaL_error(L, "To many arguments for register_command! usage: register_command(<key>, <function>);");
			}
			handler->register_command(script, pop_string(L), pop_string(L));
			return 0;
		} catch (LUAException e) {
			return luaL_error(L, std::string("Error: " + w2s(e.getMessage())).c_str());
		} catch (...) {
			return luaL_error(L, "Unknown exception in: register_command");
		}
	}
	class lua_script {
		Lua_State L;
		std::wstring script_;
	public:
		lua_script(const std::wstring file) : script_(file) {
			load();
		}
		void load() {
			luaL_openlibs(L);
			//Luna<Account>::Register(L);
			lua_register(L, "inject", inject);
			lua_register(L, "register_command", register_command);

			if (luaL_loadfile(L, strEx::wstring_to_string(script_).c_str()) != 0) {
				throw LUAException(_T("Failed to load script: ") + script_ + _T(": ") + s2w(lua_tostring(L, -1)));
			}

		}
		std::wstring get_script() const {
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
				throw LUAException(_T("Failed to parse script: ") + script_ + _T(": ") + s2w(lua_tostring(L, -1)));
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

		NSCAPI::nagiosReturn handleCommand(lua_handler *handler, std::wstring function, strEx::blindstr command, const unsigned int argLen, TCHAR **char_args, std::wstring &msg, std::wstring &perf) {
			lua_manager::set_handler(L, handler);
			lua_manager::set_script(L, this);
			int nargs = lua_gettop( L );
			lua_getglobal(L, w2s(function).c_str());
			if (!lua_isfunction(L, -1)) {
				lua_pop(L, 1); // remove function from LUA stack
				throw LUAException(_T("Failed to run script: ") + script_ + _T(": Function not found: handle"));
			}
			std::wstring cmd = command.c_str();
			lua_pushstring(L, w2s(cmd).c_str()); 

			if (lua_pcall(L, 1, LUA_MULTRET, 0) != 0) {
				std::wstring err = strEx::string_to_wstring(lua_tostring(L, -1));
				NSC_LOG_ERROR_STD(_T("Failed to call main function in script: ") + script_ + _T(": ") + err);
				lua_pop(L, 1); // remove error message
				return NSCAPI::returnUNKNOWN;
			}
			return extract_return(L, lua_gettop( L )-nargs, msg, perf);
		}
	};





}