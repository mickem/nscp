cmake_minimum_required(VERSION 3.10)
set(TARGET ${PLUGIN_API_TARGET})

include_directories(${BOOST_INCLUDE_DIRS})
link_directories(${BOOST_LIB_DIRS})

set(SRCS
    ${NSCP_INCLUDEDIR}/str/utf8.cpp
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
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_program_options.cpp
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_metrics_helper.cpp
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_facts_helper.cpp
    ${NSCP_INCLUDEDIR}/metrics/metrics_store_map.cpp
)

if(WIN32)
    set(SRCS
        ${SRCS}
        ${NSCP_INCLUDEDIR}/str/utf8.hpp
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
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_program_options.hpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_metrics_helper.hpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_facts_helper.hpp
        ${NSCP_INCLUDEDIR}/metrics/metrics_store_map.hpp
        ${NSCP_INCLUDEDIR}/nscapi/macros.hpp
    )
endif(WIN32)

set(EXTRA_LIBS)
if(APPLE)
    set(EXTRA_LIBS ${ICONV_LIBRARIES})
endif()

add_definitions(${NSCP_GLOBAL_DEFINES})
# The public surface is annotated with NSCAPI_EXPORT (nscapi/dll_defines.hpp),
# which most of these headers already used - what forced export-all was the
# handful that never did. Four of those are annotated now: settings/helper.hpp,
# nscapi_program_options.hpp, nscapi_metrics_helper.hpp and metrics_store_map.hpp.
# So the wholesale export can go; libs/client/CMakeLists.txt says why that is
# worth doing.
#
# utf8.cpp and nscapi_helper.cpp are still built here because this library uses
# them, but their headers stay unannotated and every consumer keeps compiling
# its own copy. They are small enough that duplicating them is cheaper than
# putting an export macro in headers this much of the tree includes; the
# reasoning is with NSCP_DEF_PLUGIN_CPP in the top-level CMakeLists.
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
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    nscp_protobuf
    # The facts builder is JSON, so every module that links the plugin API
    # gets Boost.JSON through it rather than compiling its own copy.
    nscp_json
    ${EXTRA_LIBS}
)
add_dependencies(${TARGET} nscp_protobuf)

NSCP_CREATE_TEST(
    ${TARGET}_test
    SOURCES
        nscapi_helper_test.cpp
        nscapi_program_options_test.cpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.hpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_program_options.cpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_core_helper_test.cpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_core_wrapper_test.cpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_metrics_helper.cpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_metrics_helper_test.cpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_facts_helper.cpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_facts_helper_test.cpp
    LIBRARIES
        GTest::gtest
        GTest::gtest_main
        ${NSCP_DEF_PLUGIN_LIB}
        nscp_protobuf
        nscp_json
        ${EXTRA_LIBS}
        ${Boost_DATE_TIME_LIBRARY}
        ${Boost_PROGRAM_OPTIONS_LIBRARY}
    INCLUDES
        ${NSCP_INCLUDEDIR}
)
