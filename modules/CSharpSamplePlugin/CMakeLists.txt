cmake_minimum_required(VERSION 3.10)

# The target is named after the directory so the module loop in the top-level
# CMakeLists.txt can reference it. It compiles against the NSCP.Core.dll built
# into NSCP_DOTNET_OUTPUT_DIR, which is also where the plugin itself lands.
project(CSharpSamplePlugin)

NSCP_ADD_DOTNET_PROJECT(
    CSharpSamplePlugin
    ${CMAKE_CURRENT_SOURCE_DIR}/NSCP.Plugin.CSharpSample.csproj
    DEPENDS NSCP.Core
    PRODUCTS
        ${NSCP_DOTNET_OUTPUT_DIR}/NSCP.Plugin.CSharpSample.dll
        ${NSCP_DOTNET_OUTPUT_DIR}/NSCP.Plugin.CSharpSample.deps.json
)
set_target_properties(
    CSharpSamplePlugin
    PROPERTIES
        FOLDER
            "modules/dotnet"
)

# Shipped like the sample scripts are: useful to verify an installation (the
# integration tests drive it), optional for packagers who want the API only.
if(NSCP_DOTNET_INSTALL_SAMPLE)
    install(
        FILES
            ${NSCP_DOTNET_OUTPUT_DIR}/NSCP.Plugin.CSharpSample.dll
            ${NSCP_DOTNET_OUTPUT_DIR}/NSCP.Plugin.CSharpSample.deps.json
        DESTINATION ${NSCP_DOTNET_INSTALL_DIR}
    )
endif()
