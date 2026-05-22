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
# Also defines the imported target `libzip::zip` when the CONFIG package is
# available; the libs/minizip wrapper prefers the target when present and
# falls back to LIBZIP_INCLUDE_DIRS / LIBZIP_LIBRARIES otherwise.
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

if(NOT LIBZIP_FOUND)
    find_package(libzip CONFIG QUIET)
    if(TARGET libzip::zip)
        set(LIBZIP_FOUND TRUE)
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
)
