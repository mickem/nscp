cmake_minimum_required(VERSION 3.10)

# Hosting the .NET runtime from native code: locating an installed runtime,
# starting it through hostfxr and resolving the managed bridge in NSCP.Core.dll.
# Shared by the modules that host managed code (DotnetPlugins, PowerShellScript).
#
# Static on purpose, like the other per-module helper libraries: each module
# gets its own dotnet::host, and the second one to initialize is answered by
# hostfxr with Success_HostAlreadyInitialized and handed a delegate into the
# runtime the first one started. The runtime, and therefore the single loaded
# copy of NSCP.Core.dll, stays shared.
#
# Nothing here has a build-time dependency on .NET, so the library builds
# everywhere; the runtime is located when a module loads.

set(TARGET nscp_dotnet_host)

include_directories(${BOOST_INCLUDE_DIRS})
link_directories(${BOOST_LIB_DIRS})
add_definitions(${NSCP_GLOBAL_DEFINES})

set(SRCS
    ${NSCP_INCLUDEDIR}/dotnet/host.cpp
    ${NSCP_INCLUDEDIR}/dotnet/runtime.cpp
)

if(WIN32)
    set(SRCS
        ${SRCS}
        ${NSCP_INCLUDEDIR}/dotnet/bridge.hpp
        ${NSCP_INCLUDEDIR}/dotnet/host.hpp
        ${NSCP_INCLUDEDIR}/dotnet/runtime.hpp
        ${NSCP_INCLUDEDIR}/dotnet/hostfxr/coreclr_delegates.h
        ${NSCP_INCLUDEDIR}/dotnet/hostfxr/hostfxr.h
    )
endif(WIN32)

add_library(${TARGET} STATIC ${SRCS})
NSCP_DEBUG_SYMBOLS(${TARGET})

target_link_libraries(
    ${TARGET}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_SYSTEM_LIBRARY}
    ${CMAKE_DL_LIBS}
)
if(WIN32)
    # RegOpenKeyExW & co. for locating the .NET install through the registry.
    target_link_libraries(${TARGET} advapi32)
endif()

set_target_properties(
    ${TARGET}
    PROPERTIES
        FOLDER
            "libraries"
)

if(CMAKE_COMPILER_IS_GNUCXX)
    nscp_apply_pic(${TARGET})
endif(CMAKE_COMPILER_IS_GNUCXX)

NSCP_CREATE_TEST(
    dotnet_host_test
    SOURCES
        host_test.cpp
    LIBRARIES
        GTest::gtest_main
        ${TARGET}
        ${Boost_FILESYSTEM_LIBRARY}
        ${Boost_SYSTEM_LIBRARY}
        ${CMAKE_DL_LIBS}
    INCLUDES
        ${NSCP_INCLUDEDIR}
)
