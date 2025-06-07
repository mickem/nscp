cmake_minimum_required(VERSION 3.10)

set(TARGET where_filter)

include_directories(${BOOST_INCLUDE_DIRS})
link_directories(${BOOST_LIB_DIRS})
add_definitions(${NSCP_GLOBAL_DEFINES})

set(SRCS
    ${NSCP_INCLUDEDIR}/parsers/where.cpp
    ${NSCP_INCLUDEDIR}/parsers/operators.cpp
    ${NSCP_INCLUDEDIR}/parsers/helpers.cpp
    ${NSCP_INCLUDEDIR}/utf8.cpp
    ${NSCP_INCLUDEDIR}/parsers/where/binary_op.cpp
    ${NSCP_INCLUDEDIR}/parsers/where/helpers.cpp
    ${NSCP_INCLUDEDIR}/parsers/where/list_node.cpp
    ${NSCP_INCLUDEDIR}/parsers/where/node.cpp
    ${NSCP_INCLUDEDIR}/parsers/where/unary_fun.cpp
    ${NSCP_INCLUDEDIR}/parsers/where/unary_op.cpp
    ${NSCP_INCLUDEDIR}/parsers/where/value_node.cpp
    ${NSCP_INCLUDEDIR}/parsers/where/variable.cpp
    ${NSCP_INCLUDEDIR}/parsers/where/engine.cpp
    ${NSCP_INCLUDEDIR}/parsers/where/grammar/grammar.cpp)

if(WIN32)
  set(SRCS
      ${SRCS}
      ${NSCP_INCLUDEDIR}/parsers/operators.hpp
      ${NSCP_INCLUDEDIR}/parsers/helpers.hpp
      ${NSCP_INCLUDEDIR}/parsers/where.hpp
      ${NSCP_INCLUDEDIR}/utf8.hpp
      ${NSCP_INCLUDEDIR}/parsers/where/binary_op.hpp
      ${NSCP_INCLUDEDIR}/parsers/where/helpers.hpp
      ${NSCP_INCLUDEDIR}/parsers/where/list_node.hpp
      ${NSCP_INCLUDEDIR}/parsers/where/node.hpp
      ${NSCP_INCLUDEDIR}/parsers/where/unary_fun.hpp
      ${NSCP_INCLUDEDIR}/parsers/where/unary_op.hpp
      ${NSCP_INCLUDEDIR}/parsers/where/value_node.hpp
      ${NSCP_INCLUDEDIR}/parsers/where/variable.hpp
      ${NSCP_INCLUDEDIR}/parsers/where/filter_handler_impl.hpp
      ${NSCP_INCLUDEDIR}/parsers/where/grammar/grammar.hpp
      ${NSCP_INCLUDEDIR}/parsers/where/engine.hpp
      ${NSCP_INCLUDEDIR}/parsers/where/engine_impl.hpp)
endif(WIN32)

set(EXTRA_LIBS)
if(APPLE)
  set(EXTRA_LIBS ${ICONV_LIBRARIES})
endif()

nscp_make_library(${TARGET} "${SRCS}")

target_link_libraries(
  ${TARGET} ${Boost_SYSTEM_LIBRARY} ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_PROGRAM_OPTIONS_LIBRARY} ${Boost_REGEX_LIBRARY} ${EXTRA_LIBS})
