cmake_minimum_required(VERSION 2.6)

IF (HAVE_LUA)

	IF(NOT LUA_LIBS_FOUND)
		FIND_PATH(LUA_SOURCE_DIR lapi.c
			${LUA_INCLUE_DIR}
			${LUA_INCLUE_DIR}/src
			${LUA_DIR}
			${LUA_DIR}/src
		)
	
		SET(lua_SOURCES
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
		IF (MSVC)
			CMAKE_POLICY(SET CMP0008 NEW)
			SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Oi")
		ENDIF (MSVC)

		ADD_LIBRARY(lua SHARED ${lua_SOURCES})
		SET_TARGET_PROPERTIES(lua PROPERTIES FOLDER "libraries")
		ADD_LIBRARY(lua_static STATIC  ${lua_SOURCES})
		SET_TARGET_PROPERTIES(lua_static PROPERTIES FOLDER "libraries")

		IF(APPLE)
		   SET_TARGET_PROPERTIES(lua PROPERTIES COMPILE_FLAGS "-fPIC -w -DOSX  -DNDEBUG -finline-functions -O3 -DCRYPTOPP_DISABLE_ASM")
		   SET_TARGET_PROPERTIES(lua_static PROPERTIES COMPILE_FLAGS "-w -DOSX  -DNDEBUG -finline-functions -O3 -DCRYPTOPP_DISABLE_ASM")
		ELSEIF(UNIX AND NOT APPLE)
		  SET_TARGET_PROPERTIES(lua PROPERTIES COMPILE_FLAGS "-fPIC -w -pipe -DNDEBUG -DLINUX -finline-functions -O3")
		  SET_TARGET_PROPERTIES(lua_static PROPERTIES COMPILE_FLAGS " -w -pipe -DNDEBUG -DLINUX -finline-functions -O3")
		ELSEIF(WIN32)
		  IF(NOT MSVC)
			SET_TARGET_PROPERTIES(lua PROPERTIES COMPILE_FLAGS "-mthreads -w -finline-functions -O3 -DNDEBUG -D__MINGW__ -DWIN32 -D_WIN32_WINNT=0x0501 -D_WIN32")
			SET_TARGET_PROPERTIES(lua_static PROPERTIES COMPILE_FLAGS "-mthreads -w -finline-functions -O3 -DNDEBUG -D__MINGW__ -DWIN32 -D_WIN32_WINNT=0x0501 -D_WIN32")
		  ENDIF(NOT MSVC)
		ENDIF(APPLE)
		SET(LUA_LIB lua_static)
		SET(LUA_LIB lua_static PARENT_SCOPE)
	ENDIF(NOT LUA_LIBS_FOUND)
ENDIF (HAVE_LUA)
