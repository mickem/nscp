cmake_minimum_required(VERSION 3.10)

set(TARGET DotnetPlugins)

project(${TARGET})

set(SRCS
    ${SRCS}
    "${TARGET}.cpp"
    plugin_instance.cpp
    block_allocator.cpp
    json.cpp
    json_writer.cpp
    ${NSCP_INCLUDEDIR}/utf8.cpp
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_core_wrapper.cpp
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper_singleton.cpp
)

add_definitions(${NSCP_GLOBAL_DEFINES} -Dplugin_api_NOLIB)

if(WIN32)
    set(SRCS
        ${SRCS}
        "${TARGET}.h"
        plugin_instance.hpp
        DotnetPlugins.def
        block_allocator.h
        json.h
        json_writer.h
        ${NSCP_INCLUDEDIR}/utf8.hpp
        ${NSCP_INCLUDEDIR}/str/format.hpp
        ${NSCP_INCLUDEDIR}/str/utils.hpp
        # ${NSCP_INCLUDEDIR}/config.h
        ${NSCP_INCLUDEDIR}/NSCAPI.h
    )
endif(WIN32)

string(REPLACE "/EHsc" "/EHa" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
string(REPLACE "/EHs" "/EHa" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
string(REPLACE "/MT" "/MD" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
string(
    REPLACE
    "/EHsc"
    "/EHa"
    CMAKE_CXX_FLAGS_RELWITHDEBINFO
    ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}
)
string(
    REPLACE
    "/EHs"
    "/EHa"
    CMAKE_CXX_FLAGS_RELWITHDEBINFO
    ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}
)
string(
    REPLACE
    "/MT"
    "/MD"
    CMAKE_CXX_FLAGS_RELWITHDEBINFO
    ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}
)
string(
    REPLACE
    "/MT"
    "/MD"
    CMAKE_CXX_FLAGS_MINSIZEREL
    ${CMAKE_CXX_FLAGS_MINSIZEREL}
)
string(REPLACE "/MT" "/MD" CMAKE_CXX_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE})
string(REPLACE "/EHsc" "/EHa" CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})
string(REPLACE "/EHs" "/EHa" CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})
string(REPLACE "/MTd" "/MDd" CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})
string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})
string(REPLACE "/RTCs" "" CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})
string(REPLACE "/RTCu" "" CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})
string(REPLACE "/GZ" "" CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})
set(CMAKE_CXX_FLAGS
    "${CMAKE_CXX_FLAGS} /clr /EHa /FU${NSCP_PROJECT_BINARY_DIR}/Google.Protobuf.dll /FU${NSCP_PROJECT_BINARY_DIR}/NSCP.Core.dll /FU${NSCP_PROJECT_BINARY_DIR}/NSCP.Protobuf.dll"
)
add_library(${TARGET} MODULE ${SRCS})
nscp_debug_symbols(${TARGET})

add_dependencies(${TARGET} NSCP.Core)
add_dependencies(${TARGET} NSCP.Protobuf)

target_link_libraries(${TARGET})
include(${BUILD_CMAKE_FOLDER}/module.cmake)
