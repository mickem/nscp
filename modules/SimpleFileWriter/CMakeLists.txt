cmake_minimum_required(VERSION 3.10)

set(TARGET SimpleFileWriter)

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

target_link_libraries(
    ${TARGET}
    ${Boost_FILESYSTEM_LIBRARY}
    ${NSCP_DEF_PLUGIN_LIB}
    ${Boost_THREAD_LIBRARY}
    expression_parser
)

include(${BUILD_CMAKE_FOLDER}/module.cmake)

# Unit tests for the module class itself: the settings it registers/reads, the
# syntax it compiles into lookup functors and the line it writes. The stub core
# (include/nscapi/test_helpers.hpp) stands in for the daemon.
NSCP_CREATE_TEST(
    simple_file_writer_module_test
    SOURCES
        simple_file_writer_module_test.cpp
        "${TARGET}.cpp"
        ${NSCP_DEF_PLUGIN_CPP}
    LIBRARIES
        GTest::gtest_main
        ${NSCP_DEF_PLUGIN_LIB}
        ${Boost_FILESYSTEM_LIBRARY}
        ${Boost_THREAD_LIBRARY}
        ${Boost_DATE_TIME_LIBRARY}
        expression_parser
    INCLUDES
        ${NSCP_INCLUDEDIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
)
