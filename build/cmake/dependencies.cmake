cmake_minimum_required(VERSION 3.10)

# ##############################################################################
#
# Find all dependencies and report anything missing.
#
# ##############################################################################
message(STATUS "Looking for dependencies:")
find_package(
    Python3
    COMPONENTS
        Interpreter
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
find_package(GTest)
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
# HTTP backend selector for WEBServer. The mongoose default keeps the
# Windows packaging story unchanged; setting `beast` switches every
# platform to the Boost.Beast implementation (libs/mongoose-cpp/
# ServerBeastImpl.cpp) and drops the vendored mongoose download from
# the build. See docs/design/beast-web-backend.md.
set(NSCP_WEB_BACKEND
    "mongoose"
    CACHE STRING
    "HTTP backend for WEBServer: mongoose | beast"
)
set_property(CACHE NSCP_WEB_BACKEND PROPERTY STRINGS mongoose beast)

if(NSCP_WEB_BACKEND STREQUAL "mongoose")
    find_package(Mongoose)
elseif(NSCP_WEB_BACKEND STREQUAL "beast")
    # Beast is header-only; the Boost components (coroutine + context)
    # needed by ServerBeastImpl are added below.
    message(STATUS "WEB backend: Boost.Beast (mongoose download skipped)")
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

find_package(
    Boost
    COMPONENTS
        system
        filesystem
        thread
        regex
        date_time
        program_options
        ${NSCP_BOOST_PYTHON_VERSION}
        chrono
        json
        container
        ${_nscp_extra_boost_components}
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
    message(STATUS " ! python(lib) not found: TODO")
endif()
if(TINYXML2_FOUND)
    message(STATUS " - tinyXML found: ${TINYXML2_INCLUDE_DIR}")
else(TINYXML2_FOUND)
    message(
        STATUS
        " ! tinyXML not found: TINY_XML2_SOURCE_DIR=${TINY_XML2_SOURCE_DIR}"
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
            " ! lua not found: LUA_INCLUDE_DIR=${LUA_INCLUDE_DIR} or LUA_SOURCE_DIR=${LUA_SOURCE_DIR}"
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
if(GTest_FOUND)
    message(STATUS " - google test found")
else(GTest_FOUND)
    message(FATAL_ERROR " ! google test not found: GTEST_ROOT=${GTEST_ROOT}")
endif(GTest_FOUND)
if(OPENSSL_FOUND)
    message(
        STATUS
        " - OpenSSL found in: ${OPENSSL_INCLUDE_DIR} / ${OPENSSL_LIBRARIES}"
    )
else(OPENSSL_FOUND)
    message(
        FATAL_ERROR
        " ! OpenSSL not found OPENSSL_INCLUDE_DIR=${OPENSSL_INCLUDE_DIR}"
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
if(MINIZ_FOUND)
    message(STATUS " - Miniz found in: ${MINIZ_INCLUDE_DIR}")
else()
    if(LIBZIP_FOUND AND TARGET libzip::zip)
        message(
            STATUS
            " - libzip found (CMake config package): ${LIBZIP_INCLUDE_DIRS}"
        )
    else()
        message(
            STATUS
            " ! Neither miniz (MINIZ_INCLUDE_DIR=${MINIZ_INCLUDE_DIR}) nor libzip (install libzip-dev (Debian) or libzip-devel (RPM)) was found"
        )
    endif()
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
