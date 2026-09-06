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
# build entirely with -DNSCP_DOTNET=OFF.
#
# `dotnet build` restores the Google.Protobuf package from nuget.org by default.
# A build without network access (distribution packaging) has three knobs:
#   -DNSCP_DOTNET_PACKAGE_SOURCE=<dir|url>  restore from a local folder holding
#                                            the .nupkg (or another feed) instead
#                                            of the configured NuGet sources
#   -DNSCP_DOTNET_NO_RESTORE=ON              skip the restore step entirely; the
#                                            packages must already sit in the
#                                            NuGet cache ($NUGET_PACKAGES or
#                                            ~/.nuget/packages)
#   -DNSCP_DOTNET_PROTOBUF_VERSION=<x.y.z>   the Google.Protobuf version to
#                                            reference, when the build host's
#                                            protoc is newer than the default
# The dotnet CLI also honours NUGET_PACKAGES and a NuGet.Config on its own; the
# variables above are simply forwarded to it. Without any of these, an offline
# build must pass -DNSCP_DOTNET=OFF or the managed targets fail the build.
option(
    NSCP_DOTNET
    "Build the managed .NET plugin API and sample plugin (requires the dotnet SDK)"
    ON
)
option(
    NSCP_DOTNET_NO_RESTORE
    "Build the managed projects with --no-restore (NuGet packages must already be cached)"
    OFF
)
set(NSCP_DOTNET_PACKAGE_SOURCE
    ""
    CACHE STRING
    "NuGet package source (local folder or feed URL) used instead of the configured sources when restoring the managed projects"
)
# Generated code from protoc 21.x needs at least this runtime; newer runtimes
# stay compatible with older generated code, but a newer protoc may require a
# newer runtime, hence the override.
set(NSCP_DOTNET_PROTOBUF_VERSION
    "3.36.1"
    CACHE STRING
    "Google.Protobuf NuGet package version the managed plugin API is built against"
)
option(
    NSCP_DOTNET_INSTALL_SAMPLE
    "Install the C# sample plugin (NSCP.Plugin.CSharpSample) into modules/dotnet alongside the API"
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
