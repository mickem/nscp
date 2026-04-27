cmake_minimum_required(VERSION 3.10)

set(TARGET Op5Client)

project(${TARGET})

CREATE_MODULE(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(SRCS
    ${SRCS}
    "${TARGET}.cpp"
    op5_client.cpp
    ${NSCP_DEF_PLUGIN_CPP}
    ${NSCP_CLIENT_CPP}
    ${NSCP_INCLUDEDIR}/json/use_json.cpp
    ${NSCP_INCLUDEDIR}/bytes/base64.c
)

add_definitions(${NSCP_GLOBAL_DEFINES})

if(WIN32)
    set(SRCS
        ${SRCS}
        "${TARGET}.h"
        op5_client.hpp
        ${NSCP_DEF_PLUGIN_HPP}
        ${NSCP_CLIENT_HPP}
    )
endif(WIN32)

add_definitions(${NSCP_GLOBAL_DEFINES})
if(OPENSSL_FOUND)
    include_directories(${OPENSSL_INCLUDE_DIR})
    add_definitions(-DUSE_SSL)
    set(EXTRA_LIBS
        ${EXTRA_LIBS}
        ${OPENSSL_LIBRARIES}
    )
    if(WIN32)
        set(EXTRA_LIBS
            ${EXTRA_LIBS}
            CRYPT32
        )
    endif(WIN32)
endif(OPENSSL_FOUND)

add_library(${TARGET} MODULE ${SRCS})
NSCP_DEBUG_SYMBOLS(${TARGET})

target_link_libraries(
    ${TARGET}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    ${NSCP_DEF_PLUGIN_LIB}
)
OPENSSL_LINK_FIX(${TARGET})
include(${BUILD_CMAKE_FOLDER}/module.cmake)
