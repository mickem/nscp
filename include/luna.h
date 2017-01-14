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

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}
template < class T > class Luna {
	typedef struct {
		T *pT;
	} userdataType;

public:

	enum {
		NUMBER,
		STRING
	};

	struct PropertyType {
		const char     *name;
		int             (T::*getter) (lua_State *);
		int             (T::*setter) (lua_State *);
	};

	struct FunctionType {
		const char     *name;
		int             (T::*function) (lua_State *);
	};

	static void lua_gettablevalue (lua_State * luaVM, int tableindex, int valueindex)
	{
		lua_pushnumber (luaVM, valueindex);
		lua_gettable (luaVM, tableindex);
		// use lua_to<type>(-1); to get the value.
	}
	/*
	@ check
	Arguments:
	* L - Lua State
	* narg - Position to check

	Description:
	Retrieves a wrapped class from the arguments passed to the function, specified by narg (position).
	This function will raise an exception if the argument is not of the correct type.
	*/
	static T       *check(lua_State * L, int narg) {
		// Check to see whether we are a table
		if (lua_istable(L,narg+1))
		{
			lua_gettablevalue(L,narg+1,0);
			userdataType *ud = static_cast<userdataType*>(luaL_checkudata(L, -1, T::className));
			if (!ud) {
				luaL_typerror(L, narg+1, T::className);
				return NULL;
			}
			lua_pop(L,1);
			return ud->pT;		// pointer to T object
		}
		else
		{
			luaL_typerror(L, narg+1, T::className);
			return NULL;
		} 
	}

	/*
	@ lightcheck
	Arguments:
	* L - Lua State
	* narg - Position to check

	Description:
	Retrieves a wrapped class from the arguments passed to the function, specified by narg (position).
	This function will return NULL if the argument is not of the correct type.  Useful for supporting
	multiple types of arguments passed to the function
	*/ 
	static T* lightcheck(lua_State * L, int narg) {
		// Check to see whether we are a table
		if (lua_istable(L,narg+1))
		{
			lua_gettablevalue(L,narg+1,0);
			userdataType *ud = static_cast <userdataType * >(luaL_testudata(L, -1, T::className));
			if (!ud)
				return NULL; // lightcheck returns NULL if not found.
			lua_pop(L,1);
			return ud->pT;		// pointer to T object
		}
		else
		{
			return NULL;
		} 
	}

	/*
	@ Register
	Arguments:
	* L - Lua State
	* namespac - Namespace to load into

	Description:
	Registers your class with Lua.  Leave namespac "" if you want to load it into the global space.
	*/
	// REGISTER CLASS AS A GLOBAL TABLE 
	static void Register(lua_State * L, std::string namespac) {

		if (namespac != "") {
			lua_getglobal(L, namespac.c_str());
			lua_pushcfunction(L, &Luna < T >::constructor);
			lua_setfield(L, -2, T::className);
			lua_pop(L, 1);
		} else {
			lua_pushcfunction(L, &Luna < T >::constructor);
			lua_setglobal(L, T::className);
		}

		luaL_newmetatable(L, T::className);
		int             metatable = lua_gettop(L);

		lua_pushstring(L, "__gc");
		lua_pushcfunction(L, &Luna < T >::gc_obj);
		lua_settable(L, metatable);

		lua_pushstring(L, "__index");
		lua_pushcfunction(L, &Luna < T >::property_getter);
		lua_settable(L, metatable);

		lua_pushstring(L, "__setindex");
		lua_pushcfunction(L, &Luna < T >::property_setter);
		lua_settable(L, metatable);
		lua_pop(L, 1);

	}

	/*
	@ constructor (internal)
	Arguments:
	* L - Lua State
	*/
	static int constructor(lua_State * L) {
		lua_newtable(L);
		int newtable = lua_gettop(L);
		lua_pushnumber(L, 0);
		T **a = (T **) lua_newuserdata(L, sizeof(T *));
		T *obj = new T(L, true);
		*a = obj;
		int userdata = lua_gettop(L);
		luaL_getmetatable(L, T::className);
		lua_setmetatable(L, userdata);
		lua_settable(L, newtable);
		luaL_getmetatable(L, T::className);
		lua_setmetatable(L, newtable);
		luaL_getmetatable(L, T::className);

		for (int i = 0; T::Properties[i].name; i++) {
			lua_pushstring(L, T::Properties[i].name);
			lua_pushnumber(L, i);
			lua_settable(L, -3);
		}

		lua_pop(L, 1);

		for (int i = 0; T::Functions[i].name; i++) {
			lua_pushstring(L, T::Functions[i].name);
			lua_pushnumber(L, i);
			lua_pushcclosure(L, &Luna < T >::function_dispatch, 1);
			lua_settable(L, newtable);
		}

		return 1;
	}

	/*
	@ createNew
	Arguments:
	* L - Lua State

	Description:
	Loads an instance of the class into the Lua stack, and provides you a pointer so you can modify it.
	*/
	static T       *createNew(lua_State * L) {

		lua_newtable(L);

		int             newtable = lua_gettop(L);

		lua_pushnumber(L, 0);

		T             **a = (T **) lua_newuserdata(L, sizeof(T *));
		T              *obj = new T(L, false);
		obj->isExisting = false;
		*a = obj;

		int             userdata = lua_gettop(L);

		luaL_getmetatable(L, T::className);

		lua_setmetatable(L, userdata);

		lua_settable(L, newtable);

		luaL_getmetatable(L, T::className);
		lua_setmetatable(L, newtable);

		luaL_getmetatable(L, T::className);

		for (int i = 0; T::Properties[i].name; i++) {
			// ADD NAME KEY 
			lua_pushstring(L, T::Properties[i].name);
			lua_pushnumber(L, i);
			lua_settable(L, -3);
		}

		lua_pop(L, 1);

		for (int i = 0; T::Functions[i].name; i++) {
			lua_pushstring(L, T::Functions[i].name);
			lua_pushnumber(L, i);
			lua_pushcclosure(L, &Luna < T >::function_dispatch, 1);
			lua_settable(L, newtable);
		}

		return obj;
	}

	/*
	@ createFromExisting
	Arguments:
	* L - Lua State
	* existingobj - Existing instance of object

	Description:
	Loads an instance of the class into the Lua stack, instead using an existing object rather than creating a new one.
	Returns the existing object.
	*/
	static T       *createFromExisting(lua_State * L, T * existingobj) {

		lua_newtable(L);

		int             newtable = lua_gettop(L);

		lua_pushnumber(L, 0);

		T             **a = (T **) lua_newuserdata(L, sizeof(T *));
		T              *obj = existingobj;
		obj->isExisting = true;
		*a = obj;

		int             userdata = lua_gettop(L);


		luaL_getmetatable(L, T::className);

		lua_setmetatable(L, userdata);

		lua_settable(L, newtable);

		luaL_getmetatable(L, T::className);
		lua_setmetatable(L, newtable);

		luaL_getmetatable(L, T::className);

		for (int i = 0; T::Properties[i].name; i++) {
			lua_pushstring(L, T::Properties[i].name);
			lua_pushnumber(L, i);
			lua_settable(L, -3);
		}

		lua_pop(L, 1);

		for (int i = 0; T::Functions[i].name; i++) {
			lua_pushstring(L, T::Functions[i].name);
			lua_pushnumber(L, i);
			lua_pushcclosure(L, &Luna < T >::function_dispatch, 1);
			lua_settable(L, newtable);
		}

		return obj;
	}

	/*
	@ property_getter (internal)
	Arguments:
	* L - Lua State
	*/
	static int      property_getter(lua_State * L) {

		lua_pushvalue(L, 2);

		lua_getmetatable(L, 1);

		lua_pushvalue(L, 2);
		lua_rawget(L, -2);

		if (lua_isnumber(L, -1)) {

			int             _index = static_cast<int>(lua_tonumber(L, -1));

			lua_pushnumber(L, 0);
			lua_rawget(L, 1);

			T             **obj =
				static_cast < T ** >(lua_touserdata(L, -1));

			lua_pushvalue(L, 3);

			int result = ((*obj)->*(T::Properties[_index].getter)) (L);

			return result;

		}
		// PUSH NIL 
		lua_pushnil(L);

		return 1;

	}

	/*
	@ property_setter (internal)
	Arguments:
	* L - Lua State
	*/
	static int      property_setter(lua_State * L) {

		lua_getmetatable(L, 1);

		lua_pushvalue(L, 2);
		lua_rawget(L, -2);

		if (lua_isnil(L, -1)) {
			lua_pop(L, 2);
			lua_rawset(L, 1);
			return 0;
		} else {
			int _index = lua_tonumber(L, -1);
			lua_pushnumber(L, 0);
			lua_rawget(L, 1);
			T **obj = static_cast<T**>(lua_touserdata(L, -1));

			lua_pushvalue(L, 3);

			//const PropertyType *_properties = (*obj)->T::Properties;

			return ((*obj)->*(T::Properties[_index].setter)) (L);

		}

	}

	/*
	@ function_dispatch (internal)
	Arguments:
	* L - Lua State
	*/
	static int function_dispatch(lua_State * L) {
		if (!lua_istable(L, 1)) {
			return luaL_error(L, "invalid data");
		}
		int i = (int) lua_tonumber(L, lua_upvalueindex(1));

		lua_pushnumber(L, 0);
		lua_rawget(L, 1);

		if (!lua_isuserdata(L, -1)) {
			return luaL_error(L, "invalid data");
		}
		T **obj = static_cast < T ** >(lua_touserdata(L, -1));
		lua_pop(L, 1);

		lua_remove(L, 1);
		return ((*obj)->*(T::Functions[i].function)) (L);
	}

	/*
	@ gc_obj (internal)
	Arguments:
	* L - Lua State
	*/
	static int gc_obj(lua_State * L) {
		T **obj = static_cast < T ** >(luaL_checkudata(L, -1, T::className));
		if (!(*obj)->isExisting && !(*obj)->isPrecious())
		{
			//cout << "Cleaning up a " << T::className << "." << endl;
			delete(*obj);
			*obj = NULL;
		}

		return 0;
	}

};
