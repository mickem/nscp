# Locate libzip (https://libzip.org) — the system-installed ZIP backend used
# on Linux/macOS where miniz is not packaged. CMake does not ship a built-in
# Find module for libzip; libzip itself installs a CMake config package
# (libzipConfig.cmake) on modern distros (Debian/Ubuntu's libzip-dev,
# Fedora/Rocky's libzip-devel). This module prefers pkg-config and falls back
# to the CONFIG package.
#
# Sets:
#   LIBZIP_FOUND         - whether libzip was located
#   LIBZIP_INCLUDE_DIRS  - header search paths
#   LIBZIP_LIBRARIES     - library names to link
#
# Also defines the imported target `libzip::zip` - directly on the CONFIG path,
# and synthesised on the pkg-config path so both carry the library directory.
# The libs/minizip wrapper prefers the target and falls back to
# LIBZIP_INCLUDE_DIRS / LIBZIP_LIBRARIES only if neither path produced one.
#
# Why pkg-config first? Debian's libzip-targets.cmake hard-references the
# zipcmp / zipmerge / ziptool binaries shipped by the separate libzip-tools
# package and raises FATAL_ERROR if they are missing — and that error fires
# during include() inside libzip-targets.cmake, so `find_package(libzip
# CONFIG QUIET)` cannot suppress it. pkg-config sidesteps the issue and gives
# us -lzip and the headers directly. Distros that lack pkg-config still get
# the CONFIG package as a fallback.

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(LIBZIP QUIET libzip)
endif()

# pkg_check_modules reports LIBZIP_LIBRARIES as bare names ("zip") and puts the
# directory in LIBZIP_LIBRARY_DIRS, so linking the names alone only works where
# the library happens to sit on the linker's default search path. It does on
# Debian and RedHat; it does not under a Homebrew prefix, where the macOS build
# failed with
#
#     ld: library 'zip' not found
#
# So this path exposes the same `libzip::zip` imported target the CONFIG package
# would, carrying the link directory with it - which is also what the sibling
# FindTinyXML2 does on its pkg-config path, and what makes the consumer in
# libs/minizip take its `if(TARGET libzip::zip)` branch either way rather than
# having two ways to be wrong.
if(LIBZIP_FOUND AND NOT TARGET libzip::zip)
    # Which route found it, for the configure-time report. It cannot be inferred
    # from `TARGET libzip::zip` any more, now that both routes define one.
    set(LIBZIP_SOURCE "pkg-config")
    add_library(libzip::zip INTERFACE IMPORTED)
    set_target_properties(
        libzip::zip
        PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${LIBZIP_INCLUDE_DIRS}"
            INTERFACE_LINK_LIBRARIES "${LIBZIP_LIBRARIES}"
            INTERFACE_LINK_DIRECTORIES "${LIBZIP_LIBRARY_DIRS}"
    )
endif()

if(NOT LIBZIP_FOUND)
    find_package(libzip CONFIG QUIET)
    if(TARGET libzip::zip)
        set(LIBZIP_FOUND TRUE)
        set(LIBZIP_SOURCE "CMake config package")
        # Pull the include path off the imported target so callers that
        # don't link the target directly still get the right -I flags.
        get_target_property(
            _libzip_inc
            libzip::zip
            INTERFACE_INCLUDE_DIRECTORIES
        )
        if(_libzip_inc)
            set(LIBZIP_INCLUDE_DIRS ${_libzip_inc})
        endif()
        set(LIBZIP_LIBRARIES libzip::zip)
    endif()
endif()

mark_as_advanced(
    LIBZIP_INCLUDE_DIRS
    LIBZIP_LIBRARIES
    LIBZIP_SOURCE
)
