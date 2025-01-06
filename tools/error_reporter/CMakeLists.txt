cmake_minimum_required(VERSION 3.5)

set(TARGET reporter)

project(reporter)

add_definitions(${NSCP_GLOBAL_DEFINES})

set(reporter_SRCS reporter.cpp ${NSCP_INCLUDEDIR}/utf8.cpp)

if(WIN32)
  set(reporter_SRCS
      ${reporter_SRCS}
      ${NSCP_INCLUDEDIR}/ServiceCmd.cpp
      reporter.h
      reporter.rc
      ${NSCP_INCLUDEDIR}/utf8.hpp
      ${NSCP_ERROR_CPP}
      ${NSCP_ERROR_HPP})
  set(EXTRA_LIBS Wininet.lib)
endif(WIN32)

if(BREAKPAD_FOUND)
  add_definitions(-DUSE_BREAKPAD)
  include_directories(${BREAKPAD_INCLUDE_DIR})
  set(EXTRA_LIBS ${EXTRA_LIBS} ${BREAKPAD_COMMON_LIBRARY} ${BREAKPAD_LIBRARY})
endif(BREAKPAD_FOUND)

nscp_make_exe_sbin(${TARGET} "${reporter_SRCS}")

target_link_libraries(${TARGET} ${Boost_FILESYSTEM_LIBRARY}
                      ${Boost_SYSTEM_LIBRARY} ${EXTRA_LIBS})
set_target_properties(${TARGET} PROPERTIES FOLDER "tools")

source_group("Common Files" REGULAR_EXPRESSION .*include/.*)
source_group("Parser" REGULAR_EXPRESSION .*include/parser/.*)
source_group("NSCP API" REGULAR_EXPRESSION .*include/nscapi/.*)
source_group("Settings" REGULAR_EXPRESSION .*include/settings/.*)
