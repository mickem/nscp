cmake_minimum_required(VERSION 3.10)

set(TARGET LUAScript)

project(${TARGET})

CREATE_MODULE(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(SRCS
    ${SRCS}
    "${TARGET}.cpp"
    extscr_cli.cpp
    script_provider.cpp
    ${NSCP_INCLUDEDIR}/json/use_json.cpp
    ${NSCP_DEF_PLUGIN_CPP}
)

add_definitions(${NSCP_GLOBAL_DEFINES})

if(WIN32)
    set(SRCS
        ${SRCS}
        "${TARGET}.h"
        extscr_cli.h
        script_provider.hpp
        ${NSCP_DEF_PLUGIN_HPP}
    )
endif(WIN32)
include_directories(${LUA_INCLUDE_DIR})
add_library(${TARGET} MODULE ${SRCS})
NSCP_DEBUG_SYMBOLS(${TARGET})

target_link_libraries(
    ${TARGET}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${NSCP_DEF_PLUGIN_LIB}
    ${LUA_LIBRARIES}
    lua_nscp
)

include(${BUILD_CMAKE_FOLDER}/module.cmake)
