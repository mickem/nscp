cmake_minimum_required(VERSION 3.10)

PROJECT(NSCP.Plugin.CSharpSample LANGUAGES CSharp)


set(CSHARP_BINARY_DIRECTORY ${NSCP_PROJECT_BINARY_DIR}/modules/dotnet)

ADD_LIBRARY(
  NSCP.Plugin.CSharpSample
  ${CMAKE_CURRENT_SOURCE_DIR}/SamplePlugin.cs
)

set_property(TARGET NSCP.Plugin.CSharpSample PROPERTY VS_DOTNET_REFERENCES
        System
        System.Memory
        NSCP.Core
        NSCP.Protobuf
        # This is ugly...
        # Need to build this...
        "C:\\src\\build\\protobuf-21.12\\csharp\\src\\Google.Protobuf\\bin\\Release\\net45\\Google.Protobuf.dll"
)

set_target_properties(
    NSCP.Plugin.CSharpSample
    PROPERTIES FOLDER "modules/dotnet"
)
