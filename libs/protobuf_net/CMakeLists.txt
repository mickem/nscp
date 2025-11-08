cmake_minimum_required(VERSION 3.10)

PROJECT(NSCP.Protobuf LANGUAGES CSharp)


#file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/Google.Protobuf.dll DESTINATION  ${NSCP_PROJECT_BINARY_DIR})

SET(CSHARP_BINARY_DIRECTORY ${NSCP_PROJECT_BINARY_DIR})

SET(CSHARP_BINARY_DIRECTORY ${BUILD_TARGET_EXE_PATH})
ADD_LIBRARY(NSCP.Protobuf Helpers.cs Plugin.cs Metrics.cs Log.cs Registry.cs Settings.cs Storage.cs Common.cs)

set_property(TARGET NSCP.Protobuf PROPERTY VS_DOTNET_REFERENCES
        #Microsoft.CSharp
        System
        System.Memory
        NSCP.Core
        # This is ugly...
        # Need to build this...
        "C:\\src\\build\\protobuf-21.12\\csharp\\src\\Google.Protobuf\\bin\\Release\\net45\\Google.Protobuf.dll"
)

add_dependencies(${TARGET} NSCP.Core)
set_library_out_folder(NSCP.Protobuf)

SET_TARGET_PROPERTIES(NSCP.Protobuf PROPERTIES FOLDER  "libraries/dotnet")
