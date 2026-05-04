cmake_minimum_required(VERSION 3.15)

if(NOT NSCP_DOTNET_SUPPORTED)
    return()
endif()

project(NSCP.Plugin.CSharpSample LANGUAGES CSharp)

add_library(
    NSCP.Plugin.CSharpSample
    ${CMAKE_CURRENT_SOURCE_DIR}/SamplePlugin.cs
)

set_property(
    TARGET NSCP.Plugin.CSharpSample
    PROPERTY VS_DOTNET_REFERENCES
        System
        NSCP.Core
        NSCP.Protobuf
)

set_property(
    TARGET NSCP.Plugin.CSharpSample
    PROPERTY VS_PACKAGE_REFERENCES
        "Google.Protobuf_3.21.12"
)

set_target_properties(
    NSCP.Plugin.CSharpSample
    PROPERTIES
        VS_DOTNET_TARGET_FRAMEWORK_VERSION "v4.7.2"
        FOLDER "modules/dotnet"
)
set_library_out_folder_module(NSCP.Plugin.CSharpSample)

add_dependencies(NSCP.Plugin.CSharpSample NSCP.Core NSCP.Protobuf)
