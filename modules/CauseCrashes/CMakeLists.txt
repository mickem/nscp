cmake_minimum_required(VERSION 3.10)

set(TARGET CauseCrashes)

project(${TARGET})

create_module(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(SRCS ${SRCS} "${TARGET}.cpp" ${NSCP_DEF_PLUGIN_CPP})

add_definitions(${NSCP_GLOBAL_DEFINES})

if(WIN32)
    set(SRCS ${SRCS} "${TARGET}.h" ${NSCP_DEF_PLUGIN_HPP})
endif(WIN32)

add_library(${TARGET} MODULE ${SRCS})
nscp_debug_symbols(${TARGET})

target_link_libraries(${TARGET} ${NSCP_DEF_PLUGIN_LIB})

source_group("Common Files" REGULAR_EXPRESSION .*include/.*)
source_group("Parser" REGULAR_EXPRESSION .*include/parser/.*)
source_group("NSCP API" REGULAR_EXPRESSION .*include/nscapi/.*)
source_group("Filter" REGULAR_EXPRESSION .*include/parsers/.*)
source_group("Socket" REGULAR_EXPRESSION .*include/socket/.*)

set_target_properties(${TARGET} PROPERTIES FOLDER ${MODULE_SUBFOLDER})
nscp_install_module(${TARGET})

if(WIN32)
    set(_FOLDER "${MODULE_SUBFOLDER}")
elseif(APPLE)
    set(_FOLDER ${MODULE_SUBFOLDER})
else()
    set(_FOLDER ${MODULE_TARGET_FOLDER})
endif()
if(MSVC11 OR MSVC12 OR MSVC13 OR MSVC14 OR APPLE)
    set_target_properties(
        ${TARGET}
        PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${BUILD_TARGET_ROOT_PATH}/${_FOLDER}
    )
    set_target_properties(
        ${TARGET}
        PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY_DEBUG ${BUILD_TARGET_ROOT_PATH}/${_FOLDER}
    )
    set_target_properties(
        ${TARGET}
        PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY_RELEASE
                ${BUILD_TARGET_ROOT_PATH}/${_FOLDER}
    )
    set_target_properties(
        ${TARGET}
        PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO
                ${BUILD_TARGET_ROOT_PATH}/${_FOLDER}
    )
endif()
