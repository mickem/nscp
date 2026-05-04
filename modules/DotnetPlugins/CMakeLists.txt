cmake_minimum_required(VERSION 3.15)

set(TARGET DotnetPlugins)

project(${TARGET})

set(SRCS
    ${SRCS}
    "${TARGET}.cpp"
    plugin_instance.cpp
    block_allocator.cpp
    json.cpp
    json_writer.cpp
    ${NSCP_INCLUDEDIR}/str/utf8.cpp
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_core_wrapper.cpp
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper_singleton.cpp
)

if(WIN32)
    set(SRCS
        ${SRCS}
        "${TARGET}.h"
        plugin_instance.hpp
        DotnetPlugins.def
        block_allocator.h
        json.h
        json_writer.h
        ${NSCP_INCLUDEDIR}/str/utf8.hpp
        ${NSCP_INCLUDEDIR}/str/format.hpp
        ${NSCP_INCLUDEDIR}/str/utils.hpp
        ${NSCP_INCLUDEDIR}/NSCAPI.h
    )
endif(WIN32)

# C++/CLI assemblies must be built as SHARED libraries; CMake's MODULE +
# `COMMON_LANGUAGE_RUNTIME` combination produces vcxprojs that confuse
# MSBuild's CLR linker.
add_library(${TARGET} SHARED ${SRCS})
nscp_setup_clr_target(${TARGET})
nscp_debug_symbols(${TARGET})

set_target_properties(${TARGET} PROPERTIES FOLDER ${MODULE_SUBFOLDER})

set_property(
    TARGET ${TARGET}
    PROPERTY VS_DOTNET_REFERENCES
        NSCP.Core
        NSCP.Protobuf
)

# `Google.Protobuf` is brought in via NuGet so we don't need a hard-coded
# `Google.Protobuf.dll` path.  The `/FU` (force-using) directives that used
# to reference local DLLs are no longer required because the managed
# references above are visible to the C++/CLI compiler automatically.
set_property(
    TARGET ${TARGET}
    PROPERTY VS_PACKAGE_REFERENCES
        "Google.Protobuf_3.21.12"
)

add_dependencies(${TARGET} NSCP.Core NSCP.Protobuf)

target_link_libraries(${TARGET})
include(${BUILD_CMAKE_FOLDER}/module.cmake)
