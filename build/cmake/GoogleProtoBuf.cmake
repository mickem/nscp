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

SET(INCL)
SET(${VAR})
FOREACH(FIL ${ARGN})
  GET_FILENAME_COMPONENT(ABS_FIL ${FIL} ABSOLUTE)
  GET_FILENAME_COMPONENT(FIL_WE ${FIL} NAME_WE)
  LIST(APPEND ${VAR} "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.pb.cc")
  LIST(APPEND INCL "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.pb.h")
  
  SET(PB_TARGET_INCLUDE ${INCL})
  #configure_file(${ABS_FIL}.h.in ${ABS_FIL}.h)
  file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/scripts/python/lib)
  file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/libs/lua_pb)

  SET(ARGS)
  LIST(APPEND ARGS --cpp_out ${CMAKE_CURRENT_BINARY_DIR})
  LIST(APPEND ARGS --python_out ${PROJECT_BINARY_DIR}/scripts/python/lib)
  IF(PROTOC_GEN_LUA_FOUND)
	SET(PROTOC_GEN_LUA_EXTRA --plugin=protoc-gen-lua=${PROTOC_GEN_LUA_CMD})
	LIST(APPEND ARGS --lua_out ${PROJECT_BINARY_DIR}/libs/lua_pb ${PROTOC_GEN_LUA_EXTRA})
  ENDIF(PROTOC_GEN_LUA_FOUND)
  
  ADD_CUSTOM_COMMAND(
	OUTPUT ${${VAR}} ${INCL}  ${PROJECT_BINARY_DIR}/scripts/python/lib/${FIL_WE}_pb2.py
	COMMAND  ${PROTOBUF_PROTOC_EXECUTABLE}
	ARGS ${ARGS}
		--proto_path ${CMAKE_CURRENT_SOURCE_DIR} ${ABS_FIL}
	DEPENDS ${ABS_FIL}
	COMMENT "Running protocol buffer compiler on ${FIL} - ${PROTOBUF_PROTOC_EXECUTABLE}" VERBATIM )

  SET_SOURCE_FILES_PROPERTIES(${${VAR}} ${INCL} PROPERTIES GENERATED TRUE)
ENDFOREACH(FIL)

SET(${VAR} ${${VAR}} PARENT_SCOPE)

ENDFUNCTION(WRAP_PROTO)
