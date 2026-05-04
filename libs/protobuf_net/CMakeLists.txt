cmake_minimum_required(VERSION 3.15)

if(NOT NSCP_DOTNET_SUPPORTED)
    return()
endif()

project(NSCP.Protobuf LANGUAGES CSharp)

# `NSCP.Protobuf` is a pure C# assembly that wraps the protobuf-generated C#
# stubs together with the hand-written `Helpers.cs`.  The generated files are
# produced by `WRAP_PROTO` (see `libs/protobuf/CMakeLists.txt`) into
# `${PROJECT_BINARY_DIR}/libs/protobuf_net`; we list them here as absolute
# paths and depend on the `nscp_protobuf` target so protoc has already run by
# the time MSBuild compiles this project.
set(NSCP_PROTOBUF_NET_GENERATED_DIR "${PROJECT_BINARY_DIR}/libs/protobuf_net")
set(NSCP_PROTOBUF_NET_GENERATED_SRCS
    "${NSCP_PROTOBUF_NET_GENERATED_DIR}/Plugin.cs"
    "${NSCP_PROTOBUF_NET_GENERATED_DIR}/Metrics.cs"
    "${NSCP_PROTOBUF_NET_GENERATED_DIR}/Log.cs"
    "${NSCP_PROTOBUF_NET_GENERATED_DIR}/Registry.cs"
    "${NSCP_PROTOBUF_NET_GENERATED_DIR}/Settings.cs"
    "${NSCP_PROTOBUF_NET_GENERATED_DIR}/Storage.cs"
    "${NSCP_PROTOBUF_NET_GENERATED_DIR}/Common.cs"
)
set_source_files_properties(
    ${NSCP_PROTOBUF_NET_GENERATED_SRCS}
    PROPERTIES GENERATED TRUE
)

add_library(
    NSCP.Protobuf
    ${CMAKE_CURRENT_SOURCE_DIR}/Helpers.cs
    ${NSCP_PROTOBUF_NET_GENERATED_SRCS}
)

# Project-to-project reference to the managed C++/CLI assembly.
set_property(
    TARGET NSCP.Protobuf
    PROPERTY VS_DOTNET_REFERENCES
        System
        NSCP.Core
)

# Pull in `Google.Protobuf` from NuGet rather than relying on a hand-built
# `Google.Protobuf.dll`.  The version is pinned to match the C++ protobuf
# version used by the rest of the build.
set_property(
    TARGET NSCP.Protobuf
    PROPERTY VS_PACKAGE_REFERENCES
        "Google.Protobuf_3.21.12"
)

set_target_properties(
    NSCP.Protobuf
    PROPERTIES
        VS_DOTNET_TARGET_FRAMEWORK_VERSION "v4.7.2"
        FOLDER "libraries/dotnet"
)
set_library_out_folder(NSCP.Protobuf)

# `nscp_protobuf` (the C++ proto library) carries the `add_custom_command`
# that runs `protoc` and produces the C# files; depending on it ensures the
# files exist before MSBuild starts compiling `NSCP.Protobuf`.
add_dependencies(NSCP.Protobuf nscp_protobuf NSCP.Core)

install(TARGETS NSCP.Protobuf DESTINATION .)
