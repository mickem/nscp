# Sample plugin written in C#: only buildable with the dotnet SDK.
if(DOTNET_FOUND)
    set(BUILD_MODULE 1)
else()
    set(BUILD_MODULE_SKIP_REASON "dotnet SDK not found")
endif()
