cmake_minimum_required(VERSION 3.10)

project(NSCP.Plugin.CSharpSample)

set(CSHARP_BINARY_DIRECTORY ${NSCP_PROJECT_BINARY_DIR}/modules/dotnet)
include(${CSHARP_USE_FILE})

csharp_add_library(
  NSCP.Plugin.CSharpSample
  ${CMAKE_CURRENT_SOURCE_DIR}/SamplePlugin.cs
  ${NSCP_PROJECT_BINARY_DIR}/Google.Protobuf.dll
  ${NSCP_PROJECT_BINARY_DIR}/NSCP.Core.dll
  ${NSCP_PROJECT_BINARY_DIR}/NSCP.Protobuf.dll)
add_dependencies(NSCP.Plugin.CSharpSample NSCP.Core)
add_dependencies(NSCP.Plugin.CSharpSample NSCP.Protobuf)
set_target_properties(NSCP.Plugin.CSharpSample PROPERTIES FOLDER
                                                          "modules/dotnet")
