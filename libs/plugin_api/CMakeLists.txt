cmake_minimum_required(VERSION 3.10)
set(TARGET ${PLUGIN_API_TARGET})

include_directories(${BOOST_INCLUDE_DIRS})
link_directories(${BOOST_LIB_DIRS})

set(SRCS
    ${NSCP_INCLUDEDIR}/utf8.cpp
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_core_wrapper.cpp
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_core_helper.cpp
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper_singleton.cpp
    ${NSCP_INCLUDEDIR}/nscapi/protobuf/functions_convert.cpp
    ${NSCP_INCLUDEDIR}/nscapi/protobuf/functions_copy.cpp
    ${NSCP_INCLUDEDIR}/nscapi/protobuf/functions_perfdata.cpp
    ${NSCP_INCLUDEDIR}/nscapi/protobuf/functions_query.cpp
    ${NSCP_INCLUDEDIR}/nscapi/protobuf/functions_exec.cpp
    ${NSCP_INCLUDEDIR}/nscapi/protobuf/functions_response.cpp
    ${NSCP_INCLUDEDIR}/nscapi/protobuf/functions_submit.cpp
    ${NSCP_INCLUDEDIR}/nscapi/protobuf/functions_status.cpp
    ${NSCP_INCLUDEDIR}/nscapi/protobuf/settings_functions.cpp
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_targets.cpp
    ${NSCP_INCLUDEDIR}/nscapi/command_client.cpp
    ${NSCP_INCLUDEDIR}/nscapi/settings/filter.cpp
    ${NSCP_INCLUDEDIR}/nscapi/settings/proxy.cpp
    ${NSCP_INCLUDEDIR}/nscapi/settings/helper.cpp
)

if(WIN32)
    set(SRCS
        ${SRCS}
        ${NSCP_INCLUDEDIR}/utf8.hpp
        # ${NSCP_INCLUDEDIR}/config.h
        ${NSCP_INCLUDEDIR}/NSCAPI.h
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_core_wrapper.hpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_core_helper.hpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.hpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper_singleton.hpp
        ${NSCP_INCLUDEDIR}/nscapi/protobuf/settings_functions.hpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_targets.hpp
        ${NSCP_INCLUDEDIR}/nscapi/settings/filter.hpp
        ${NSCP_INCLUDEDIR}/nscapi/settings/proxy.hpp
        ${NSCP_INCLUDEDIR}/nscapi/settings/object.hpp
        ${NSCP_INCLUDEDIR}/nscapi/settings/helper.hpp
        ${NSCP_INCLUDEDIR}/nscapi/macros.hpp
    )
endif(WIN32)

set(EXTRA_LIBS)
if(APPLE)
    set(EXTRA_LIBS ${ICONV_LIBRARIES})
endif()

add_definitions(${NSCP_GLOBAL_DEFINES})
NSCP_MAKE_LIBRARY(${TARGET} "${SRCS}")
NSCP_FORCE_INCLUDE(
    ${TARGET}
    "${BUILD_ROOT_FOLDER}/include/nscapi/dll_defines_protobuf.hpp"
)

set_target_properties(
    ${TARGET}
    PROPERTIES
        FOLDER
            "libraries"
)
target_link_libraries(
    ${TARGET}
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    nscp_protobuf
    ${EXTRA_LIBS}
)
add_dependencies(${TARGET} nscp_protobuf)

NSCP_CREATE_TEST(
    ${TARGET}_test
    SOURCES
        nscapi_helper_test.cpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.hpp
    LIBRARIES
        GTest::gtest
        GTest::gtest_main
        ${NSCP_DEF_PLUGIN_LIB}
        ${EXTRA_LIBS}
        ${Boost_DATE_TIME_LIBRARY}
    INCLUDES
        ${NSCP_INCLUDEDIR}
)
