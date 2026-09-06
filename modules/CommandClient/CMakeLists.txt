cmake_minimum_required(VERSION 3.10)

set(TARGET CommandClient)

project(${TARGET})

CREATE_MODULE(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(SRCS
    ${SRCS}
    "${TARGET}.cpp"
    console_editor.cpp
    console_syntax.cpp
    ${NSCP_INCLUDEDIR}/client/simple_client.cpp
    ${NSCP_INCLUDEDIR}/metrics/metrics_store_map.cpp
    ${NSCP_DEF_PLUGIN_CPP}
)

add_definitions(${NSCP_GLOBAL_DEFINES})

if(WIN32)
    set(SRCS
        ${SRCS}
        "${TARGET}.h"
        console_editor.hpp
        console_syntax.hpp
        ${NSCP_INCLUDEDIR}/client/simple_client.hpp
        ${NSCP_INCLUDEDIR}/metrics/metrics_store_map.hpp
        ${NSCP_DEF_PLUGIN_HPP}
    )
endif(WIN32)

add_library(${TARGET} MODULE ${SRCS})
NSCP_DEBUG_SYMBOLS(${TARGET})

target_link_libraries(
    ${TARGET}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${Boost_DATE_TIME_LIBRARY}
    ${NSCP_DEF_PLUGIN_LIB}
    ${EXTRA_LIBS}
    # The interactive prompt (line editing, history, completion, colours).
    # Carries its own PUBLIC include dir and REPLXX_STATIC define.
    replxx
)
include(${BUILD_CMAKE_FOLDER}/module.cmake)

# The syntax layer is a pure function of the typed line plus a vocabulary
# snapshot, so it tests with no terminal, no core and no replxx.
NSCP_CREATE_TEST(
    command_client_test
    SOURCES
        console_syntax_test.cpp
        console_syntax.cpp
    LIBRARIES
        GTest::gtest_main
    INCLUDES
        ${NSCP_INCLUDEDIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
)
