cmake_minimum_required(VERSION 3.10)

wrap_proto(PROTO_PLUGIN_SRC plugin.proto)
wrap_proto(COMMON_PLUGIN_SRC common.proto)
wrap_proto(METRICS_PLUGIN_SRC metrics.proto)
wrap_proto(LOG_PLUGIN_SRC log.proto)
wrap_proto(REGISTRY_PLUGIN_SRC registry.proto)
wrap_proto(SETTINGS_PLUGIN_SRC settings.proto)
wrap_proto(STORAGE_PLUGIN_SRC storage.proto)

set(TARGET nscp_protobuf)

set(SRCS
    ${COMMON_PLUGIN_SRC_C}
    ${PROTO_PLUGIN_SRC_C}
    ${METRICS_PLUGIN_SRC_C}
    ${LOG_PLUGIN_SRC_C}
    ${REGISTRY_PLUGIN_SRC_C}
    ${SETTINGS_PLUGIN_SRC_C}
    ${STORAGE_PLUGIN_SRC_C})
if(WIN32)
  set(SRCS
      ${SRCS}
      plugin.proto
      metrics.proto
      log.proto
      registry.proto
      settings.proto
      storage.proto
      common.proto
      ${COMMON_PLUGIN_SRC_H}
      ${PROTO_PLUGIN_SRC_H}
      ${METRICS_PLUGIN_SRC_H}
      ${LOG_PLUGIN_SRC_H}
      ${REGISTRY_PLUGIN_SRC_H}
      ${SETTINGS_PLUGIN_SRC_H}
      ${STORAGE_PLUGIN_SRC_H})
endif(WIN32)
nscp_make_library(${TARGET} "${SRCS}")
nscp_force_include(${TARGET}
                   ${BUILD_ROOT_FOLDER}/include/nscapi/dll_defines_protobuf.hpp)

target_link_libraries(${TARGET} ${PROTOBUF_LIBRARY})
if(MSVC)
target_compile_options(${TARGET} PRIVATE /wd4251)
endif(MSVC)

if(PROTOC_GEN_LUA_FOUND AND (LUA_FOUND OR LUA_SOURCE_FOUND))
  set(TARGET nscp_lua_pb)

  set(SRCS
      ${COMMON_PLUGIN_SRC_LUA_C}
      ${PROTO_PLUGIN_SRC_LUA_C}
      ${METRICS_PLUGIN_SRC_LUA_C}
      ${LOG_PLUGIN_SRC_LUA_C}
      ${REGISTRY_PLUGIN_SRC_LUA_C}
      ${SETTINGS_PLUGIN_SRC_LUA_C}
      ${STORAGE_PLUGIN_SRC_LUA_C})
  if(WIN32)
    set(SRCS
        ${SRCS}
        plugin.proto
        metrics.proto
        log.proto
        registry.proto
        settings.proto
        storage.proto
        common.proto
        ${COMMON_PLUGIN_SRC_LUA_H}
        ${PROTO_PLUGIN_SRC_LUA_H}
        ${METRICS_PLUGIN_SRC_LUA_H}
        ${LOG_PLUGIN_SRC_LUA_H}
        ${REGISTRY_PLUGIN_SRC_LUA_H}
        ${SETTINGS_PLUGIN_SRC_LUA_H}
        ${STORAGE_PLUGIN_SRC_LUA_H})
  endif(WIN32)
  include_directories(${LUA_INCLUE_DIR})
  include_directories(${LUA_SOURCE_DIR})
  include_directories(${PROJECT_BINARY_DIR}/libs/protobuf)
  add_definitions(${NSCP_GLOBAL_DEFINES})
  add_definitions(-DLUA_COMPAT_MODULE)

  if(MSVC)
    set(CMAKE_CXX_FLAGS
        "${CMAKE_CXX_FLAGS} /wd4267 /wd4244 /wd4702 /wd4800 /wd4275 /wd4251 /wd4100 /wd4125"
    )
  endif(MSVC)

  if(USE_STATIC_RUNTIME)
    add_definitions(-Dlua_protobuf_NOLIB)
  else(USE_STATIC_RUNTIME)
    add_definitions(-Dlua_protobuf_EXPORTS)
  endif(USE_STATIC_RUNTIME)
  nscp_make_library(${TARGET} "${SRCS}")
  nscp_force_include(
          ${TARGET} ${BUILD_ROOT_FOLDER}/include/nscapi/dll_defines_protobuf.hpp)

  target_link_libraries(${TARGET} ${PROTOBUF_LIBRARY} nscp_protobuf ${LUA_LIB})
if(WIN32)
  target_compile_options(${TARGET} PRIVATE /wd4251)
endif(WIN32)

endif()
