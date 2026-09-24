# -*- cmake -*-
#
# Find crypto++ library or sources.
#
# Input variables: CRYPTOPP_DIR    - set this to specify the crypto++ source to
# be built.
#
# Output variables:
#
# CRYPTOPP_FOUND                  - set if library was found
# CRYPTOPP_INCLUDE_DIR    - Set to where include files ar found (or sources)
# CRYPTOPP_LIBRARIES              - Set to library

# The headers are included unqualified (`#include <cryptlib.h>`, see
# libs/nscpcrypt/nscpcrypt.cpp), so what is wanted here is the directory the
# headers are *in* - /usr/include/crypto++ on Debian, <prefix>/include/cryptopp
# from a package manager - not the prefix above it. CRYPTOPP_ROOT is accepted as
# either: pass `brew --prefix cryptopp` on macOS and the include/cryptopp
# spelling below picks it up.
find_path(
    CRYPTOPP_INCLUDE_DIR
    cryptlib.h
    PATHS
        ${CRYPTOPP_DIR}
        ${CRYPTOPP_ROOT}
        ${CRYPTOPP_ROOT}/include/cryptopp
        ${CRYPTOPP_ROOT}/include/crypto++
        /usr/include/crypto++
        /usr/include/cryptopp
        /usr/include
)

# Crypto++ stages its libraries under $(Platform)/Output/$(Configuration), so
# the directory is named after the Visual Studio platform verbatim: Win32, x64
# or ARM64. That tree is always produced by msbuild (see
# .github/actions/cryptopp), so the names are the msbuild ones whatever
# generator consumes them here.
nscp_target_arch(_CRYPTOPP_ARCH)
if(_CRYPTOPP_ARCH STREQUAL "arm64")
    set(CRYPTOPP_LIB_ROOT ${CRYPTOPP_INCLUDE_DIR}/ARM64)
elseif(_CRYPTOPP_ARCH STREQUAL "x64")
    set(CRYPTOPP_LIB_ROOT ${CRYPTOPP_INCLUDE_DIR}/x64)
else()
    set(CRYPTOPP_LIB_ROOT ${CRYPTOPP_INCLUDE_DIR}/Win32)
endif()

# The installed-prefix layout, where there is one build rather than a
# debug/release pair - so both lookups resolve to the same file, as they already
# do on Debian. CRYPTOPP_FOUND requires both, so leaving it out of one of them
# would disable Crypto++ entirely (and with it NSCA encryption).
#
# Guarded on CRYPTOPP_ROOT being set: interpolating an unset one yields the bare
# "/lib", which find_library also tries as "/lib64" - a real directory holding
# libcryptopp.so on RedHat - ahead of /usr/lib, silently changing which path a
# working Linux build records.
set(_CRYPTOPP_ROOT_LIBDIRS)
if(CRYPTOPP_ROOT)
    set(_CRYPTOPP_ROOT_LIBDIRS ${CRYPTOPP_ROOT}/lib)
endif()
find_library(
    CRYPTOPP_LIBRARIES_RELEASE
    NAMES
        crypto++
        cryptlib
        cryptopp
    PATHS
        ${CRYPTOPP_LIB_ROOT}/Output/Release
        ${CRYPTOPP_LIB_ROOT}/Output
        ${_CRYPTOPP_ROOT_LIBDIRS}
        /usr/lib/
)
find_library(
    CRYPTOPP_LIBRARIES_DEBUG
    NAMES
        crypto++
        cryptlib
        cryptopp
    PATHS
        ${CRYPTOPP_LIB_ROOT}/Output/Debug
        ${CRYPTOPP_LIB_ROOT}/Output
        ${_CRYPTOPP_ROOT_LIBDIRS}
        /usr/lib/
)

if(CMAKE_TRACE)
    message(STATUS " - CRYPTOPP_INCLUDE_DIR=${CRYPTOPP_INCLUDE_DIR}")
    message(STATUS " - CRYPTOPP_LIB_ROOT=${CRYPTOPP_LIB_ROOT}")
    message(STATUS " - CRYPTOPP_LIBRARIES_DEBUG=${CRYPTOPP_LIBRARIES_DEBUG}")
    message(
        STATUS
        " - CRYPTOPP_LIBRARIES_RELEASE=${CRYPTOPP_LIBRARIES_RELEASE}"
    )
    message(STATUS " - CRYPTOPP_DIR=${CRYPTOPP_DIR}")
endif()

if(
    CRYPTOPP_INCLUDE_DIR
    AND CRYPTOPP_LIBRARIES_RELEASE
    AND CRYPTOPP_LIBRARIES_DEBUG
)
    set(CRYPTOPP_FOUND TRUE)
    set(CRYPTOPP_LIBRARIES
        optimized
        ${CRYPTOPP_LIBRARIES_RELEASE}
        debug
        ${CRYPTOPP_LIBRARIES_DEBUG}
    )
endif()
if(CMAKE_TRACE)
    message(STATUS " - CRYPTOPP_FOUND=${CRYPTOPP_FOUND}")
    message(STATUS " - CRYPTOPP_LIBRARIES=${CRYPTOPP_LIBRARIES}")
endif()
