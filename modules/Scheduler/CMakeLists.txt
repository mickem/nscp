cmake_minimum_required(VERSION 3.10)

set(TARGET Scheduler)

project(${TARGET})

CREATE_MODULE(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(SRCS
    ${SRCS}
    "${TARGET}.cpp"
    ${NSCP_INCLUDEDIR}/scheduler/simple_scheduler.cpp
    ${NSCP_INCLUDEDIR}/threads/has-threads.cpp
    schedules_handler.cpp
    ${NSCP_DEF_PLUGIN_CPP}
)

add_definitions(${NSCP_GLOBAL_DEFINES})

if(WIN32)
    set(SRCS
        ${SRCS}
        "${TARGET}.h"
        ${NSCP_INCLUDEDIR}/scheduler/simple_scheduler.hpp
        schedules_handler.hpp
        ${NSCP_INCLUDEDIR}/threads/has-threads.hpp
        ${NSCP_DEF_PLUGIN_HPP}
    )
endif(WIN32)

add_library(Scheduler MODULE ${SRCS})

target_link_libraries(
    Scheduler
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${NSCP_DEF_PLUGIN_LIB}
)
include(${BUILD_CMAKE_FOLDER}/module.cmake)

NSCP_CREATE_TEST(
    schedules_module_test
    SOURCES
        schedules_handler_test.cpp
        schedules_handler.cpp
        ${NSCP_INCLUDEDIR}/scheduler/simple_scheduler.cpp
        ${NSCP_INCLUDEDIR}/threads/has-threads.cpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp
        ${NSCP_INCLUDEDIR}/str/utf8.cpp
        ${NSCP_INCLUDEDIR}/nscapi/settings/helper.cpp
    LIBRARIES
        GTest::gtest_main
        ${Boost_FILESYSTEM_LIBRARY}
        ${Boost_THREAD_LIBRARY}
        ${Boost_CHRONO_LIBRARY}
        ${Boost_SYSTEM_LIBRARY}
        ${Boost_DATE_TIME_LIBRARY}
    INCLUDES
        ${NSCP_INCLUDEDIR}
        ${CMAKE_SOURCE_DIR}/modules/Scheduler
)
