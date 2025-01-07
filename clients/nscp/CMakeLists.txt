cmake_minimum_required(VERSION 3.5)

set(TARGET check_nscp)

project(clients)

set(SRCS
    ${SRCS} check_nscp.cpp ${NSCP_INCLUDEDIR}/socket/socket_helpers.cpp
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_settings_helper.cpp ${NSCP_CLIENT_CPP}
    ../../resources/nsclient.ico)

add_definitions(${NSCP_GLOBAL_DEFINES})
if(OPENSSL_FOUND)
  add_definitions(-DUSE_SSL)
  set(EXTRA_LIBS ${EXTRA_LIBS} ${OPENSSL_LIBRARIES})
  if(WIN32)
    set(EXTRA_LIBS ${EXTRA_LIBS} CRYPT32)
  endif(WIN32)
  include_directories(${OPENSSL_INCLUDE_DIR})
endif(OPENSSL_FOUND)

if(WIN32)
  set(SRCS
      ${SRCS}
      check_nscp.hpp
      ../../modules/NSCPClient/nscp_client.hpp
      ../../modules/NSCPClient/nscp_handler.hpp
      ${NSCP_INCLUDEDIR}/nscapi/nscapi_settings_helper.hpp
      ${NSCP_INCLUDEDIR}/socket/clients/http/http_client_protocol.hpp
      ${NSCP_INCLUDEDIR}/socket/clients/http/http_packet.hpp
      ${NSCP_INCLUDEDIR}/socket/socket_helpers.hpp
      ${NSCP_INCLUDEDIR}/socket/client.hpp
      ${NSCP_INCLUDEDIR}/http/client.hpp
      ${NSCP_CLIENT_HPP})
endif(WIN32)

include_directories(${BOOST_INCLUDE_DIRS})
if(JSON_SPIRIT_FOUND)
  include_directories(${JSON_SPIRIT_INCLUDE_DIR})
  add_definitions(-DHAVE_JSON_SPIRIT)
  set(JSON_LIB json_spirit)
else(JSON_SPIRIT_FOUND)
  set(JSON_LIB)
endif(JSON_SPIRIT_FOUND)
link_directories(${BOOST_LIB_DIRS})

if(BREAKPAD_FOUND)
  if(WIN32)
    set(${_SRC} ${${_SRC}}
                ${NSCP_INCLUDEDIR}/breakpad/exception_handler_win32.cpp)
    set(${_SRC} ${${_SRC}}
                ${NSCP_INCLUDEDIR}/breakpad/exception_handler_win32.hpp)
  endif(WIN32)
  add_definitions(-DUSE_BREAKPAD)
  include_directories(${BREAKPAD_INCLUDE_DIR})
  set(EXTRA_LIBS ${EXTRA_LIBS} ${BREAKPAD_COMMON_LIBRARY} ${BREAKPAD_LIBRARY})
endif(BREAKPAD_FOUND)

nscp_make_exe_bin(${TARGET} "${SRCS}")
if(WIN32 AND NOT CMAKE_CL_64)
  openssl_link_fix(${TARGET})
endif(WIN32 AND NOT CMAKE_CL_64)

target_link_libraries(
  ${TARGET} ${Boost_FILESYSTEM_LIBRARY} ${Boost_PROGRAM_OPTIONS_LIBRARY}
  ${Boost_THREAD_LIBRARY} ${NSCP_DEF_PLUGIN_LIB} ${EXTRA_LIBS})
set_target_properties(${TARGET} PROPERTIES FOLDER "clients")

source_group("Client" REGULAR_EXPRESSION .*include/socket/clients/http/.*)
source_group("Socket" REGULAR_EXPRESSION .*include/socket/.*)
source_group("NSCP API" REGULAR_EXPRESSION .*include/nscapi/.*)
source_group("Settings" REGULAR_EXPRESSION .*include/settings/.*)
source_group("Common Files" REGULAR_EXPRESSION .*include/.*)
