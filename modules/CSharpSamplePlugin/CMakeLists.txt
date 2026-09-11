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

# The sample is built into <build>/modules/dotnet so the integration tests can
# drive it, but it is deliberately not installed: it is an example, not part of
# the product, so neither the installers nor the packages carry it.
