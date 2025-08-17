if(WIN32)
    set(EXE_NAME protoc-gen-md.exe)
else(WIN32)
    set(EXE_NAME protoc-gen-md)
endif(WIN32)
find_program(
    PROTOC_GEN_MD_BIN
    NAMES ${EXE_NAME}
    PATHS
        ${CMAKE_SOURCE_DIR}/ext/md-protobuf
        ${PROTOC_GEN_MD}
        /usr/local/bin/
        /usr/bin/
        ${Python3_ROOT_DIR}/Scripts
)
if(PROTOC_GEN_MD_BIN)
    set(PROTOC_GEN_MD_FOUND TRUE)
else()
    set(PROTOC_GEN_MD_FOUND FALSE)
endif()
