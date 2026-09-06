# Locate the .NET SDK (`dotnet` CLI) used to build the managed half of the
# .NET plugin support (libs/dotnet-plugin-api and the C# sample plugin).
#
# The native DotnetPlugins module itself has no build-time dependency on .NET;
# only these managed projects do, and they are skipped when no SDK is found.
#
# Sets:
#   DOTNET_FOUND       - TRUE when an SDK was found (and NSCP_DOTNET is ON)
#   DOTNET_EXECUTABLE  - the dotnet CLI
#   DOTNET_VERSION     - the SDK version it reports
#
# Override with -DDOTNET_EXECUTABLE=/path/to/dotnet, or disable the managed
# build entirely with -DNSCP_DOTNET=OFF. Note that `dotnet build` restores the
# Google.Protobuf package from nuget.org (or a configured NuGet source): a build
# on a machine without that access must pass -DNSCP_DOTNET=OFF, or the managed
# targets fail the build.
option(
    NSCP_DOTNET
    "Build the managed .NET plugin API and sample plugin (requires the dotnet SDK)"
    ON
)

set(DOTNET_FOUND FALSE)
if(NSCP_DOTNET)
    find_program(
        DOTNET_EXECUTABLE
        NAMES
            dotnet
            dotnet.exe
        HINTS
        ENV DOTNET_ROOT
        PATHS
            "$ENV{ProgramFiles}/dotnet"
            "$ENV{LOCALAPPDATA}/Microsoft/dotnet"
            /usr/lib/dotnet
            /usr/share/dotnet
            /usr/local/share/dotnet
            /opt/dotnet
            "$ENV{HOME}/.dotnet"
        DOC "The dotnet CLI (SDK) used to build the managed plugin API"
    )
    if(DOTNET_EXECUTABLE)
        # `dotnet --version` reports the SDK version and fails when only a
        # runtime (no SDK) is installed, which is exactly the distinction we need.
        execute_process(
            COMMAND
                ${DOTNET_EXECUTABLE} --version
            RESULT_VARIABLE _dotnet_rc
            OUTPUT_VARIABLE DOTNET_VERSION
            ERROR_VARIABLE _dotnet_err
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_STRIP_TRAILING_WHITESPACE
        )
        if(_dotnet_rc EQUAL 0 AND DOTNET_VERSION)
            # The managed projects target net8.0: an older SDK would accept the
            # build here and then fail inside `dotnet build` (NETSDK1045),
            # taking the whole build down instead of skipping the managed half.
            string(REGEX MATCH "^[0-9]+" _dotnet_major "${DOTNET_VERSION}")
            if(_dotnet_major AND _dotnet_major GREATER_EQUAL 8)
                set(DOTNET_FOUND TRUE)
            else()
                message(
                    STATUS
                    " ! dotnet SDK ${DOTNET_VERSION} at ${DOTNET_EXECUTABLE} is too old: the managed .NET plugin API needs SDK 8.0 or later"
                )
                set(DOTNET_VERSION "")
            endif()
        else()
            set(DOTNET_VERSION "")
            message(
                STATUS
                " ! dotnet found at ${DOTNET_EXECUTABLE} but it has no SDK: ${_dotnet_err}"
            )
        endif()
    endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
    Dotnet
    REQUIRED_VARS
        DOTNET_EXECUTABLE
        DOTNET_FOUND
    VERSION_VAR DOTNET_VERSION
)
mark_as_advanced(DOTNET_EXECUTABLE)
