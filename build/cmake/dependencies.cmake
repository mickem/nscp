cmake_minimum_required(VERSION 3.10)

# ##############################################################################
#
# Find all dependencies and report anything missing.
#
# ##############################################################################
message(STATUS "Looking for dependencies:")
# The Python *interpreter* is required by the build itself (protobuf / module
# code generation). The *development* libraries are only needed to embed Python
# in the PythonScript module, so they are optional — a missing libpython simply
# disables that one module.
find_package(
    Python3
    COMPONENTS
        Interpreter
    OPTIONAL_COMPONENTS
        Development
)
find_package(TinyXML2)
find_package(CryptoPP)
find_package(Lua)
if(NOT Lua_FOUND)
    find_package(LUASource)
endif()

find_package(PROTOC_GEN_LUA)
find_package(ProtocGenMd)
find_package(ProtoBuf)
# The unit tests link the googletest pulled in via FetchContent (see the
# top-level CMakeLists), so a system Google Test is not actually required. We
# still probe for it for the diagnostic report below, but only when tests are
# being built.
if(NSCP_BUILD_TESTS)
    find_package(GTest)
endif()
find_package(OpenSSL)
# Two separate backends for the ZIP archive reader in service/plugins/
# zip_plugin.cpp; only the matching one is required for the current platform.
# Windows uses the vendored miniz source; Linux/macOS uses libzip from the
# system. The libs/minizip wrapper hides the choice from callers.
if(WIN32)
    find_package(Miniz)
else()
    find_package(LibZip)
endif()
# Record which ZIP backend (if any) the libs/minizip wrapper will use. "none"
# is a supported configuration: the wrapper builds a stub and zip_archive
# reading is disabled (zip_plugin / unzip return failure). See
# libs/minizip/CMakeLists.txt and include/bytes/unzip.cpp.
if(MINIZ_FOUND)
    set(NSCP_ZIP_BACKEND "miniz")
elseif(LIBZIP_FOUND)
    set(NSCP_ZIP_BACKEND "libzip")
else()
    set(NSCP_ZIP_BACKEND "none")
endif()
set(NSCP_ZIP_BACKEND
    "${NSCP_ZIP_BACKEND}"
    CACHE STRING
    "Resolved ZIP backend: miniz | libzip | none"
    FORCE
)
# HTTP backend selector for WEBServer. The mongoose default keeps the
# Windows packaging story unchanged; setting `beast` switches every
# platform to the Boost.Beast implementation (libs/mongoose-cpp/
# ServerBeastImpl.cpp) and drops the vendored mongoose download from
# the build. See docs/design/beast-web-backend.md.
# Seed the default only when nothing else has chosen a backend. This must
# honour BOTH a command-line `-DNSCP_WEB_BACKEND=...` (a cache entry) AND a
# `SET(NSCP_WEB_BACKEND ...)` in build.cmake (a normal variable, since
# build.cmake is include()d earlier in the top-level CMakeLists).
#
# A bare `set(NSCP_WEB_BACKEND "mongoose" CACHE STRING ...)` here is a trap:
# when no cache entry exists yet, CMake creates it AND removes any normal
# variable of the same name — silently reverting build.cmake's
# `SET(NSCP_WEB_BACKEND "beast")` back to mongoose. That broke the Linux
# (beast) package builds, which opt in via build.cmake rather than -D.
if(NOT DEFINED NSCP_WEB_BACKEND)
    set(NSCP_WEB_BACKEND "mongoose")
endif()
set(NSCP_WEB_BACKEND
    "${NSCP_WEB_BACKEND}"
    CACHE STRING
    "HTTP backend for WEBServer: mongoose | beast"
)
set_property(CACHE NSCP_WEB_BACKEND PROPERTY STRINGS mongoose beast)

# Decide whether the nscp_mongoose web library can be built. It is only
# built when the selected backend's dependencies are satisfied:
#   mongoose -> the vendored mongoose source is found
#   beast    -> OpenSSL is available (ServerBeastImpl includes Boost.Asio
#               SSL headers, which need OpenSSL headers/libs at compile time
#               even when TLS is not enabled at runtime)
# When they are not, we skip building nscp_mongoose rather than failing the
# whole configure; modules/WEBServer/CMakeLists.txt raises a targeted error
# if it is asked to build without the library.
set(NSCP_MONGOOSE_AVAILABLE FALSE)
if(NSCP_WEB_BACKEND STREQUAL "mongoose")
    find_package(Mongoose)
    if(MONGOOSE_FOUND)
        set(NSCP_MONGOOSE_AVAILABLE TRUE)
    else()
        message(
            STATUS
            "NSCP_WEB_BACKEND=mongoose but the vendored mongoose source was not "
            "found; nscp_mongoose (and the WEBServer module) will not be built. "
            "Set MONGOOSE_SOURCE_DIR (see build.md), or switch to the Beast "
            "backend with -DNSCP_WEB_BACKEND=beast (the default on Linux CI "
            "builds)."
        )
    endif()
elseif(NSCP_WEB_BACKEND STREQUAL "beast")
    # Beast is header-only; the Boost components (coroutine + context)
    # needed by ServerBeastImpl are added below.
    if(OPENSSL_FOUND)
        set(NSCP_MONGOOSE_AVAILABLE TRUE)
        message(STATUS "WEB backend: Boost.Beast (mongoose download skipped)")
    else()
        message(
            STATUS
            "NSCP_WEB_BACKEND=beast but OpenSSL was not found; nscp_mongoose "
            "(and the WEBServer module) will not be built. ServerBeastImpl "
            "includes Boost.Asio SSL headers, so OpenSSL must be available at "
            "configure/build time even if TLS is not enabled at runtime. "
            "Install/configure OpenSSL, or switch to -DNSCP_WEB_BACKEND=mongoose."
        )
    endif()
else()
    message(FATAL_ERROR "Unknown NSCP_WEB_BACKEND='${NSCP_WEB_BACKEND}' (expected mongoose | beast)")
endif()
# CMP0167 (CMake 3.30+) removes the bundled FindBoost module in favour of
# upstream BoostConfig.
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
endif()
# Beast-only Boost components: spawn (stackful coroutines) pulls in
# Boost.Coroutine, which in turn pulls in Boost.Context. Mongoose builds
# don't compile ServerBeastImpl.cpp at all, so requesting these would be
# unused work — and would force the package list (libboost-coroutine,
# libboost-context on Debian; analogous on RPM) on environments that
# only ever build the mongoose backend (typically Windows).
set(_nscp_extra_boost_components)
if(NSCP_WEB_BACKEND STREQUAL "beast")
    list(APPEND _nscp_extra_boost_components coroutine context)
endif()

# Boost.Python is requested as an OPTIONAL component: it is only consumed by
# the PythonScript module, and not every environment ships libboost-python.
# Listing it under COMPONENTS would drag a missing Boost.Python into
# Boost_FOUND=FALSE and fail the whole build (see the required-dependency check
# in the top-level CMakeLists). The PythonScript module checks
# Boost_<version>_FOUND itself before building.
find_package(
    Boost 1.75
    COMPONENTS
        filesystem
        thread
        regex
        date_time
        program_options
        chrono
        json
        container
        ${_nscp_extra_boost_components}
    OPTIONAL_COMPONENTS
        ${NSCP_BOOST_PYTHON_VERSION}
)
find_package(Mkdocs)
find_package(CSharp)

if(WIN32)
    include(${BUILD_CMAKE_FOLDER}/wix.cmake)
else(WIN32)
    # s       FIND_PACKAGE(Threads REQUIRED) FIND_PACKAGE(ICU REQUIRED)
    find_package(IConv)
endif(WIN32)
message(STATUS "Found dependencies:")
if(Python3_Interpreter_FOUND)
    message(STATUS " - python(exe) found: ${Python3_EXECUTABLE}")
else()
    message(STATUS " ! python(exe) not found: TODO")
endif()
if(Python3_Development_FOUND)
    message(STATUS " - python(lib) found: ${Python3_LIBRARIES}")
else()
    message(
        STATUS
        " - python(lib) not found (optional: PythonScript module disabled)"
    )
endif()
if(TINYXML2_FOUND)
    message(STATUS " - tinyXML found: ${TINYXML2_INCLUDE_DIR}")
else(TINYXML2_FOUND)
    message(
        STATUS
        " - tinyXML not found (optional: NRDPClient module disabled): TINY_XML2_SOURCE_DIR=${TINY_XML2_SOURCE_DIR}"
    )
endif(TINYXML2_FOUND)
if(CRYPTOPP_FOUND)
    message(
        STATUS
        " - crypto++(lib) found in: ${CRYPTOPP_INCLUDE_DIR} (${CRYPTOPP_LIBRARIES})"
    )
else(CRYPTOPP_FOUND)
    message(STATUS " ! crypto++ not found: CRYPTOPP_ROOT=${CRYPTOPP_ROOT}")
endif(CRYPTOPP_FOUND)
if(LUA_FOUND)
    message(STATUS " - lua found in ${LUA_INCLUDE_DIR}")
else(LUA_FOUND)
    if(LUA_SOURCE_FOUND)
        message(STATUS " - lua source found in ${LUA_SOURCE_DIR}")
    else()
        message(
            STATUS
            " - lua not found (optional: LUAScript + CheckMK modules disabled): LUA_INCLUDE_DIR=${LUA_INCLUDE_DIR} or LUA_SOURCE_DIR=${LUA_SOURCE_DIR}"
        )
    endif()
endif(LUA_FOUND)
if(PROTOC_GEN_LUA_FOUND)
    message(STATUS " - lua.protocol_buffers found in: ${PROTOC_GEN_LUA_BIN}")
else(PROTOC_GEN_LUA_FOUND)
    message(
        STATUS
        " ! lua.protocol_buffers not found: PROTOC_GEN_LUA=${PROTOC_GEN_LUA_BIN}"
    )
endif(PROTOC_GEN_LUA_FOUND)
if(PROTOC_GEN_MD_FOUND)
    message(STATUS " - md.protocol_buffers found in: ${PROTOC_GEN_MD_BIN}")
else(PROTOC_GEN_MD_FOUND)
    message(
        STATUS
        " ! md.protocol_buffers not found: PROTOC_GEN_MD_BIN=${PROTOC_GEN_MD_BIN}"
    )
endif(PROTOC_GEN_MD_FOUND)
if(PROTOBUF_FOUND)
    message(
        STATUS
        " - protocol buffers found in: ${PROTOBUF_INCLUDE_DIR} / ${PROTOBUF_LIBRARY}"
    )
    message(
        STATUS
        " - protocol buffers compiler: ${PROTOBUF_PROTOC_EXECUTABLE}"
    )
else(PROTOBUF_FOUND)
    message(
        STATUS
        " ! protocol buffers not found: PROTOBUF_ROOT=${PROTOBUF_ROOT}"
    )
endif(PROTOBUF_FOUND)
if(NOT NSCP_BUILD_TESTS)
    message(STATUS " - unit tests disabled (NSCP_BUILD_TESTS=OFF)")
elseif(GTest_FOUND)
    message(STATUS " - google test found (system)")
else()
    # Not fatal: the tests link the FetchContent-provided googletest, which is
    # downloaded regardless of whether a system Google Test exists.
    message(
        STATUS
        " - google test not found in system; using bundled (FetchContent) copy"
    )
endif()
if(OPENSSL_FOUND)
    message(
        STATUS
        " - OpenSSL found in: ${OPENSSL_INCLUDE_DIR} / ${OPENSSL_LIBRARIES}"
    )
else(OPENSSL_FOUND)
    # Not fatal: TLS-dependent features (NRPE/NSCA/check_mk over SSL, the
    # native NSCP protocol, HTTPS in WEBServer) are guarded on OPENSSL_FOUND in
    # their respective modules and simply drop out. The Beast web backend is
    # the one hard requirement and fails earlier above with a clear message.
    message(
        STATUS
        " ! OpenSSL not found (TLS features disabled): OPENSSL_INCLUDE_DIR=${OPENSSL_INCLUDE_DIR}"
    )
endif(OPENSSL_FOUND)
if(Boost_FOUND)
    message(
        STATUS
        " - boost found in: ${Boost_INCLUDE_DIRS} / ${Boost_LIBRARY_DIRS}"
    )
else(Boost_FOUND)
    message(STATUS " ! boost not found: BOOST_ROOT=${BOOST_ROOT}")
endif(Boost_FOUND)
if(CSHARP_FOUND)
    if(WIN32)
        message(STATUS " - CSharp found: ${CSHARP_TYPE} ${CSHARP_VERSION}")
    else()
        message(
            STATUS
            " - CSharp found: ${CSHARP_TYPE} ${CSHARP_VERSION} (but disabled sine it is not currently supported on non windows"
        )
    endif()
else()
    message(STATUS " ! CSharp not found")
endif()
# Report the resolved ZIP backend (computed above). libzip may be located
# either via its CMake config package (imported target libzip::zip) or via
# pkg-config (LIBZIP_INCLUDE_DIRS/LIBZIP_LIBRARIES, no target) — both count as
# a working libzip backend.
if(NSCP_ZIP_BACKEND STREQUAL "miniz")
    message(STATUS " - ZIP backend: miniz (${MINIZ_INCLUDE_DIR})")
elseif(NSCP_ZIP_BACKEND STREQUAL "libzip")
    if(TARGET libzip::zip)
        message(
            STATUS
            " - ZIP backend: libzip (CMake config package): ${LIBZIP_INCLUDE_DIRS}"
        )
    else()
        message(
            STATUS
            " - ZIP backend: libzip (pkg-config): ${LIBZIP_INCLUDE_DIRS}"
        )
    endif()
else()
    # Not fatal: the libs/minizip wrapper builds a stub (NSCP_NO_ZIP) and ZIP
    # archive reading is disabled. Install libzip-dev (Debian) / libzip-devel
    # (RPM) to enable it.
    message(
        STATUS
        " - ZIP backend: none (zip archive support disabled; install libzip-dev/libzip-devel to enable)"
    )
endif()
if(MKDOCS_FOUND)
    message(STATUS " - MKDocs found in: ${MKDOCS_EXECUTABLE}")
else(MKDOCS_FOUND)
    message(STATUS " ! MKDocs not found: MKDOCS_DIR=${MKDOCS_DIR}")
endif(MKDOCS_FOUND)
if(NSCP_WEB_BACKEND STREQUAL "mongoose")
    if(MONGOOSE_FOUND)
        message(STATUS " - Mongoose found in: ${MONGOOSE_INCLUDE_DIR}")
    else(MONGOOSE_FOUND)
        message(
            STATUS
            " ! Mongoose not found: MONGOOSE_SOURCE_DIR=${MONGOOSE_SOURCE_DIR}"
        )
    endif(MONGOOSE_FOUND)
endif()

if(WIN32)
    if(WIX_FOUND)
        message(STATUS " - wix found in: ${WIX_ROOT_DIR}")
    else(WIX_FOUND)
        message(STATUS " ! wix not found: WIX_ROOT_DIR=${WIX_ROOT_DIR}")
    endif(WIX_FOUND)
endif(WIN32)
if(NOT WIN32)
    # IF(CMAKE_USE_PTHREADS_INIT) MESSAGE(STATUS " - POSIX threads found: TODO")
    # ELSE(CMAKE_USE_PTHREADS_INIT) MESSAGE(STATUS " ! POSIX threads not found:
    # TODO") ENDIF(CMAKE_USE_PTHREADS_INIT) IF(NOT ICU_FOUND) MESSAGE(STATUS "ICU
    # package not found.") ELSE(NOT ICU_FOUND) ADD_DEFINITIONS( -DSI_CONVERT_ICU )
    # ENDIF(NOT ICU_FOUND)
    if(ICONV_FOUND)
        message(STATUS " - IConv found in: ${ICONV_INCLUDE_DIR}")
    else(ICONV_FOUND)
        message(STATUS " ! IConv package not found.")
    endif(ICONV_FOUND)
    # ICONV_INCLUDE_DIR
endif(NOT WIN32)
