# Try to find protocol buffers (protobuf)
#
# And the following command
#
#  WRAP_PROTO(VAR input1 input2 input3..)
#
# Which will run protoc on the input files and set VAR to the names of the created .cc files,
# ready to be added to ADD_EXECUTABLE/ADD_LIBRARY. E.g,
#
#  WRAP_PROTO(PROTO_SRC myproto.proto external.proto)
#  ADD_EXECUTABLE(server ${server_SRC} {PROTO_SRC})
#
# Author: Esben Mose Hansen <[EMAIL PROTECTED]>, (C) Ange Optimization ApS 2008
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
FUNCTION(WRAP_PROTO VAR)
IF (NOT ARGN)
  MESSAGE(SEND_ERROR "Error: WRAP PROTO called without any proto files")
  RETURN()
ENDIF(NOT ARGN)

SET(${VAR}_C)
SET(${VAR}_H)
SET(${VAR}_LUA_C)
SET(${VAR}_LUA_H)
SET(${VAR}_JSON_C)
SET(${VAR}_JSON_H)
FOREACH(FIL ${ARGN})
  GET_FILENAME_COMPONENT(ABS_FIL ${FIL} ABSOLUTE)
  GET_FILENAME_COMPONENT(FIL_WE ${FIL} NAME_WE)
  LIST(APPEND ${VAR}_C "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.pb.cc")
  LIST(APPEND ${VAR}_H "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.pb.h")
  
  #configure_file(${ABS_FIL}.h.in ${ABS_FIL}.h)
  file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/scripts/python/lib)
  file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/libs/lua_pb)
  file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/libs/json_pb)

  SET(ARGS)
  LIST(APPEND ARGS --cpp_out=dllexport_decl=NSCAPI_PROTOBUF_EXPORT:${CMAKE_CURRENT_BINARY_DIR})
  LIST(APPEND ARGS --python_out ${PROJECT_BINARY_DIR}/scripts/python/lib)
  IF(PROTOC_GEN_LUA_FOUND)
	IF(PROTOC_GEN_LUA_BIN)
		SET(PROTOC_GEN_LUA_EXTRA --plugin=protoc-gen-lua=${PROTOC_GEN_LUA_BIN})
	ENDIF(PROTOC_GEN_LUA_BIN)
	LIST(APPEND ARGS --lua_out ${PROJECT_BINARY_DIR}/libs/lua_pb ${PROTOC_GEN_LUA_EXTRA})
	LIST(APPEND ${VAR}_LUA_C "${PROJECT_BINARY_DIR}/libs/lua_pb/${FIL_WE}.pb-lua.cc")
	LIST(APPEND ${VAR}_LUA_C "${PROJECT_BINARY_DIR}/libs/lua_pb/lua-protobuf.cc")
	LIST(APPEND ${VAR}_LUA_H "${PROJECT_BINARY_DIR}/libs/lua_pb/lua-protobuf.h")
	LIST(APPEND ${VAR}_LUA_H "${PROJECT_BINARY_DIR}/libs/lua_pb/${FIL_WE}.pb-lua.h")
  ENDIF(PROTOC_GEN_LUA_FOUND)
  IF(PROTOC_GEN_JSON_FOUND)
	IF(PROTOC_GEN_JSON_BIN)
		SET(PROTOC_GEN_JSON_EXTRA --plugin=protoc-gen-json=${PROTOC_GEN_JSON_BIN})
	ENDIF(PROTOC_GEN_JSON_BIN)
	LIST(APPEND ARGS --json_out ${PROJECT_BINARY_DIR}/libs/json_pb ${PROTOC_GEN_JSON_EXTRA})
	LIST(APPEND ${VAR}_JSON_C "${PROJECT_BINARY_DIR}/libs/json_pb/${FIL_WE}.pb-json.cc")
	LIST(APPEND ${VAR}_JSON_H "${PROJECT_BINARY_DIR}/libs/json_pb/${FIL_WE}.pb-json.h")
  ENDIF(PROTOC_GEN_JSON_FOUND)
  IF(PROTOC_GEN_MD_FOUND AND PROTOBUF_PROTOC_VERSION STREQUAL "2.6.1")
	IF(PROTOC_GEN_MD_BIN)
		SET(PROTOC_GEN_MD_EXTRA --plugin=protoc-gen-md=${PROTOC_GEN_MD_BIN})
	ENDIF(PROTOC_GEN_MD_BIN)
	LIST(APPEND ARGS --md_out ${BUILD_ROOT_FOLDER}/docs/docs/api ${PROTOC_GEN_MD_EXTRA})
	LIST(APPEND ${VAR}_MD "${BUILD_ROOT_FOLDER}/docs/docs/api/${FIL_WE}.md")
  ENDIF()
  
	IF(WIN32)
		SET(ENV{PYTHON} ${PYTHON_EXECUTABLE})
	  ADD_CUSTOM_COMMAND(
		OUTPUT ${${VAR}_C} ${${VAR}_H} ${${VAR}_LUA_C} ${${VAR}_LUA_H} ${${VAR}_JSON_C} ${${VAR}_JSON_H} ${PROJECT_BINARY_DIR}/scripts/python/lib/${FIL_WE}_pb2.py
		COMMAND SET PYTHON=${PYTHON_EXECUTABLE}
		COMMAND ${PROTOBUF_PROTOC_EXECUTABLE}
		ARGS ${ARGS}
			--proto_path ${CMAKE_CURRENT_SOURCE_DIR} ${ABS_FIL}
		DEPENDS ${ABS_FIL}
		COMMENT "Running protocol buffer compiler on ${FIL} (PY)" VERBATIM )
	ELSE(WIN32)
	  ADD_CUSTOM_COMMAND(
		OUTPUT ${${VAR}_C} ${${VAR}_H} ${${VAR}_LUA_C} ${${VAR}_LUA_H} ${${VAR}_JSON_C} ${${VAR}_JSON_H} ${PROJECT_BINARY_DIR}/scripts/python/lib/${FIL_WE}_pb2.py
		COMMAND  ${PROTOBUF_PROTOC_EXECUTABLE}
		ARGS ${ARGS}
			--proto_path ${CMAKE_CURRENT_SOURCE_DIR} ${ABS_FIL}
		DEPENDS ${ABS_FIL}
		COMMENT "Running protocol buffer compiler on ${FIL} - ${PROTOBUF_PROTOC_EXECUTABLE}" VERBATIM )
	ENDIF(WIN32)

  SET_SOURCE_FILES_PROPERTIES(${${VAR}_C} ${${VAR}_H} ${${VAR}_LUA_C} ${${VAR}_LUA_H} ${${VAR}_JSON_C} ${${VAR}_JSON_H} PROPERTIES GENERATED TRUE)
ENDFOREACH(FIL)

SET(${VAR}_C ${${VAR}_C} PARENT_SCOPE)
SET(${VAR}_H ${${VAR}_H} PARENT_SCOPE)
SET(${VAR}_LUA_C ${${VAR}_LUA_C} PARENT_SCOPE)
SET(${VAR}_LUA_H ${${VAR}_LUA_H} PARENT_SCOPE)
SET(${VAR}_JSON_C ${${VAR}_JSON_C} PARENT_SCOPE)
SET(${VAR}_JSON_H ${${VAR}_JSON_H} PARENT_SCOPE)

ENDFUNCTION(WRAP_PROTO)
