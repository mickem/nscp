cmake_minimum_required(VERSION 3.10)

set(TARGET CauseCrashes)

project(${TARGET})

CREATE_MODULE(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(SRCS
    ${SRCS}
    "${TARGET}.cpp"
    ${NSCP_DEF_PLUGIN_CPP}
)

add_definitions(${NSCP_GLOBAL_DEFINES})

if(WIN32)
    set(SRCS
        ${SRCS}
        "${TARGET}.h"
        ${NSCP_DEF_PLUGIN_HPP}
    )
endif(WIN32)

add_library(${TARGET} MODULE ${SRCS})
NSCP_DEBUG_SYMBOLS(${TARGET})

target_link_libraries(${TARGET} ${NSCP_DEF_PLUGIN_LIB})

source_group("Common Files" REGULAR_EXPRESSION .*include/.*)
source_group("Parser" REGULAR_EXPRESSION .*include/parser/.*)
source_group("NSCP API" REGULAR_EXPRESSION .*include/nscapi/.*)
source_group("Filter" REGULAR_EXPRESSION .*include/parsers/.*)
source_group("Socket" REGULAR_EXPRESSION .*include/socket/.*)

set_target_properties(
    ${TARGET}
    PROPERTIES
        FOLDER
            ${MODULE_SUBFOLDER}
)
NSCP_INSTALL_MODULE(${TARGET})

# Where the module lands in the build tree. This mirrors the tail of
# NSCP_INSTALL_MODULE, which this file open-codes because it also installs the
# crash-handler helpers; keep the two in step. BUILD_TARGET_LIB_PATH rather than
# BUILD_TARGET_ROOT_PATH/${_FOLDER}: the unix install destination is absolute,
# and concatenating it onto the build root yields <build>//usr/local/....
if(MSVC11 OR MSVC12 OR MSVC13 OR MSVC14 OR APPLE)
    set(_FOLDER ${BUILD_TARGET_LIB_PATH})
    set_target_properties(
        ${TARGET}
        PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY
                ${_FOLDER}
    )
    set_target_properties(
        ${TARGET}
        PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY_DEBUG
                ${_FOLDER}
    )
    set_target_properties(
        ${TARGET}
        PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY_RELEASE
                ${_FOLDER}
    )
    set_target_properties(
        ${TARGET}
        PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO
                ${_FOLDER}
    )
endif()
