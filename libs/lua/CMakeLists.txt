cmake_minimum_required(VERSION 3.10)

if(LUA_SOURCE_FOUND)

  set(TARGET lua)

  # Lua source files from: https://www.lua.org/manual/5.4/readme.html#other
  set(LIB_FILES
          lapi.c lcode.c lctype.c ldebug.c ldo.c ldump.c lfunc.c lgc.c llex.c lmem.c lobject.c lopcodes.c lparser.c lstate.c lstring.c ltable.c ltm.c lundump.c lvm.c lzio.c lauxlib.c lbaselib.c lcorolib.c ldblib.c liolib.c lmathlib.c loadlib.c loslib.c lstrlib.c ltablib.c lutf8lib.c linit.c )

  foreach (file ${LIB_FILES})
    list(APPEND SRCS ${LUA_SOURCE_DIR}/${file})
  endforeach()

  file(GLOB lua_HEADERS "${LUA_SOURCE_DIR}/*.h")
  if(NOT USE_STATIC_RUNTIME)
    add_definitions(-DLUA_BUILD_AS_DLL)
  endif()

  nscp_make_library(${TARGET} "${SRCS}")

  set(LUA_LIB
      lua
      PARENT_SCOPE)
endif()
