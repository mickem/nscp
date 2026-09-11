# ##############################################################################
#
# Build profiles and build knobs
#
# Included by the top-level CMakeLists.txt right after the optional build.cmake
# (or a `cmake -C` initial-cache file / a preset) has been read, so everything
# the caller chose is already visible here as a cache entry or a normal
# variable. Nothing below overrides an explicit choice; it only fills in
# defaults - except where a *profile* requires a value, which is called out.
#
# Two kinds of settings live here.
#
# Knobs - one independent decision each. They combine freely: for instance
# NSCP_STATIC_LIBS=ON with NSCP_STATIC_RUNTIME=OFF gives a build with no
# nscp_*.dll / libnscp_*.so beside the modules that still uses the shared CRT.
#
#   NSCP_STATIC_RUNTIME       link the C/C++ runtime statically (MSVC /MT
#                             instead of /MD; no CRT redistributable shipped)
#   NSCP_STATIC_LIBS          build the project's own support libraries
#                             (plugin_api, nscp_protobuf, where_filter, ...) as
#                             static libraries linked into every module and
#                             executable, instead of shared libraries shipped
#                             beside them
#   NSCP_TARGET_WINDOWS_XP    target the Windows XP / Server 2003 API level
#                             (WINVER 0x0501); needs the v141_xp toolset
#   NSCP_WEB_BACKEND          HTTP backend for WEBServer: mongoose | beast
#                             (declared in dependencies.cmake, seeded here)
#   NSCP_PACKAGE_SUFFIX       appended to package / installer file names
#   Boost_USE_STATIC_LIBS     FindBoost: link Boost statically
#   Boost_USE_STATIC_RUNTIME  FindBoost: Boost was built against the static CRT
#
# Profiles - a named artefact this project ships, expressed as knob values.
#
#   NSCP_LEGACY_BUILD         the legacy Windows XP / Server 2003 flavour
#                             (NSCP-<ver>-Win32-legacy-xp.msi). It REQUIRES
#                             the static runtime (there is no supported CRT
#                             redistributable for XP), the XP API level and
#                             static support libraries, and it DEFAULTS the
#                             rest: static Boost, the mongoose web backend and
#                             the "-legacy-xp" package suffix. The defaults can
#                             still be overridden per build.
#
# The compiler toolset (-T v141_xp) and target architecture (-A Win32) cannot
# be chosen from inside CMake; CMakePresets.json carries them for each profile
# (`cmake --preset windows-x86-legacy`).
#
# ##############################################################################

# ------------------------------------------------------------------------------
# Deprecated spelling
#
# USE_STATIC_RUNTIME used to be the only switch and, despite its name, it
# selected the whole legacy build: static runtime, static libraries, the XP API
# level, static Boost and the -legacy-xp suffix. Keep an old build.cmake or
# command line producing the same artefact, but say so.
# ------------------------------------------------------------------------------
if(DEFINED USE_STATIC_RUNTIME)
    if(USE_STATIC_RUNTIME)
        message(
            DEPRECATION
            "USE_STATIC_RUNTIME is deprecated. It selected the whole legacy "
            "Windows XP build, which is now NSCP_LEGACY_BUILD=ON; a static "
            "runtime on its own is NSCP_STATIC_RUNTIME=ON (see "
            "build/cmake/build_profiles.cmake). Treating it as NSCP_LEGACY_BUILD=ON."
        )
        if(NOT DEFINED NSCP_LEGACY_BUILD)
            set(NSCP_LEGACY_BUILD ON)
        endif()
    else()
        message(
            DEPRECATION
            "USE_STATIC_RUNTIME is deprecated and OFF is already the default; "
            "drop it (see build/cmake/build_profiles.cmake)."
        )
    endif()
endif()

# ------------------------------------------------------------------------------
# Profiles
# ------------------------------------------------------------------------------
option(
    NSCP_LEGACY_BUILD
    "Build the legacy Windows XP / Server 2003 flavour (static runtime, static libraries, XP API level; -legacy-xp packages)"
    OFF
)

# ------------------------------------------------------------------------------
# Knobs
#
# Plain option()s: with CMP0077 (NEW) a value set in build.cmake is honoured
# and with CMP0126 (NEW) the cache default no longer clobbers it.
# ------------------------------------------------------------------------------
option(
    NSCP_STATIC_RUNTIME
    "Link the C/C++ runtime statically (MSVC /MT instead of /MD)"
    OFF
)
option(
    NSCP_STATIC_LIBS
    "Build the project's own support libraries as static libraries linked into each module and executable, instead of shared libraries shipped beside them"
    OFF
)
option(
    NSCP_TARGET_WINDOWS_XP
    "Target the Windows XP / Server 2003 API level (WINVER 0x0501; needs the v141_xp toolset)"
    OFF
)
set(NSCP_PACKAGE_SUFFIX
    ""
    CACHE STRING
    "Suffix appended to package and installer file names (e.g. -legacy-xp)"
)

# ------------------------------------------------------------------------------
# Apply the profile
# ------------------------------------------------------------------------------
if(NSCP_LEGACY_BUILD)
    message(STATUS "Build profile: legacy (Windows XP / Server 2003)")
    # Required by the profile. A normal variable shadows the cache entry, so
    # the caller's own setting survives for when the profile is switched off
    # again in the same build directory.
    set(NSCP_STATIC_RUNTIME ON)
    set(NSCP_STATIC_LIBS ON)
    set(NSCP_TARGET_WINDOWS_XP ON)
    message(
        STATUS
        " - static runtime, static libraries and the XP API level are required by this profile"
    )
    # Defaulted by the profile: only where the caller has not chosen.
    if(NOT DEFINED Boost_USE_STATIC_LIBS)
        set(Boost_USE_STATIC_LIBS ON)
    endif()
    # The legacy build is the reason mongoose is still around: Beast needs
    # Boost.Coroutine / Boost.Context, which the XP toolchain does not get. A
    # modern Windows build is free to use beast (see NSCP_WEB_BACKEND).
    if(NOT DEFINED NSCP_WEB_BACKEND)
        set(NSCP_WEB_BACKEND "mongoose")
    endif()
    if("${NSCP_PACKAGE_SUFFIX}" STREQUAL "")
        set(NSCP_PACKAGE_SUFFIX "-legacy-xp")
    endif()
else()
    message(STATUS "Build profile: default")
endif()

# ------------------------------------------------------------------------------
# Derived defaults
# ------------------------------------------------------------------------------
# Boost follows the runtime choice unless told otherwise (FindBoost variables;
# with --layout=system these only matter for which library names are probed).
if(NOT DEFINED Boost_USE_STATIC_LIBS)
    set(Boost_USE_STATIC_LIBS OFF)
endif()
if(NOT DEFINED Boost_USE_STATIC_RUNTIME)
    set(Boost_USE_STATIC_RUNTIME ${NSCP_STATIC_RUNTIME})
endif()

# The MSVC runtime library for every target created after this point
# (CMP0091 is NEW project-wide, so this also covers FetchContent'd googletest
# and any add_subdirectory()'d third-party code).
if(NSCP_STATIC_RUNTIME)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# Third-party subprojects (googletest, ...) that call add_library() without an
# explicit type follow our own libraries.
if(NSCP_STATIC_LIBS)
    set(BUILD_SHARED_LIBS OFF)
else()
    set(BUILD_SHARED_LIBS ON)
endif()

message(STATUS "Build knobs:")
message(STATUS " - NSCP_STATIC_RUNTIME:    ${NSCP_STATIC_RUNTIME}")
message(STATUS " - NSCP_STATIC_LIBS:       ${NSCP_STATIC_LIBS}")
message(STATUS " - NSCP_TARGET_WINDOWS_XP: ${NSCP_TARGET_WINDOWS_XP}")
message(STATUS " - Boost_USE_STATIC_LIBS:  ${Boost_USE_STATIC_LIBS}")
if(NOT "${NSCP_PACKAGE_SUFFIX}" STREQUAL "")
    message(STATUS " - NSCP_PACKAGE_SUFFIX:    ${NSCP_PACKAGE_SUFFIX}")
endif()
