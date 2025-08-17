# Try to find protocol buffers (protobuf)
#
# And the following command
#
# WRAP_PROTO(VAR input1 input2 input3..)
#
# Which will run protoc on the input files and set VAR to the names of the
# created .cc files, ready to be added to ADD_EXECUTABLE/ADD_LIBRARY. E.g,
#
# WRAP_PROTO(PROTO_SRC myproto.proto external.proto) ADD_EXECUTABLE(server
# ${server_SRC} {PROTO_SRC})
#
# Author: Esben Mose Hansen <[EMAIL PROTECTED]>, (C) Ange Optimization ApS 2008
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
function(WRAP_PROTO VAR)
    if(NOT ARGN)
        message(SEND_ERROR "Error: WRAP PROTO called without any proto files")
        return()
    endif(NOT ARGN)

    set(${VAR}_C)
    set(${VAR}_H)
    set(${VAR}_LUA_C)
    set(${VAR}_LUA_H)
    set(${VAR}_JSON_C)
    set(${VAR}_JSON_H)
    set(${VAR}_CS)
    if(PROTOC_GEN_LUA_FOUND)
        list(
            APPEND
            ${VAR}_LUA_C
            "${PROJECT_BINARY_DIR}/libs/lua_pb/lua-protobuf.cc"
        )
        list(
            APPEND
            ${VAR}_LUA_H
            "${PROJECT_BINARY_DIR}/libs/lua_pb/lua-protobuf.h"
        )
    endif(PROTOC_GEN_LUA_FOUND)

    foreach(FIL ${ARGN})
        get_filename_component(ABS_FIL ${FIL} ABSOLUTE)
        get_filename_component(FIL_WE ${FIL} NAME_WE)
        get_filename_component(FIL_NAME ${FIL} NAME)
        string(TOUPPER ${FIL_WE} tmp1)
        string(SUBSTRING ${tmp1} 0 1 tmp2)
        string(SUBSTRING ${FIL_WE} 1 -1 tmp3)
        set(FILE_PKG "${tmp2}${tmp3}")
        list(APPEND ${VAR}_C "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.pb.cc")
        list(APPEND ${VAR}_H "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.pb.h")

        # configure_file(${ABS_FIL}.h.in ${ABS_FIL}.h)
        file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/scripts/python/lib)
        file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/libs/lua_pb)
        file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/libs/json_pb)

        set(ARGS)
        list(
            APPEND
            ARGS
            --cpp_out=dllexport_decl=NSCAPI_PROTOBUF_EXPORT:${CMAKE_CURRENT_BINARY_DIR}
        )
        list(APPEND ARGS --python_out ${PROJECT_BINARY_DIR}/scripts/python/lib)
        if(CSHARP_FOUND AND WIN32)
            list(
                APPEND
                ARGS
                --csharp_out
                ${PROJECT_BINARY_DIR}/libs/protobuf_net
            )
            list(
                APPEND
                ${VAR}_CS
                "${PROJECT_BINARY_DIR}/libs/protobuf_net/${FILE_PKG}.cs"
            )
        endif()
        if(PROTOC_GEN_LUA_FOUND)
            if(PROTOC_GEN_LUA_BIN)
                set(PROTOC_GEN_LUA_EXTRA
                    --plugin=protoc-gen-lua=${PROTOC_GEN_LUA_BIN}
                )
            endif(PROTOC_GEN_LUA_BIN)
            list(
                APPEND
                ARGS
                --lua_out
                ${PROJECT_BINARY_DIR}/libs/lua_pb
                ${PROTOC_GEN_LUA_EXTRA}
            )
            list(
                APPEND
                ${VAR}_LUA_C
                "${PROJECT_BINARY_DIR}/libs/lua_pb/${FIL_WE}.pb-lua.cc"
            )
            # LIST(APPEND ${VAR}_LUA_C
            # "${PROJECT_BINARY_DIR}/libs/lua_pb/lua-protobuf.cc") LIST(APPEND
            # ${VAR}_LUA_H "${PROJECT_BINARY_DIR}/libs/lua_pb/lua-protobuf.h")
            list(
                APPEND
                ${VAR}_LUA_H
                "${PROJECT_BINARY_DIR}/libs/lua_pb/${FIL_WE}.pb-lua.h"
            )
        endif(PROTOC_GEN_LUA_FOUND)
        if(PROTOC_GEN_MD_FOUND AND PROTOBUF_PROTOC_VERSION STREQUAL "2.6.1")
            if(PROTOC_GEN_MD_BIN)
                set(PROTOC_GEN_MD_EXTRA
                    --plugin=protoc-gen-md=${PROTOC_GEN_MD_BIN}
                )
            endif(PROTOC_GEN_MD_BIN)
            list(
                APPEND
                ARGS
                --md_out
                ${BUILD_ROOT_FOLDER}/docs/docs/api
                ${PROTOC_GEN_MD_EXTRA}
            )
            list(
                APPEND
                ${VAR}_MD
                "${BUILD_ROOT_FOLDER}/docs/docs/api/${FIL_WE}.md"
            )
        endif()

        if(WIN32)
            set(ENV{PYTHON} ${Python3_EXECUTABLE})
            add_custom_command(
                OUTPUT
                    ${${VAR}_C}
                    ${${VAR}_H}
                    ${${VAR}_LUA_C}
                    ${${VAR}_LUA_H}
                    ${${VAR}_JSON_C}
                    ${${VAR}_JSON_H}
                    ${${VAR}_CS}
                    ${PROJECT_BINARY_DIR}/scripts/python/lib/${FIL_WE}_pb2.py
                COMMAND SET PYTHON=${Python3_EXECUTABLE}
                COMMAND ${PROTOBUF_PROTOC_EXECUTABLE}
                ARGS ${ARGS} --proto_path ${CMAKE_CURRENT_SOURCE_DIR} ${ABS_FIL}
                DEPENDS ${ABS_FIL}
                COMMENT "Running protocol buffer compiler on ${FIL} (PY)"
                VERBATIM
            )
        else(WIN32)
            add_custom_command(
                OUTPUT
                    ${${VAR}_C}
                    ${${VAR}_H}
                    ${${VAR}_LUA_C}
                    ${${VAR}_LUA_H}
                    ${${VAR}_JSON_C}
                    ${${VAR}_JSON_H}
                    ${PROJECT_BINARY_DIR}/scripts/python/lib/${FIL_WE}_pb2.py
                COMMAND ${PROTOBUF_PROTOC_EXECUTABLE}
                ARGS ${ARGS} --proto_path ${CMAKE_CURRENT_SOURCE_DIR} ${ABS_FIL}
                DEPENDS ${ABS_FIL}
                COMMENT
                    "Running protocol buffer compiler on ${FIL} - ${PROTOBUF_PROTOC_EXECUTABLE}"
                VERBATIM
            )
        endif(WIN32)

        set_source_files_properties(
            ${${VAR}_C}
            ${${VAR}_H}
            ${${VAR}_LUA_C}
            ${${VAR}_LUA_H}
            ${${VAR}_JSON_C}
            ${${VAR}_JSON_H}
            ${${VAR}_CS}
            PROPERTIES GENERATED TRUE
        )
    endforeach(FIL)

    set(${VAR}_C ${${VAR}_C} PARENT_SCOPE)
    set(${VAR}_H ${${VAR}_H} PARENT_SCOPE)
    set(${VAR}_LUA_C ${${VAR}_LUA_C} PARENT_SCOPE)
    set(${VAR}_LUA_H ${${VAR}_LUA_H} PARENT_SCOPE)
    set(${VAR}_JSON_C ${${VAR}_JSON_C} PARENT_SCOPE)
    set(${VAR}_JSON_H ${${VAR}_JSON_H} PARENT_SCOPE)
    set(${VAR}_CS ${${VAR}_CS} PARENT_SCOPE)
endfunction(WRAP_PROTO)
