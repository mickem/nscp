/**************************************************************************
*   Copyright (C) 2004-2007 by Michael Medin <michael@medin.name>         *
*                                                                         *
*   This code is part of NSClient++ - http://trac.nakednuns.org/nscp      *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/
#include "stdafx.h"
#include "LUAScript.h"
#include <strEx.h>
#include <time.h>
#include <filter_framework.hpp>
#include <error.hpp>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

}
#include "luna.h"

LUAScript gLUAScript;

BOOL APIENTRY DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	NSCModuleWrapper::wrapDllMain(hModule, ul_reason_for_call);
	return TRUE;
}

LUAScript::LUAScript() {
}
LUAScript::~LUAScript() {
}


bool LUAScript::loadModule() {
	return true;
}
bool LUAScript::unloadModule() {
	return true;
}

bool LUAScript::hasCommandHandler() {
	return true;
}
bool LUAScript::hasMessageHandler() {
	return false;
}

class Lua_State
{
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

NSCAPI::nagiosReturn LUAScript::RunLUA(const unsigned int argLen, TCHAR **char_args, std::wstring &msg, std::wstring &perf) {
	Lua_State L;
	std::wstring script = _T("scripts\\test.lua");
	luaL_openlibs(L);

	//Luna<Account>::Register(L);

	if (luaL_loadfile(L, strEx::wstring_to_string(script).c_str()) != 0) {
		std::wstring err = strEx::string_to_wstring(lua_tostring(L, -1));
		NSC_LOG_ERROR_STD(_T("Failed to load script: ") + script + _T(": ") + err);
		return NSCAPI::returnUNKNOWN;
	}

	if (lua_pcall(L, 0, 0, 0) != 0) {
		std::wstring err = strEx::string_to_wstring(lua_tostring(L, -1));
		NSC_LOG_ERROR_STD(_T("Failed to parse script: ") + script + _T(": ") + err);
		return NSCAPI::returnUNKNOWN;
	}

	int nargs = lua_gettop( L );
	lua_getglobal(L, "main");
	if (!lua_isfunction(L, -1)) {
		lua_pop(L, 1); // remove function from LUA stack
		NSC_LOG_ERROR_STD(_T("main was not found in script: ") + script);
		return NSCAPI::returnUNKNOWN;
	}
	lua_pushstring(L, "test"); 

	if (lua_pcall(L, 1, LUA_MULTRET, 0) != 0) {
		std::wstring err = strEx::string_to_wstring(lua_tostring(L, -1));
		NSC_LOG_ERROR_STD(_T("Failed to call main function in script: ") + script + _T(": ") + err);
		lua_pop(L, 1); // remove error message
		return NSCAPI::returnUNKNOWN;
	}
	//extract_args(L, lua_gettop( L )-nargs)

	std::cout << "[C++] These values were returned from the script" << std::endl;
	while (lua_gettop( L ))
	{
		switch (lua_type( L, lua_gettop( L ) ))
		{
		case LUA_TNUMBER: std::cout << lua_gettop( L ) << " script returned " << lua_tonumber( L, lua_gettop( L ) ) << std::endl; break;
		case LUA_TTABLE:  std::cout << lua_gettop( L ) << " script returned a table" << std::endl; break;
		case LUA_TSTRING: std::cout << lua_gettop( L ) << " script returned " << lua_tostring( L, lua_gettop( L ) ) << std::endl; break;
		case LUA_TBOOLEAN:std::cout << lua_gettop( L ) << " script returned " << lua_toboolean( L, lua_gettop( L ) ) << std::endl; break;
		default: std::cout << "script returned unknown param" << std::endl; break;
		}
		lua_pop( L, 1 );
	}
	NSC_LOG_ERROR_STD(_T("We got:") + strEx::itos(lua_gettop(L)));


/*
	std::wstring ret1 = strEx::string_to_wstring(lua_tostring(L, -3));
	NSC_LOG_ERROR_STD(_T("Return values are:") + ret1);
	std::wstring ret2 = strEx::string_to_wstring(lua_tostring(L, -2));
	NSC_LOG_ERROR_STD(_T("Return values are:") + ret2);
	std::wstring ret3 = strEx::string_to_wstring(lua_tostring(L, -1));
	NSC_LOG_ERROR_STD(_T("Return values are:") + ret3);
*/
	//lua_setgcthreshold(L, 0);  // collected garbage
	std::wcout << _T("humm 001") << std::endl;
	return 0;
}

NSCAPI::nagiosReturn LUAScript::handleCommand(const strEx::blindstr command, const unsigned int argLen, TCHAR **char_args, std::wstring &msg, std::wstring &perf) {
	if (command == _T("RunLUA")) {
		return RunLUA(argLen, char_args, msg, perf);
	}	
	return NSCAPI::returnIgnored;
}


NSC_WRAPPERS_MAIN_DEF(gLUAScript);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gLUAScript);
