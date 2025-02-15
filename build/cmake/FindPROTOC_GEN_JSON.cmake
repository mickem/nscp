if(WIN32)
  set(EXE_NAME protoc-gen-json.exe)
else(WIN32)
  set(EXE_NAME protoc-gen-json)
endif(WIN32)
find_program(
  PROTOC_GEN_JSON_BIN
  NAMES ${EXE_NAME}
  PATHS ${PROTOC_GEN_JSON} /usr/local/bin/ /usr/bin/
        ${Python3_ROOT_DIR}/Scripts)
if(PROTOC_GEN_JSON_BIN)
  set(PROTOC_GEN_JSON_FOUND TRUE)
else()
  set(PROTOC_GEN_JSON_FOUND FALSE)
endif()
