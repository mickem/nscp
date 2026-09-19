# The native half of the PowerShell script host has no build-time dependency on
# .NET or on PowerShell: the runtime is located through hostfxr when the module
# loads and the PowerShell engine is loaded out of an installed PowerShell 7 at
# run time. So it builds everywhere. The managed half
# (libs/powershell-host/NSCP.PowerShell.dll) needs the dotnet SDK and is gated
# on DOTNET_FOUND separately.
set(BUILD_MODULE 1)
if(NOT DOTNET_FOUND)
    set(MODULE_NOTE "PowerShell script host not built: dotnet SDK not found")
endif()
