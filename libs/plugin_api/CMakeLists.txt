cmake_minimum_required(VERSION 3.10)

set(TARGET plugin_api)

include_directories(${BOOST_INCLUDE_DIRS})
link_directories(${BOOST_LIB_DIRS})

set(SRCS
    ${NSCP_INCLUDEDIR}/utf8.cpp
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_core_wrapper.cpp
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_core_helper.cpp
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper_singleton.cpp
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_protobuf_functions.cpp
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_protobuf_settings_functions.cpp
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_targets.cpp
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_settings_filter.cpp
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_settings_proxy.cpp
    ${NSCP_INCLUDEDIR}/nscapi/command_client.cpp
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_settings_object.cpp
    ${NSCP_INCLUDEDIR}/nscapi/nscapi_settings_helper.cpp)

if(WIN32)
  set(SRCS
      ${SRCS}
      ${NSCP_INCLUDEDIR}/utf8.hpp
      # ${NSCP_INCLUDEDIR}/config.h
      ${NSCP_INCLUDEDIR}/NSCAPI.h
      ${NSCP_INCLUDEDIR}/nscapi/nscapi_core_wrapper.hpp
      ${NSCP_INCLUDEDIR}/nscapi/nscapi_core_helper.hpp
      ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.hpp
      ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper_singleton.hpp
      ${NSCP_INCLUDEDIR}/nscapi/nscapi_protobuf_functions.hpp
      ${NSCP_INCLUDEDIR}/nscapi/nscapi_protobuf_settings_functions.hpp
      ${NSCP_INCLUDEDIR}/nscapi/nscapi_targets.hpp
      ${NSCP_INCLUDEDIR}/nscapi/nscapi_settings_filter.hpp
      ${NSCP_INCLUDEDIR}/nscapi/nscapi_settings_proxy.hpp
      ${NSCP_INCLUDEDIR}/nscapi/macros.hpp
      ${NSCP_INCLUDEDIR}/nscapi/nscapi_settings_object.hpp
      ${NSCP_INCLUDEDIR}/nscapi/nscapi_settings_helper.hpp)
endif(WIN32)

set(EXTRA_LIBS)
if(APPLE)
  set(EXTRA_LIBS ${ICONV_LIBRARIES})
endif()

add_definitions(${NSCP_GLOBAL_DEFINES})
nscp_make_library(${TARGET} "${SRCS}")
nscp_force_include(
  ${TARGET} "${BUILD_ROOT_FOLDER}/include/nscapi/dll_defines_protobuf.hpp")

set_target_properties(${TARGET} PROPERTIES FOLDER "libraries")
target_link_libraries(
  ${TARGET} ${Boost_SYSTEM_LIBRARY} ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_PROGRAM_OPTIONS_LIBRARY} nscp_protobuf ${EXTRA_LIBS})
add_dependencies(${TARGET} nscp_protobuf)

if(GTEST_FOUND)
  include_directories(${GTEST_INCLUDE_DIR})
  set(TEST_SRCS
      nscapi_helper_test.cpp ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp
      ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.hpp)
  nscp_make_exe_test(${TARGET}_test "${TEST_SRCS}")
  nscp_add_test(${TARGET}_test ${TARGET}_test)
  target_link_libraries(
    ${TARGET}_test ${GTEST_GTEST_LIBRARY} ${GTEST_GTEST_MAIN_LIBRARY}
    ${NSCP_DEF_PLUGIN_LIB} ${EXTRA_LIBS} ${Boost_DATE_TIME_LIBRARY})
endif(GTEST_FOUND)
