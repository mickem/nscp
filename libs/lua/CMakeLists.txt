cmake_minimum_required(VERSION 3.5)

if(LUA_SOURCE_FOUND)

	SET(TARGET lua)

	SET(SRCS
			${LUA_SOURCE_DIR}/lapi.c
			${LUA_SOURCE_DIR}/lauxlib.c
			${LUA_SOURCE_DIR}/lbaselib.c
			${LUA_SOURCE_DIR}/lcode.c
			${LUA_SOURCE_DIR}/ldblib.c
			${LUA_SOURCE_DIR}/ldebug.c
			${LUA_SOURCE_DIR}/ldo.c
			${LUA_SOURCE_DIR}/ldump.c
			${LUA_SOURCE_DIR}/lfunc.c
			${LUA_SOURCE_DIR}/lgc.c
			${LUA_SOURCE_DIR}/linit.c
			${LUA_SOURCE_DIR}/liolib.c
			${LUA_SOURCE_DIR}/llex.c
			${LUA_SOURCE_DIR}/lmathlib.c
			${LUA_SOURCE_DIR}/lmem.c
			${LUA_SOURCE_DIR}/loadlib.c
			${LUA_SOURCE_DIR}/lobject.c
			${LUA_SOURCE_DIR}/lopcodes.c
			${LUA_SOURCE_DIR}/loslib.c
			${LUA_SOURCE_DIR}/lparser.c
			${LUA_SOURCE_DIR}/lstate.c
			${LUA_SOURCE_DIR}/lstring.c
			${LUA_SOURCE_DIR}/lstrlib.c
			${LUA_SOURCE_DIR}/ltable.c
			${LUA_SOURCE_DIR}/ltablib.c
			${LUA_SOURCE_DIR}/ltm.c
			${LUA_SOURCE_DIR}/lundump.c
			${LUA_SOURCE_DIR}/lvm.c
			${LUA_SOURCE_DIR}/lzio.c
	)
	
	FILE (GLOB lua_HEADERS "${LUA_SOURCE_DIR}/*.h")
	if(NOT USE_STATIC_RUNTIME)
		ADD_DEFINITIONS(-DLUA_BUILD_AS_DLL)
		ADD_DEFINITIONS(-DLUA_LIB)
	endif()

	NSCP_MAKE_LIBRARY(${TARGET} "${SRCS}")
	
	SET(LUA_LIB lua PARENT_SCOPE)
endif()
