cmake_minimum_required(VERSION 3.10)
if(WIN32)
    project(NSCP.Core)

    set(SRCS AssemblyInfo.cpp NSCP.h)

    add_library(NSCP.Core SHARED ${SRCS})
    set_library_out_folder(NSCP.Core)

    set_target_properties(NSCP.Core PROPERTIES FOLDER "libraries/dotnet")
    set_target_properties(NSCP.Core PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
    set_target_properties(NSCP.Core PROPERTIES VS_DOTNET_TARGET_FRAMEWORK_VERSION "v4.7.2")
    set_target_properties(NSCP.Core PROPERTIES COMMON_LANGUAGE_RUNTIME "")
    install(TARGETS NSCP.Core DESTINATION .)
endif(WIN32)
