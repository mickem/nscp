cmake_minimum_required(VERSION 3.10)

set(TARGET perfconfig_parser)
add_definitions(${NSCP_GLOBAL_DEFINES})

set(SRCS ${NSCP_INCLUDEDIR}/parsers/perfconfig/perfconfig.cpp)
if(WIN32)
    set(SRCS
        ${SRCS}
        ${NSCP_INCLUDEDIR}/parsers/perfconfig/perfconfig.hpp
    )
endif(WIN32)

add_library(${TARGET} STATIC ${SRCS})
NSCP_DEBUG_SYMBOLS(${TARGET})

set_target_properties(
    ${TARGET}
    PROPERTIES
        FOLDER
            "libraries"
)
if(CMAKE_COMPILER_IS_GNUCXX)
    if("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64" AND NOT APPLE)
        set_target_properties(
            ${TARGET}
            PROPERTIES
                COMPILE_FLAGS
                    -fPIC
        )
    endif("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64" AND NOT APPLE)
endif(CMAKE_COMPILER_IS_GNUCXX)
