cmake_minimum_required(VERSION 2.6)
IF (PROTOC_GEN_LUA_FOUND AND HAVE_LUA)
	SET(LUA_SOURCE_DIR ${LUA_INCLUE_DIR})
	
	SET(lua_pb_SOURCES
		${PROJECT_BINARY_DIR}/libs/lua_pb/lua-protobuf.cc
		${PROJECT_BINARY_DIR}/libs/lua_pb/Plugin.pb-lua.cc
	)
	SET(lua_pb_HEADERS
		${PROJECT_BINARY_DIR}/libs/lua_pb/lua-protobuf.h
		${PROJECT_BINARY_DIR}/libs/lua_pb/Plugin.pb-lua.h
	)
	
	INCLUDE_DIRECTORIES(${LUA_INCLUE_DIR})
	INCLUDE_DIRECTORIES(${PROJECT_BINARY_DIR}/libs/protobuf)
	
	IF (MSVC)
		CMAKE_POLICY(SET CMP0008 NEW)
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Oi")
	ENDIF (MSVC)


	ADD_LIBRARY(lua_pb SHARED ${lua_pb_SOURCES})
	SET_TARGET_PROPERTIES(lua_pb PROPERTIES FOLDER "libraries")
	ADD_LIBRARY(lua_pb_static STATIC ${lua_pb_SOURCES})
	SET_TARGET_PROPERTIES(lua_pb_static PROPERTIES FOLDER "libraries")

	target_link_libraries(lua_pb
		${PROTOBUF_LIBRARY}
		protobuf
		lua_static
	)
	target_link_libraries(lua_pb_static
		${PROTOBUF_LIBRARY}
		protobuf
		lua_static
	)
	
	IF(APPLE)
	   SET_TARGET_PROPERTIES(lua_pb PROPERTIES COMPILE_FLAGS "-fPIC -w -DOSX  -DNDEBUG -finline-functions -O3)
	   SET_TARGET_PROPERTIES(lua_pb_static PROPERTIES COMPILE_FLAGS "-w -DOSX  -DNDEBUG -finline-functions -O3)
	ELSEIF(UNIX AND NOT APPLE)
	  SET_TARGET_PROPERTIES(lua_pb PROPERTIES COMPILE_FLAGS "-fPIC -w -pipe -DNDEBUG -DLINUX -finline-functions -O3")
	  SET_TARGET_PROPERTIES(lua_pb_static PROPERTIES COMPILE_FLAGS " -w -pipe -DNDEBUG -DLINUX -finline-functions -O3")
	ELSEIF(WIN32)
	  IF(NOT MSVC)
		SET_TARGET_PROPERTIES(lua_pb PROPERTIES COMPILE_FLAGS "-mthreads -w -finline-functions -O3 -DNDEBUG -D__MINGW__ -DWIN32 -D_WIN32_WINNT=0x0501 -D_WIN32")
		SET_TARGET_PROPERTIES(lua_pb_static PROPERTIES COMPILE_FLAGS "-mthreads -w -finline-functions -O3 -DNDEBUG -D__MINGW__ -DWIN32 -D_WIN32_WINNT=0x0501 -D_WIN32")
	  ENDIF(NOT MSVC)
	ENDIF(APPLE)
ENDIF (PROTOC_GEN_LUA_FOUND AND HAVE_LUA)
