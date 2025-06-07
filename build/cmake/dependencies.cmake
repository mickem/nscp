cmake_minimum_required(VERSION 3.5)

# ##############################################################################
#
# Find all dependencies and report anything missing.
#
# ##############################################################################
message(STATUS "Looking for dependencies:")
find_package(Python3 COMPONENTS Interpreter Development)
find_package(TinyXML2)
find_package(CryptoPP)
find_package(Lua)
if(NOT LUA_FOUND)
  find_package(LUASource)
endif()

find_package(PROTOC_GEN_LUA)
find_package(ProtocGenMd)
find_package(ProtoBuf)
find_package(GoogleTest)
find_package(OpenSSL)
find_package(Miniz)
if(WIN32)
  set(boost_python_dep python311)
else(WIN32)
  set(boost_python_dep python312)
endif(WIN32)
find_package(
  Boost
  COMPONENTS system
             filesystem
             thread
             regex
             date_time
             program_options
             ${boost_python_dep}
             chrono
             json)
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
  message(STATUS " ! tinyXML not found: TODO")
endif(TINYXML2_FOUND)
if(CRYPTOPP_FOUND)
  message(
    STATUS
      " - crypto++(lib) found in: ${CRYPTOPP_INCLUDE_DIR} (${CRYPTOPP_LIBRARIES})"
  )
else(CRYPTOPP_FOUND)
  message(STATUS " ! crypto++ not found: ${CRYPTOPP_ROOT}")
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
      " ! lua.protocol_buffers not found: PROTOC_GEN_LUA=${PROTOC_GEN_LUA_BIN}")
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
  message(STATUS " - protocol buffers compiler: ${PROTOBUF_PROTOC_EXECUTABLE}")
else(PROTOBUF_FOUND)
  message(
    STATUS " ! protocol buffers not found: PROTOBUF_ROOT=${PROTOBUF_ROOT}")
endif(PROTOBUF_FOUND)
if(GTEST_FOUND)
  message(STATUS " - google test found in: ${GTEST_INCLUDE_DIR}")
else(GTEST_FOUND)
  message(STATUS " ! google test not found: GTEST_ROOT=${GTEST_ROOT}")
endif(GTEST_FOUND)
if(OPENSSL_FOUND)
  message(
    STATUS " - OpenSSL found in: ${OPENSSL_INCLUDE_DIR} / ${OPENSSL_LIBRARIES}")
else(OPENSSL_FOUND)
  message(STATUS " ! OpenSSL not found TODO=${OPENSSL_INCLUDE_DIR}")
endif(OPENSSL_FOUND)
if(Boost_FOUND)
  message(
    STATUS " - boost found in: ${Boost_INCLUDE_DIRS} / ${Boost_LIBRARY_DIRS}")
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
else(MINIZ_FOUND)
  message(STATUS " ! Miniz not found: MINIZ_INCLUDE_DIR=${MINIZ_INCLUDE_DIR}")
endif(MINIZ_FOUND)
if(MKDOCS_FOUND)
  message(STATUS " - MKDocs found in: ${MKDOCS_EXECUTABLE}")
else(MKDOCS_FOUND)
  message(STATUS " ! MKDocs not found: MKDOCS_DIR=${MKDOCS_DIR}")
endif(MKDOCS_FOUND)

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
