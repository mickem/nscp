cmake_minimum_required(VERSION 3.10)

set(TARGET CheckPowershell)

project(${TARGET})

create_module(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(SRCS
    ${SRCS}
    "${TARGET}.cpp"
    block_allocator.cpp
    ${NSCP_INCLUDEDIR}/managed/convert.cpp
    ${NSCP_INCLUDEDIR}/managed/core_impl.cpp
    ${NSCP_INCLUDEDIR}/utf8.cpp
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_core_wrapper.cpp
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper_singleton.cpp
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_thin_plugin_impl.cpp
)

if(WIN32)
    set(SRCS
        ${SRCS}
        "${TARGET}.h"
        ${NSCP_INCLUDEDIR}/managed/convert.hpp
        ${NSCP_INCLUDEDIR}/managed/core_impl.hpp
        block_allocator.h
        ${NSCP_INCLUDEDIR}/utf8.hpp
        ${NSCP_INCLUDEDIR}/str/format.hpp
        ${NSCP_INCLUDEDIR}/str/utils.hpp
        # ${NSCP_INCLUDEDIR}/config.h
        ${NSCP_INCLUDEDIR}/NSCAPI.h
    )
endif(WIN32)



add_library(${TARGET} MODULE ${SRCS})
nscp_debug_symbols(${TARGET})

set_library_out_folder_module(${TARGET})

set_target_properties(${TARGET} PROPERTIES FOLDER "libraries/dotnet")
set_target_properties(${TARGET} PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
set_target_properties(${TARGET} PROPERTIES VS_DOTNET_TARGET_FRAMEWORK_VERSION "v4.7.2")
set_target_properties(${TARGET} PROPERTIES COMMON_LANGUAGE_RUNTIME "")
set_target_properties(${TARGET} PROPERTIES FOLDER ${MODULE_SUBFOLDER})

set_property(TARGET ${TARGET} PROPERTY VS_DOTNET_REFERENCES
        NSCP.Core
        # This is ugly...
        # Need to build this...
        "C:\\src\\build\\protobuf-21.12\\csharp\\src\\Google.Protobuf\\bin\\Release\\net45\\Google.Protobuf.dll"
)

add_dependencies(${TARGET} NSCP.Core)
add_dependencies(${TARGET} NSCP.Protobuf)

target_link_libraries(${TARGET})
include(${BUILD_CMAKE_FOLDER}/module.cmake)
