# NCPAServer serves HTTPS through nscp_mongoose, the library the WEB server
# uses, which is only built when the selected web backend's dependencies are
# satisfied (NSCP_MONGOOSE_AVAILABLE in build/cmake/dependencies.cmake). Skip
# the module rather than fail the build when it is not, so a build that has
# to leave the web backend out (-DBUILD_MODULE_WEBServer=OFF) still configures.
if(NSCP_MONGOOSE_AVAILABLE)
    set(BUILD_MODULE 1)
else()
    set(BUILD_MODULE_SKIP_REASON
        "the nscp_mongoose web backend is not available (see NSCP_WEB_BACKEND)"
    )
endif()
