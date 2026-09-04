# The native half of the .NET plugin host has no build-time dependency on .NET
# (the runtime is located through hostfxr when the module loads), so it builds
# everywhere. The managed half (libs/dotnet-plugin-api, the sample plugin) needs
# the dotnet SDK and is gated on DOTNET_FOUND separately.
set(BUILD_MODULE 1)
if(NOT DOTNET_FOUND)
    set(MODULE_NOTE "managed plugin API not built: dotnet SDK not found")
endif()
