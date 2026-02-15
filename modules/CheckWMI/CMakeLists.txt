cmake_minimum_required(VERSION 3.10)

set(TARGET CheckWMI)

project(${TARGET})

CREATE_MODULE(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(SRCS
    ${SRCS}
    "${TARGET}.cpp"
    ${NSCP_INCLUDEDIR}/wmi/wmi_query.cpp
    ${NSCP_DEF_PLUGIN_CPP}
    ${NSCP_FILTER_CPP}
    ${NSCP_ERROR_CPP}
)

add_definitions(${NSCP_GLOBAL_DEFINES})

if(WIN32)
    set(SRCS
        ${SRCS}
        "${TARGET}.h"
        ${NSCP_INCLUDEDIR}/wmi/wmi_query.hpp
        ${NSCP_DEF_PLUGIN_HPP}
        ${NSCP_FILTER_HPP}
        ${NSCP_ERROR_HPP}
    )
endif(WIN32)

add_library(${TARGET} MODULE ${SRCS})
NSCP_DEBUG_SYMBOLS(${TARGET})

target_link_libraries(
    ${TARGET}
    ${NSCP_DEF_PLUGIN_LIB}
    ${Boost_REGEX_LIBRARY}
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    ${NSCP_FILTER_LIB}
    expression_parser
    Wbemuuid.lib
)

include(${BUILD_CMAKE_FOLDER}/module.cmake)
