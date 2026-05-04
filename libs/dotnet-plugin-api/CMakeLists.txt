cmake_minimum_required(VERSION 3.15)

if(NOT NSCP_DOTNET_SUPPORTED)
    return()
endif()

project(NSCP.Core)

set(SRCS
    AssemblyInfo.cpp
    NSCP.h
)

# `NSCP.Core` is a managed C++/CLI assembly.  It must be a SHARED library so
# that CMake produces a vcxproj with `<ConfigurationType>DynamicLibrary</...>`
# and `<CLRSupport>true</CLRSupport>` (a MODULE library on Windows turns into
# the same DLL but the combination with `COMMON_LANGUAGE_RUNTIME` produces
# spurious linker subsystem warnings).
add_library(NSCP.Core SHARED ${SRCS})
nscp_setup_clr_target(NSCP.Core)
set_library_out_folder(NSCP.Core)

set_target_properties(
    NSCP.Core
    PROPERTIES
        FOLDER "libraries/dotnet"
)
install(TARGETS NSCP.Core DESTINATION .)
