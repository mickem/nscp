cmake_minimum_required(VERSION 3.15)

set(TARGET CheckPowershell)

project(${TARGET})

CREATE_MODULE(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(SRCS
    ${SRCS}
    "${TARGET}.cpp"
    block_allocator.cpp
    ${NSCP_INCLUDEDIR}/managed/convert.cpp
    ${NSCP_INCLUDEDIR}/managed/core_impl.cpp
    ${NSCP_INCLUDEDIR}/str/utf8.cpp
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
        ${NSCP_INCLUDEDIR}/str/utf8.hpp
        ${NSCP_INCLUDEDIR}/str/format.hpp
        ${NSCP_INCLUDEDIR}/str/utils.hpp
        ${NSCP_INCLUDEDIR}/NSCAPI.h
    )
endif(WIN32)

# `CheckPowershell` hosts the .NET PowerShell runtime in-process via
# `System.Management.Automation`.  That assembly is part of the .NET Framework
# and lives in the GAC; locate it at configure time so the source code does
# not need to embed a hard-coded path.
find_file(
    NSCP_SYSTEM_MANAGEMENT_AUTOMATION_DLL
    NAMES System.Management.Automation.dll
    HINTS
        ENV ProgramFiles
        ENV "ProgramFiles(x86)"
        ENV WINDIR
    PATH_SUFFIXES
        "Reference Assemblies/Microsoft/WindowsPowerShell/3.0"
        "Reference Assemblies/Microsoft/WindowsPowerShell/v1.0"
        "assembly/GAC_MSIL/System.Management.Automation"
)
if(NOT NSCP_SYSTEM_MANAGEMENT_AUTOMATION_DLL)
    # Fall back to a recursive search of the GAC; the directory under
    # `assembly/GAC_MSIL/System.Management.Automation` contains a
    # version-stamped subdirectory that varies per machine.
    file(
        GLOB_RECURSE _sma_candidates
        "$ENV{WINDIR}/assembly/GAC_MSIL/System.Management.Automation/*/System.Management.Automation.dll"
        "$ENV{WINDIR}/Microsoft.NET/assembly/GAC_MSIL/System.Management.Automation/*/System.Management.Automation.dll"
    )
    if(_sma_candidates)
        list(GET _sma_candidates 0 NSCP_SYSTEM_MANAGEMENT_AUTOMATION_DLL)
    endif()
endif()
if(NOT NSCP_SYSTEM_MANAGEMENT_AUTOMATION_DLL)
    message(
        FATAL_ERROR
        "CheckPowershell requires System.Management.Automation.dll. "
        "Set NSCP_SYSTEM_MANAGEMENT_AUTOMATION_DLL on the cmake command line "
        "to the full path of the assembly (typically under "
        "%WINDIR%/assembly/GAC_MSIL/System.Management.Automation/...)."
    )
endif()
message(
    STATUS
    " - System.Management.Automation: ${NSCP_SYSTEM_MANAGEMENT_AUTOMATION_DLL}"
)

# C++/CLI assemblies must be built as SHARED libraries; see DotnetPlugins.
add_library(${TARGET} SHARED ${SRCS})
nscp_setup_clr_target(${TARGET})
nscp_debug_symbols(${TARGET})

# Force-using replaces the historical `#using <C:\\...>` directive in
# CheckPowershell.cpp with a compile option whose value is discovered at
# configure time, removing the hard-coded developer-machine path.
target_compile_options(
    ${TARGET}
    PRIVATE "/FU${NSCP_SYSTEM_MANAGEMENT_AUTOMATION_DLL}"
)

set_target_properties(${TARGET} PROPERTIES FOLDER ${MODULE_SUBFOLDER})

set_property(
    TARGET ${TARGET}
    PROPERTY VS_DOTNET_REFERENCES
        NSCP.Core
        NSCP.Protobuf
)

set_property(
    TARGET ${TARGET}
    PROPERTY VS_PACKAGE_REFERENCES
        "Google.Protobuf_3.21.12"
)

add_dependencies(${TARGET} NSCP.Core NSCP.Protobuf)

target_link_libraries(${TARGET})
include(${BUILD_CMAKE_FOLDER}/module.cmake)
