cmake_minimum_required(VERSION 3.5)

set(TARGET settings_manager)

include_directories(${BOOST_INCLUDE_DIRS})
link_directories(${BOOST_LIB_DIRS})

set(EXTRA_LIBS)

set(service_SRCS
    settings_manager_impl.cpp
    settings_handler_impl.cpp
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp
    # ${NSCP_INCLUDEDIR}/settings/client/settings_client.cpp
    ${NSCP_INCLUDEDIR}/simpleini/ConvertUTF.c
    ${NSCP_INCLUDEDIR}/utf8.cpp)

if(WIN32)
  set(service_SRCS
      ${service_SRCS}
      settings_manager_impl.h
      settings_handler_impl.hpp
      # ${NSCP_INCLUDEDIR}/config.h
      ${NSCP_INCLUDEDIR}/settings/settings_core.hpp
      ${NSCP_INCLUDEDIR}/settings/settings_interface_impl.hpp
      ${NSCP_INCLUDEDIR}/settings/impl/settings_ini.hpp
      ${NSCP_INCLUDEDIR}/settings/impl/settings_dummy.hpp
      ${NSCP_INCLUDEDIR}/settings/impl/settings_old.hpp
      ${NSCP_INCLUDEDIR}/settings/impl/settings_registry.hpp
      ${NSCP_INCLUDEDIR}/settings/impl/settings_http.hpp
      ${NSCP_INCLUDEDIR}/http/client.hpp
      ${NSCP_INCLUDEDIR}/utf8.hpp)
endif(WIN32)

add_definitions(${NSCP_GLOBAL_DEFINES})
if(OPENSSL_FOUND)
  add_definitions(-DUSE_SSL)
  set(EXTRA_LIBS ${EXTRA_LIBS} ${OPENSSL_LIBRARIES})
  if(WIN32)
    set(EXTRA_LIBS ${EXTRA_LIBS} CRYPT32)
  endif(WIN32)
  include_directories(${OPENSSL_INCLUDE_DIR})
endif(OPENSSL_FOUND)

if(CRYPTOPP_FOUND)
  add_definitions(-DHAVE_LIBCRYPTOPP)
  include_directories(${CRYPTOPP_INCLUDE_DIR})
  set(EXTRA_LIBS ${EXTRA_LIBS} ${CRYPTOPP_LIBRARIES})
endif(CRYPTOPP_FOUND)

add_library(${TARGET} STATIC ${service_SRCS})

target_link_libraries(${TARGET} ${EXTRA_LIBS} nscp_protobuf)

set_target_properties(${TARGET} PROPERTIES FOLDER "core")

source_group("Common Files" REGULAR_EXPRESSION .*include/.*)
source_group("Settings" REGULAR_EXPRESSION .*include/settings/.*)
