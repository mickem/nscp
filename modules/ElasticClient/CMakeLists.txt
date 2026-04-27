cmake_minimum_required(VERSION 3.10)

set(TARGET ElasticClient)

project(${TARGET})

CREATE_MODULE(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(SRCS
    ${SRCS}
    "${TARGET}.cpp"
    ${NSCP_INCLUDEDIR}/nsclient/logger/logger_helper.cpp
    ${NSCP_DEF_PLUGIN_CPP}
    ${NSCP_CLIENT_CPP}
    ${NSCP_INCLUDEDIR}/json/use_json.cpp
)

add_definitions(${NSCP_GLOBAL_DEFINES})

if(WIN32)
    set(SRCS
        ${SRCS}
        "${TARGET}.h"
        ${NSCP_INCLUDEDIR}/nsclient/logger/logger_helper.hpp
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
    ${Boost_DATE_TIME_LIBRARY}
    ${NSCP_DEF_PLUGIN_LIB}
)
OPENSSL_LINK_FIX(${TARGET})
include(${BUILD_CMAKE_FOLDER}/module.cmake)
