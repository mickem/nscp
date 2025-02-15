# * Find google test include folder and libraries This module finds tinyxml2 if
#   it is installed and determines where the files are. This code sets the
#   following variables:
#
# GTEST_FOUND             - have google test been found GTEST_INCLUDE_DIR - path
# to where tinyxml2.h is found
#

if(CMAKE_TRACE)
  message(STATUS "GTEST_INCLUDE_DIR=${GTEST_INCLUDE_DIR}")
  message(STATUS "GTEST_SRC_DIR=${GTEST_SRC_DIR}")
  message(STATUS "GTEST_ROOT=${GTEST_ROOT}")
endif(CMAKE_TRACE)

find_path(
  GTEST_INCLUDE_DIR
  NAMES gtest/gtest.h
  PATHS ${GTEST_INCLUDE_DIR} /usr/include)
find_path(
  GTEST_SRC_DIR
  NAMES src/gtest-all.cc
  PATHS ${GTEST_SRC_DIR} /usr/src/gtest)
if(NOT GTEST_INCLUDE_DIR)
  find_path(
    GTEST_INCLUDE_DIR
    NAMES gtest/gtest.h
    PATHS ${GTEST_ROOT}/include ${CMAKE_SOURCE_DIR}/ext/gtest/include
    NO_DEFAULT_PATH)
  find_path(
    GTEST_SRC_DIR
    NAMES src/gtest-all.cc
    PATHS ${GTEST_ROOT} ${CMAKE_SOURCE_DIR}/ext/gtest
    NO_DEFAULT_PATH)
endif(NOT GTEST_INCLUDE_DIR)

if(CMAKE_TRACE)
  message(STATUS "GTEST_INCLUDE_DIR=${GTEST_INCLUDE_DIR}")
  message(STATUS "GTEST_SRC_DIR=${GTEST_SRC_DIR}")
endif(CMAKE_TRACE)

set(GTEST_FIND_COMPONENTS gtest gtest_main)

if(GTEST_INCLUDE_DIR)
  set(GTEST_FOUND TRUE)
  foreach(COMPONENT ${GTEST_FIND_COMPONENTS})
    string(TOUPPER ${COMPONENT} UPPERCOMPONENT)
    find_library(
      GTEST_${UPPERCOMPONENT}_LIBRARY_RELEASE
      NAMES ${COMPONENT}
      PATHS ${GTEST_ROOT}/release ${GNUWIN32_DIR}/lib /usr/lib)
    find_library(
      GTEST_${UPPERCOMPONENT}_LIBRARY_DEBUG
      NAMES ${COMPONENT}
      PATHS ${GTEST_ROOT}/debug ${GNUWIN32_DIR}/lib /usr/lib)
    if(GTEST_${UPPERCOMPONENT}_LIBRARY_RELEASE
       AND GTEST_${UPPERCOMPONENT}_LIBRARY_DEBUG)
      set(GTEST_${UPPERCOMPONENT}_FOUND TRUE)
      set(GTEST_${UPPERCOMPONENT}_LIBRARY
          optimized ${GTEST_${UPPERCOMPONENT}_LIBRARY_RELEASE} debug
          ${GTEST_${UPPERCOMPONENT}_LIBRARY_DEBUG})
      set(GTEST_${UPPERCOMPONENT}_LIBRARY
          ${GTEST_${UPPERCOMPONENT}_LIBRARY}
          CACHE FILEPATH "The google test ${UPPERCOMPONENT} library")
    else(GTEST_${UPPERCOMPONENT}_LIBRARY_RELEASE
         AND GTEST_${UPPERCOMPONENT}_LIBRARY_DEBUG)
      if(CMAKE_TRACE)
        message(STATUS "Failed to find: ${COMPONENT}")
      endif(CMAKE_TRACE)
      set(GTEST_${UPPERCOMPONENT}_FOUND FALSE)
      set(GTEST_${UPPERCOMPONENT}_LIBRARY
          "${GTEST_${UPPERCOMPONENT}_LIBRARY-NOTFOUND}")
    endif(GTEST_${UPPERCOMPONENT}_LIBRARY_RELEASE
          AND GTEST_${UPPERCOMPONENT}_LIBRARY_DEBUG)
    if(CMAKE_TRACE)
      message(
        STATUS
          "${COMPONENT}: GTEST_${UPPERCOMPONENT}_LIBRARY=${GTEST_${UPPERCOMPONENT}_LIBRARY}"
      )
    endif(CMAKE_TRACE)
  endforeach(COMPONENT)
  if(NOT GTEST_GTEST_FOUND OR NOT GTEST_GTEST_MAIN_FOUND)
    if(NOT GTEST_SRC_DIR)
      if(CMAKE_TRACE)
        message(
          STATUS
            "No modules found and no source: ${GTEST_GTEST_FOUND}/${GTEST_GTEST_MAIN_FOUND}/${GTEST_SRC_DIR}"
        )
      endif(CMAKE_TRACE)
      set(GTEST_FOUND FALSE)
    endif(NOT GTEST_SRC_DIR)
  endif(NOT GTEST_GTEST_FOUND OR NOT GTEST_GTEST_MAIN_FOUND)
endif(GTEST_INCLUDE_DIR)
