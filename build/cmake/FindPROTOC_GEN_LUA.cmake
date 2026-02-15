if(WIN32)
    set(EXE_NAME protoc-gen-lua.exe)
else(WIN32)
    set(EXE_NAME protoc-gen-lua)
endif(WIN32)
find_program(
    PROTOC_GEN_LUA_BIN
    NAMES
        ${EXE_NAME}
    PATHS
        ${CMAKE_SOURCE_DIR}/ext/lua-protobuf
        ${PROTOC_GEN_LUA}
        /usr/local/bin/
        /usr/bin/
        ${Python3_ROOT_DIR}/Scripts
)
if(PROTOC_GEN_LUA_BIN)
    set(PROTOC_GEN_LUA_FOUND FALSE)
else(PROTOC_GEN_LUA_BIN)
    set(PROTOC_GEN_LUA_FOUND FALSE)
endif(PROTOC_GEN_LUA_BIN)
